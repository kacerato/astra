"""Gera a textura de teste do spike S-01 em KTX 1.1 (RGBA8 sRGB, cadeia de mips completa).

Padrão: xadrez grafite/limão (cores da marca) com faixa de gradiente no topo, para que
orientação de UV e filtragem de mips fiquem visíveis na captura.

Uso: python make_texture.py <saida.ktx>
"""

import struct
import sys

SIZE = 256
CELLS = 8
GRAPHITE = (0x15, 0x17, 0x1B, 0xFF)
LIME = (0xCA, 0xFB, 0x04, 0xFF)

GL_UNSIGNED_BYTE = 0x1401
GL_RGBA = 0x1908
GL_SRGB8_ALPHA8 = 0x8C43
KTX_IDENTIFIER = bytes([0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A])


def base_level():
    pixels = bytearray()
    cell = SIZE // CELLS
    for y in range(SIZE):
        for x in range(SIZE):
            if y < cell // 2:
                # Faixa superior: gradiente horizontal grafite -> limão (mostra a direção de U).
                t = x / (SIZE - 1)
                pixels += bytes(round(GRAPHITE[i] + (LIME[i] - GRAPHITE[i]) * t) for i in range(4))
            else:
                pixels += bytes(LIME if ((x // cell) + (y // cell)) % 2 == 0 else GRAPHITE)
    return SIZE, pixels


def downsample(size, pixels):
    half = size // 2
    out = bytearray()
    for y in range(half):
        for x in range(half):
            for c in range(4):
                total = 0
                for dy in (0, 1):
                    for dx in (0, 1):
                        total += pixels[((2 * y + dy) * size + (2 * x + dx)) * 4 + c]
                out.append((total + 2) // 4)
    return half, out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    levels = [base_level()]
    while levels[-1][0] > 1:
        levels.append(downsample(*levels[-1]))

    header = KTX_IDENTIFIER + struct.pack(
        "<13I",
        0x04030201,       # endianness
        GL_UNSIGNED_BYTE,  # glType
        1,                 # glTypeSize
        GL_RGBA,           # glFormat
        GL_SRGB8_ALPHA8,   # glInternalFormat
        GL_RGBA,           # glBaseInternalFormat
        SIZE, SIZE, 0,     # largura, altura, profundidade
        0,                 # elementos de array
        1,                 # faces
        len(levels),       # níveis de mip
        0,                 # bytes de key/value
    )
    with open(sys.argv[1], "wb") as f:
        f.write(header)
        for _, data in levels:
            f.write(struct.pack("<I", len(data)))
            f.write(data)  # RGBA8: sempre múltiplo de 4, sem padding


if __name__ == "__main__":
    main()
