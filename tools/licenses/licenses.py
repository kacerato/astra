"""Licenças de terceiros (doc 03 §5, R-11).

Fonte única: third_party/VERSIONS.md
- tabela principal: uma linha por pasta de third_party/ (coluna Forma cita `third_party/<pasta>`),
  coluna Aviso com os arquivos de licença;
- tabela de componentes do The Forge: caminho no submódulo e Aviso (`arquivo` ou `arquivo#Lx-y`).

Falha (código 1) quando:
- uma pasta de third_party/ não tem linha na tabela principal;
- um arquivo de Aviso não existe, ou um recorte #Lx-y não contém texto de licença;
- com --build: um arquivo de terceiros que entrou no build (ninja -t deps) não está registrado;
- com --check: THIRD_PARTY_NOTICES.md não corresponde ao que seria gerado.

Uso:
  python tools/licenses/licenses.py                 # gera THIRD_PARTY_NOTICES.md
  python tools/licenses/licenses.py --check         # CI: só confere
  python tools/licenses/licenses.py --check --build build/android-arm64-release [--build ...]
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TP = ROOT / "third_party"
VERSIONS = TP / "VERSIONS.md"
NOTICES = ROOT / "THIRD_PARTY_NOTICES.md"
FORGE = "the-forge"
# Palavras que um recorte de licença precisa conter (pega intervalos de linha desatualizados).
LICENSE_WORDS = re.compile(r"licen[cs]e|permission|public domain|copyright|redistribut", re.I)


def cells(line):
    return [c.strip() for c in line.strip().strip("|").split("|")]


def ticks(cell):
    return re.findall(r"`([^`]+)`", cell)


def parse_versions():
    libs, comps, table = [], [], None
    for line in VERSIONS.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|"):
            table = None
            continue
        c = cells(line)
        if c[0] == "Biblioteca":
            table, head = "libs", c
            continue
        if c[0] == "Componente":
            table, head = "comps", c
            continue
        if table is None or set(line) <= set("|-: "):
            continue
        row = dict(zip(head, c))
        if table == "libs":
            m = re.search(r"third_party/([\w.-]+)", row["Forma"])
            if not m:
                sys.exit(f"VERSIONS.md: linha sem `third_party/<pasta>` na coluna Forma: {row['Biblioteca']}")
            libs.append({"name": row["Biblioteca"], "dir": m.group(1), "version": row["Versão"],
                         "license": row["Licença"], "notices": ticks(row["Aviso"]), "origin": row["Origem"]})
        else:
            comps.append({"name": row["Componente"], "path": ticks(row["Caminho no submódulo"])[0].rstrip("/"),
                          "license": row["Licença"], "notices": ticks(row["Aviso"])})
    return libs, comps


def read_notice(base, spec, errors):
    """Texto do aviso; `arquivo#Lx-y` recorta linhas. PDF é citado pelo caminho."""
    file, _, rng = spec.partition("#")
    path = base / file
    if not path.is_file():
        errors.append(f"aviso inexistente: {path.relative_to(ROOT).as_posix()}")
        return None
    if path.suffix.lower() == ".pdf":
        return f"(Contrato em PDF, distribuído junto: {path.relative_to(ROOT).as_posix()})"
    text = path.read_bytes().decode("utf-8", errors="replace").replace("\r\n", "\n").replace("\r", "\n")
    if rng:
        m = re.fullmatch(r"L(\d+)-(\d+)", rng)
        if not m:
            errors.append(f"recorte inválido: {spec}")
            return None
        a, b = int(m.group(1)), int(m.group(2))
        text = "\n".join(text.split("\n")[a - 1:b])
        if not LICENSE_WORDS.search(text):
            errors.append(f"recorte sem texto de licença (linhas mudaram?): {path.relative_to(ROOT).as_posix()}#{rng}")
    return "\n".join(l.rstrip() for l in text.strip("\n").split("\n"))


def render(libs, comps, errors):
    out = ["# Avisos de terceiros", "",
           "Gerado por `tools/licenses/licenses.py` a partir de `third_party/VERSIONS.md`. Não edite à mão.", ""]

    def section(title, meta, base, specs):
        out.extend([f"## {title}", "", meta, ""])
        for spec in specs:
            text = read_notice(base, spec, errors)
            if text is None:
                continue
            out.extend([f"`{spec}`", "", "````text", text, "````", ""])

    for lib in libs:
        if not lib["notices"]:
            errors.append(f"{lib['name']}: coluna Aviso vazia")
        section(lib["name"], f"Versão {lib['version']} · {lib['license']} · {lib['origin']}", TP / lib["dir"], lib["notices"])
    for comp in comps:
        if not comp["notices"]:
            errors.append(f"{comp['name']}: coluna Aviso vazia")
        section(f"{comp['name']} (dentro do The Forge)", f"{comp['license']} · `third_party/{FORGE}/{comp['path']}`",
                TP / FORGE / comp["path"], comp["notices"])
    return "\n".join(out).rstrip("\n") + "\n"


def build_deps(build_dir):
    ninja = os.environ.get("NINJA", "ninja")
    res = subprocess.run([ninja, "-C", str(build_dir), "-t", "deps"], capture_output=True, text=True,
                         encoding="utf-8", errors="replace")
    if res.returncode != 0:
        sys.exit(f"ninja -t deps falhou em {build_dir}: {res.stderr.strip()}")
    deps = set()
    for line in res.stdout.splitlines():
        if line.startswith("    "):
            p = line.strip().replace("\\", "/")
            i = p.find("third_party/")
            if i >= 0:
                deps.add(p[i + len("third_party/"):])
    return deps


def check_deps(deps, libs, comps, errors):
    dirs = {l["dir"] for l in libs}
    paths = [c["path"] + "/" for c in comps]
    missing = set()
    for d in deps:
        top, _, rest = d.partition("/")
        if top not in dirs:
            missing.add(f"third_party/{top}")
        elif top == FORGE and "/ThirdParty/" in rest and not any(rest.startswith(p) for p in paths):
            m = re.match(r"(.*?/ThirdParty/(?:OpenSource/)?[^/]+)", rest)
            missing.add(f"third_party/{FORGE}/{m.group(1) if m else rest}")
    for m in sorted(missing):
        errors.append(f"usado no build e sem registro em VERSIONS.md: {m}")
    return len(deps)


def main():
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="não grava; falha se THIRD_PARTY_NOTICES.md estiver desatualizado")
    ap.add_argument("--build", action="append", default=[], help="pasta de build Ninja para conferir dependências reais")
    args = ap.parse_args()

    errors = []
    libs, comps = parse_versions()
    registered = {l["dir"] for l in libs}
    for d in sorted(p.name for p in TP.iterdir() if p.is_dir()):
        if d not in registered:
            errors.append(f"pasta sem licença registrada em VERSIONS.md: third_party/{d}")

    text = render(libs, comps, errors)
    for b in args.build:
        n = check_deps(build_deps(Path(b)), libs, comps, errors)
        print(f"{b}: {n} arquivos de terceiros nas dependências")

    if args.check:
        current = NOTICES.read_text(encoding="utf-8") if NOTICES.exists() else ""
        if current != text:
            errors.append("THIRD_PARTY_NOTICES.md desatualizado: rode python tools/licenses/licenses.py")
    elif not errors:
        NOTICES.write_text(text, encoding="utf-8", newline="\n")
        print(f"gravado {NOTICES.name}: {len(libs)} bibliotecas, {len(comps)} componentes do The Forge")

    for e in errors:
        print("ERRO:", e, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
