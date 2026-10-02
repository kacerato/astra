"""Vetoriza a marca ASTRA (quadrado limão + estrela, órbita e ponto) a partir do original em PNG.

Doc 17 §2: a marca precisa existir em vetor fiel, conferida por diferença de pixels contra o original.
- Quadrado: dois retângulos arredondados geométricos (borda preta + miolo limão) ajustados ao original;
  o PNG tem um entalhe na borda direita (artefato) que não é copiado.
- Desenho interno: contornos da "tinta preta" por marching squares (nível 0,5 do campo antisserrilhado),
  simplificados (Douglas-Peucker) e suavizados em Béziers cúbicas (Catmull-Rom).

Uso: python trace_mark.py <mark.png> <saida.svg> [previa.png]
Imprime a diferença: fração de pixels que mudam de classe (preto/limão/fundo) entre o vetor e o original.
"""

import math
import sys

import numpy as np
from PIL import Image, ImageDraw

LIME = np.array([202, 251, 4], dtype=np.float32)


def load(path):
    im = Image.open(path).convert("RGBA")
    a = np.asarray(im).astype(np.float32)
    rgb, alpha = a[..., :3], a[..., 3] / 255.0
    # "Limão-idade" por projeção na cor da marca (preto = 0, limão = 1), só onde há pixel opaco.
    lime = np.clip((rgb @ LIME) / float(LIME @ LIME), 0.0, 1.0)
    # Fundo claro (branco do PNG) conta como fundo, não como limão.
    whiteish = (rgb.min(axis=2) > 200) & (alpha > 0.5)
    alpha = np.where(whiteish, 0.0, alpha)
    return im.size, alpha, lime


def bbox(mask):
    ys, xs = np.nonzero(mask)
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1


def corner_radius(mask, box):
    """Raio pelo recuo do canto superior esquerdo ao longo da diagonal (círculo inscrito no canto)."""
    x0, y0, x1, y1 = [int(round(v)) for v in box]
    for d in range(0, min(x1 - x0, y1 - y0) // 2):
        if 0 <= y0 + d < mask.shape[0] and 0 <= x0 + d < mask.shape[1] and mask[y0 + d, x0 + d]:
            # Para um canto circular de raio r, a diagonal entra em r(1 - 1/sqrt2).
            return d / (1.0 - 1.0 / math.sqrt(2.0))
    return 0.0


def marching_squares(field, level):
    """Contornos fechados da isolinha `level` (campo h x w). Devolve listas de pontos (x, y) em pixels."""
    h, w = field.shape
    f = np.pad(field, 1, constant_values=0.0)  # garante contornos fechados
    segs = {}

    def interp(p, q, vp, vq):
        t = (level - vp) / (vq - vp) if vq != vp else 0.5
        return (p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1]))

    edges = []
    coord = {}
    for y in range(h + 1):
        for x in range(w + 1):
            v = [f[y, x], f[y, x + 1], f[y + 1, x + 1], f[y + 1, x]]
            idx = sum(1 << i for i, val in enumerate(v) if val >= level)
            if idx in (0, 15):
                continue
            c = [(x, y), (x + 1, y), (x + 1, y + 1), (x, y + 1)]
            # Cada ponto é identificado pela aresta da grade onde está (chave inteira), não pela coordenada:
            # duas células vizinhas calculam o mesmo ponto com arredondamento diferente.
            keys = {0: ("h", x, y), 1: ("v", x + 1, y), 2: ("h", x, y + 1), 3: ("v", x, y)}
            e = {
                0: interp(c[0], c[1], v[0], v[1]), 1: interp(c[1], c[2], v[1], v[2]),
                2: interp(c[2], c[3], v[2], v[3]), 3: interp(c[3], c[0], v[3], v[0]),
            }
            table = {
                1: [(3, 0)], 2: [(0, 1)], 3: [(3, 1)], 4: [(1, 2)], 5: [(3, 0), (1, 2)], 6: [(0, 2)], 7: [(3, 2)],
                8: [(2, 3)], 9: [(2, 0)], 10: [(0, 1), (2, 3)], 11: [(2, 1)], 12: [(1, 3)], 13: [(1, 0)], 14: [(0, 3)],
            }
            for a, b in table[idx]:
                coord[keys[a]] = (e[a][0] - 1, e[a][1] - 1)
                coord[keys[b]] = (e[b][0] - 1, e[b][1] - 1)
                edges.append((keys[a], keys[b]))
    # Encadeia como grafo não orientado (a tabela acima não garante orientação consistente nos segmentos).
    adj = {}
    for a, b in edges:
        if a == b:
            continue
        adj.setdefault(a, []).append(b)
        adj.setdefault(b, []).append(a)
    contours = []
    while adj:
        start = next(iter(adj))
        if not adj[start]:
            del adj[start]
            continue
        path = [start]
        prev, cur = None, start
        while True:
            nbrs = adj.get(cur, [])
            nxt = next((q for q in nbrs if q != prev), None) if prev is not None else (nbrs[0] if nbrs else None)
            if nxt is None:
                break
            adj[cur].remove(nxt)
            adj[nxt].remove(cur)
            if not adj[cur]:
                del adj[cur]
            if nxt == start:
                if not adj.get(start):
                    adj.pop(start, None)
                break
            path.append(nxt)
            prev, cur = cur, nxt
        if len(path) > 8:
            contours.append([coord[k] for k in path])
    return contours


def douglas_peucker(pts, eps):
    if len(pts) < 3:
        return pts
    a, b = np.array(pts[0]), np.array(pts[-1])
    ab = b - a
    n = np.linalg.norm(ab)
    arr = np.array(pts)
    if n == 0:
        d = np.linalg.norm(arr - a, axis=1)
    else:
        d = np.abs(ab[0] * (arr[:, 1] - a[1]) - ab[1] * (arr[:, 0] - a[0])) / n
    i = int(np.argmax(d))
    if d[i] > eps:
        return douglas_peucker(pts[: i + 1], eps)[:-1] + douglas_peucker(pts[i:], eps)
    return [pts[0], pts[-1]]


def simplify_closed(pts, eps):
    # Divide no ponto mais distante do primeiro para simplificar um laço fechado.
    arr = np.array(pts)
    far = int(np.argmax(np.linalg.norm(arr - arr[0], axis=1)))
    return douglas_peucker(pts[: far + 1], eps)[:-1] + douglas_peucker(pts[far:] + [pts[0]], eps)[:-1]


def to_bezier_path(pts, sharp_deg=50.0):
    """Catmull-Rom -> Bézier cúbicas; mantém quinas (pontas da estrela) onde o ângulo é agudo."""
    n = len(pts)
    P = [np.array(p) for p in pts]

    def is_sharp(i):
        a, b, c = P[i - 1], P[i], P[(i + 1) % n]
        u, v = a - b, c - b
        cos = np.dot(u, v) / (np.linalg.norm(u) * np.linalg.norm(v) + 1e-9)
        return math.degrees(math.acos(max(-1.0, min(1.0, cos)))) < (180.0 - sharp_deg) and \
            math.degrees(math.acos(max(-1.0, min(1.0, cos)))) < 100.0

    d = f"M{P[0][0]:.2f},{P[0][1]:.2f}"
    for i in range(n):
        p0, p1, p2, p3 = P[i - 1], P[i], P[(i + 1) % n], P[(i + 2) % n]
        c1 = p1 if is_sharp(i) else p1 + (p2 - p0) / 6.0
        c2 = p2 if is_sharp((i + 1) % n) else p2 - (p3 - p1) / 6.0
        d += f" C{c1[0]:.2f},{c1[1]:.2f} {c2[0]:.2f},{c2[1]:.2f} {p2[0]:.2f},{p2[1]:.2f}"
    return d + " Z"


def rounded_rect_path(x0, y0, x1, y1, r):
    k = 0.5522847498 * r
    return (f"M{x0 + r:.2f},{y0:.2f} H{x1 - r:.2f} C{x1 - r + k:.2f},{y0:.2f} {x1:.2f},{y0 + r - k:.2f} {x1:.2f},{y0 + r:.2f} "
            f"V{y1 - r:.2f} C{x1:.2f},{y1 - r + k:.2f} {x1 - r + k:.2f},{y1:.2f} {x1 - r:.2f},{y1:.2f} H{x0 + r:.2f} "
            f"C{x0 + r - k:.2f},{y1:.2f} {x0:.2f},{y1 - r + k:.2f} {x0:.2f},{y1 - r:.2f} V{y0 + r:.2f} "
            f"C{x0:.2f},{y0 + r - k:.2f} {x0 + r - k:.2f},{y0:.2f} {x0 + r:.2f},{y0:.2f} Z")


def main():
    src, out_svg = sys.argv[1], sys.argv[2]
    preview = sys.argv[3] if len(sys.argv) > 3 else None
    (w, h), alpha, lime = load(src)
    opaque = alpha > 0.5
    lime_mask = opaque & (lime > 0.5)

    ob = bbox(opaque)
    lb = bbox(lime_mask)
    # Miolo: caixa real do limão. Borda: espessura medida em cima/à esquerda, aplicada nos quatro lados
    # (o PNG vem cortado embaixo e tem um entalhe à direita).
    inner = lb
    border = ((lb[0] - ob[0]) + (lb[1] - ob[1])) / 2.0
    outer = (lb[0] - border, lb[1] - border, lb[2] + border, lb[3] + border)
    side_o = outer[2] - outer[0]
    r_out = corner_radius(opaque, outer)
    r_in = corner_radius(lime_mask, inner)

    # Tinta preta dentro do miolo limão: campo = 1 - limão (antisserrilhado), recortado ao miolo com margem.
    ink = (1.0 - lime) * (alpha > 0.5)
    m = 3
    ix0, iy0, ix1, iy1 = [int(v) for v in (inner[0] + m, inner[1] + m, inner[2] - m, inner[3] - m)]
    sub = np.zeros_like(ink)
    sub[iy0:iy1, ix0:ix1] = ink[iy0:iy1, ix0:ix1]
    contours = marching_squares(sub, 0.5)
    # Descarta as cunhas dos cantos (entre o canto arredondado do miolo e a caixa retangular): elas encostam na
    # borda do recorte; o desenho (estrela, órbita, ponto) não.
    def touches_border(c):
        a = np.array(c)
        return a[:, 0].min() <= ix0 + 1 or a[:, 1].min() <= iy0 + 1 or a[:, 0].max() >= ix1 - 2 or a[:, 1].max() >= iy1 - 2
    contours = [c for c in contours if not touches_border(c)]
    paths = []
    for c in contours:
        simp = simplify_closed(c, 0.6)
        if len(simp) >= 3:
            paths.append(to_bezier_path(simp))

    # Normaliza para uma caixa 0..1024 (quadrado externo).
    s = 1024.0 / side_o
    tx, ty = -outer[0], -outer[1]
    transform = f'transform="scale({s:.6f}) translate({tx:.2f},{ty:.2f})"'
    vb_h = (outer[3] - outer[1]) * s  # proporção real do original (o miolo não é exatamente quadrado)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="{vb_h:.0f}" viewBox="0 0 1024 {vb_h:.2f}">',
           f'<g {transform}>',
           f'<path d="{rounded_rect_path(*outer, r_out)}" fill="#0B0C0E"/>',
           f'<path d="{rounded_rect_path(*inner, r_in)}" fill="#CAFB04"/>',
           f'<path d="{" ".join(paths)}" fill="#0B0C0E" fill-rule="evenodd"/>',
           '</g></svg>']
    open(out_svg, "w", encoding="utf-8").write("\n".join(svg))
    # Glyph: só o desenho interno, cor herdada (ícone monocromático, fundos de estado vazio).
    gx0, gy0 = inner[0], inner[1]
    gs = 1024.0 / (inner[2] - inner[0])
    gh = (inner[3] - inner[1]) * gs
    glyph = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="{gh:.0f}" viewBox="0 0 1024 {gh:.2f}">',
             f'<g transform="scale({gs:.6f}) translate({-gx0:.2f},{-gy0:.2f})">',
             f'<path d="{" ".join(paths)}" fill="currentColor" fill-rule="evenodd"/>', '</g></svg>']
    open(out_svg.replace("-mark.svg", "-glyph.svg"), "w", encoding="utf-8").write("\n".join(glyph))

    # Diferença: rasteriza o vetor (polígonos densos dos contornos) e compara classes pixel a pixel.
    big = 4
    test = Image.new("L", (w * big, h * big), 0)  # 0 fundo, 1 preto, 2 limão
    dr = ImageDraw.Draw(test)
    def rr(box, r, val):
        x0, y0, x1, y1 = [v * big for v in box]
        dr.rounded_rectangle([x0, y0, x1 - 1, y1 - 1], radius=r * big, fill=val)
    rr(outer, r_out, 1)
    rr(inner, r_in, 2)
    ink_layer = Image.new("1", (w * big, h * big), 0)
    di = ImageDraw.Draw(ink_layer)
    for c in contours:
        di.polygon([(x * big, y * big) for x, y in c], fill=1, outline=None)
    # evenodd aproximado: contornos internos (buracos) são raros aqui; XOR resolve os buracos.
    arr_test = np.array(test)
    arr_ink = np.zeros(arr_test.shape, dtype=bool)
    for c in contours:
        layer = Image.new("1", (w * big, h * big), 0)
        ImageDraw.Draw(layer).polygon([(x * big, y * big) for x, y in c], fill=1)
        arr_ink ^= np.array(layer, dtype=bool)
    arr_test = np.where(arr_ink & (arr_test == 2), 1, arr_test)
    test_small = np.array(Image.fromarray(arr_test.astype(np.uint8)).resize((w, h), Image.NEAREST))
    ref = np.where(~opaque, 0, np.where(lime > 0.5, 2, 1))
    # Compara só dentro do quadrado externo (fora dele o PNG tem o artefato do corte).
    ox0, oy0, ox1, oy1 = [int(round(v)) for v in outer]
    ox0, oy0 = max(ox0, 0), max(oy0, 0)
    oy1, ox1 = min(oy1, h), min(ox1, w)
    region = (slice(oy0, oy1), slice(ox0, ox1))
    diff = test_small[region] != ref[region]
    frac = diff.mean()
    print(f"quadrado externo {outer} raio {r_out:.1f}; miolo {inner} raio {r_in:.1f}; contornos {len(contours)}")
    print(f"diferença de classe dentro do quadrado: {frac * 100:.2f}% dos pixels ({diff.sum()} de {diff.size})")
    if preview:
        vis = np.zeros((oy1 - oy0, (ox1 - ox0) * 3, 3), dtype=np.uint8)
        pal = np.array([[245, 245, 245], [11, 12, 14], [202, 251, 4]], dtype=np.uint8)
        vis[:, : ox1 - ox0] = pal[ref[region]]
        vis[:, ox1 - ox0: 2 * (ox1 - ox0)] = pal[test_small[region]]
        vis[:, 2 * (ox1 - ox0):] = np.where(diff[..., None], np.array([255, 0, 90], np.uint8), np.array([30, 30, 30], np.uint8))
        Image.fromarray(vis).save(preview)


if __name__ == "__main__":
    main()
