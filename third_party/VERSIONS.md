# Bibliotecas de terceiros fixadas

Toda biblioteca entra fixada por commit. Atualização só por tarefa explícita (AGENTS.md §6).

| Biblioteca | Versão | Commit | Origem | Licença | Forma | Desde |
|---|---|---|---|---|---|---|
| GameActivity (androidx.games:games-activity) | 4.4.2 | AAR sha256 `16069bb61a34e9cc7a4f6b4cef0ddb9e2f3f61fe781b282787f574f8880ba38e` | https://dl.google.com/android/maven2/androidx/games/games-activity/4.4.2/ | Apache-2.0 | `third_party/games-activity`: headers e `libgame-activity_static.a` (arm64) do prefab do AAR; o Java vem do mesmo AAR pelo Gradle | 02/10/2026 |
| flecs | v4.1.6 (29/06/2026) | `fb55f3c25660425cfe1bc4cf5e6bff8b3f18a9b8` | https://github.com/SanderMertens/flecs | MIT | Submódulo `third_party/flecs`; `distr/flecs.c` com `FLECS_CUSTOM_BUILD` (`cmake/Flecs.cmake`) | 02/10/2026 |
| glslang | 16.6.0 (11/09/2026) | `e1b562a8bed273a02f30b59b66a5d499793cede5` | https://github.com/KhronosGroup/glslang | BSD-3-Clause, BSD-2, MIT, Apache-2.0 (LICENSE.txt; sem Bison/GPL) | Submódulo `third_party/glslang`; só front-end GLSL + SPIR-V (`cmake/Glslang.cmake`) | 02/10/2026 |
| The Forge | v1.63 (21/03/2025) | upstream `7443530ef7e888771f307de74b7872becfd64450` | https://github.com/ConfettiFX/The-Forge | Apache-2.0 | Submódulo `third_party/the-forge` → fork privado https://github.com/kacerato/astra-forge, branch `astra/1.63`: commit-base `945f008` com a árvore idêntica à tag (clone raso, sem histórico) + patches `forge:` | 02/10/2026 |

## Componentes do The Forge usados como vieram

| Componente | Caminho no submódulo | Observação |
|---|---|---|
| Vulkan headers + glslangValidator + validation layers (SDK 1.3.275) | `Common_3/Graphics/ThirdParty/OpenSource/VulkanSDK` | `glslangValidator.exe` compila os shaders FSL no host |
| volk, VulkanMemoryAllocator | `Common_3/Graphics/ThirdParty/OpenSource/` | Compilados dentro do renderer |
| AGDK (swappy, paddleboat, memory_advice) estáticos | `Common_3/OS/ThirdParty/OpenSource/agdk` | Binários pré-compilados pelo upstream |
