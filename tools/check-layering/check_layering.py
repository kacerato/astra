"""Fronteira de terceiros (doc 04 §2, "Verificação automática").

Falha (código 1) se um arquivo C/C++ fora de `backends/<x>/` incluir header de biblioteca de terceiros,
ou se `flecs.h` aparecer fora de `engine/world/`. A dependência entre módulos Astra é conferida pelo
CMake (cmake/AstraModules.cmake), não aqui.

Fora da varredura: third_party/ (as próprias bibliotecas), spikes/ (código descartável que testa as
bibliotecas diretamente, AGENTS.md §2) e pastas de build.

Uso: python tools/check-layering/check_layering.py [raiz]
"""

import re
import sys
from pathlib import Path

ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
SKIP = {"third_party", "spikes", "build", ".git", ".idea", ".vs", "out"}
EXTS = {".h", ".hpp", ".hh", ".inl", ".c", ".cc", ".cpp", ".cxx", ".mm"}

# Biblioteca → padrão do caminho incluído. The Forge entra pela árvore Common_3 (IGraphics.h e o resto).
THIRD_PARTY = {
    "The Forge": r"(^|/)Common_3/|(^|/)IGraphics\.h$|(^|/)IResourceLoader\.h$",
    "Jolt": r"^Jolt/",
    "miniaudio": r"(^|/)miniaudio\.h$",
    "RmlUi": r"^RmlUi/",
    "Luau": r"^(lua|lualib|luacode|luaconf)\.h$|^Luau/",
    "ozz-animation": r"^ozz/",
    "Recast/Detour": r"(^|/)(Detour|Recast|DetourCrowd|DetourTileCache)\w*\.h$",
    "glslang": r"^glslang/",
    "FreeType": r"^ft2build\.h$|^freetype/",
    "GameActivity": r"^game-activity/|^game-text-input/",
}
FLECS = re.compile(r"(^|/)flecs(\.h|/)")
INCLUDE = re.compile(r'^\s*#\s*(?:include|import)\s*[<"]([^>"]+)[>"]')


def main():
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8")
    patterns = {lib: re.compile(p) for lib, p in THIRD_PARTY.items()}
    violations, scanned = [], 0
    for path in sorted(ROOT.rglob("*")):
        rel = path.relative_to(ROOT)
        if not path.is_file() or path.suffix.lower() not in EXTS or rel.parts[0] in SKIP:
            continue
        scanned += 1
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        for n, line in enumerate(lines, 1):
            m = INCLUDE.match(line)
            if not m:
                continue
            inc = m.group(1).replace("\\", "/")
            where = rel.as_posix()
            if FLECS.search(inc):
                if not where.startswith("engine/world/"):
                    violations.append(f"{where}:{n}: flecs só em engine/world ({inc})")
                continue
            if rel.parts[0] == "backends":
                continue
            for lib, pat in patterns.items():
                if pat.search(inc):
                    violations.append(f"{where}:{n}: {lib} só em backends/<x> ({inc})")
                    break
    for v in violations:
        print("ERRO:", v, file=sys.stderr)
    print(f"check-layering: {scanned} arquivos C/C++ verificados, {len(violations)} violações")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
