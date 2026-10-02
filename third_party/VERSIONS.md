# Bibliotecas de terceiros fixadas

Toda biblioteca entra fixada por commit. Atualização só por tarefa explícita (AGENTS.md §6).

| Biblioteca | Versão | Commit | Origem | Licença | Forma | Desde |
|---|---|---|---|---|---|---|
| GameActivity (androidx.games:games-activity) | 4.4.2 | AAR sha256 `16069bb61a34e9cc7a4f6b4cef0ddb9e2f3f61fe781b282787f574f8880ba38e` | https://dl.google.com/android/maven2/androidx/games/games-activity/4.4.2/ | Apache-2.0 | `third_party/games-activity`: headers e `libgame-activity_static.a` (arm64) do prefab do AAR; o Java vem do mesmo AAR pelo Gradle | 02/10/2026 |
| flecs | v4.1.6 (29/06/2026) | `fb55f3c25660425cfe1bc4cf5e6bff8b3f18a9b8` | https://github.com/SanderMertens/flecs | MIT | Submódulo `third_party/flecs`; `distr/flecs.c` com `FLECS_CUSTOM_BUILD` (`cmake/Flecs.cmake`) | 02/10/2026 |
| RmlUi | 6.3 (22/08/2026) | `ba95ffe8bfb6370efb2cdcca927eaad4710c5413` | https://github.com/mikke89/RmlUi | MIT | Submódulo `third_party/rmlui`; `rmlui_core`/`rmlui_debugger` estáticos (`cmake/RmlUi.cmake`) | 02/10/2026 |
| FreeType | 2.14.3 (VER-2-14-3) | `0a0221a1347e2f1e07c395263540026e9a0aa7c7` | https://github.com/freetype/freetype | FreeType License (FTL) | Submódulo `third_party/freetype`; sem zlib/bzip2/png/harfbuzz/brotli; entregue à RmlUi pelo redirecionamento de `find_package` | 02/10/2026 |
| Luau | 0.740 (25/09/2026) | `c0e346edd89066b44dca174c9f54ce84c746a540` | https://github.com/luau-lang/luau | MIT | Submódulo `third_party/luau`; `Luau.VM`, `Luau.Compiler`, `Luau.CodeGen` (`cmake/Luau.cmake`) | 02/10/2026 |
| glslang | 16.6.0 (11/09/2026) | `e1b562a8bed273a02f30b59b66a5d499793cede5` | https://github.com/KhronosGroup/glslang | BSD-3-Clause, BSD-2, MIT, Apache-2.0 (LICENSE.txt; sem Bison/GPL) | Submódulo `third_party/glslang`; só front-end GLSL + SPIR-V (`cmake/Glslang.cmake`) | 02/10/2026 |
| The Forge | v1.63 (21/03/2025) | upstream `7443530ef7e888771f307de74b7872becfd64450` | https://github.com/ConfettiFX/The-Forge | Apache-2.0 | Submódulo `third_party/the-forge` → fork privado https://github.com/kacerato/astra-forge, branch `astra/1.63`: commit-base `945f008` com a árvore idêntica à tag (clone raso, sem histórico) + patches `forge:` | 02/10/2026 |

## Componentes do The Forge usados como vieram

| Componente | Caminho no submódulo | Observação |
|---|---|---|
| Vulkan headers + glslangValidator + validation layers (SDK 1.3.275) | `Common_3/Graphics/ThirdParty/OpenSource/VulkanSDK` | `glslangValidator.exe` compila os shaders FSL no host |
| volk, VulkanMemoryAllocator | `Common_3/Graphics/ThirdParty/OpenSource/` | Compilados dentro do renderer |
| AGDK (swappy, paddleboat, memory_advice) estáticos | `Common_3/OS/ThirdParty/OpenSource/agdk` | Binários pré-compilados pelo upstream |
