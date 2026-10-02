# Bibliotecas de terceiros fixadas

Toda biblioteca entra fixada por commit. Atualização só por tarefa explícita (AGENTS.md §6).

| Biblioteca | Versão | Commit | Origem | Licença | Aviso | Forma | Desde |
|---|---|---|---|---|---|---|---|
| GameActivity (androidx.games:games-activity) | 4.4.2 | AAR sha256 `16069bb61a34e9cc7a4f6b4cef0ddb9e2f3f61fe781b282787f574f8880ba38e` | https://dl.google.com/android/maven2/androidx/games/games-activity/4.4.2/ | Apache-2.0 | `LICENSE` | `third_party/games-activity`: headers e `libgame-activity_static.a` (arm64) do prefab do AAR; o Java vem do mesmo AAR pelo Gradle | 02/10/2026 |
| flecs | v4.1.6 (29/06/2026) | `fb55f3c25660425cfe1bc4cf5e6bff8b3f18a9b8` | https://github.com/SanderMertens/flecs | MIT | `LICENSE` | Submódulo `third_party/flecs`; `distr/flecs.c` com `FLECS_CUSTOM_BUILD` (`cmake/Flecs.cmake`) | 02/10/2026 |
| RmlUi | 6.3 (22/08/2026) | `ba95ffe8bfb6370efb2cdcca927eaad4710c5413` | https://github.com/mikke89/RmlUi | MIT | `LICENSE.txt` | Submódulo `third_party/rmlui`; `rmlui_core`/`rmlui_debugger` estáticos (`cmake/RmlUi.cmake`) | 02/10/2026 |
| FreeType | 2.14.3 (VER-2-14-3) | `0a0221a1347e2f1e07c395263540026e9a0aa7c7` | https://github.com/freetype/freetype | FreeType License (FTL) | `LICENSE.TXT`, `docs/FTL.TXT` | Submódulo `third_party/freetype`; sem zlib/bzip2/png/harfbuzz/brotli; entregue à RmlUi pelo redirecionamento de `find_package` | 02/10/2026 |
| Luau | 0.740 (25/09/2026) | `c0e346edd89066b44dca174c9f54ce84c746a540` | https://github.com/luau-lang/luau | MIT | `LICENSE.txt`, `lua_LICENSE.txt` | Submódulo `third_party/luau`; `Luau.VM`, `Luau.Compiler`, `Luau.CodeGen` (`cmake/Luau.cmake`) | 02/10/2026 |
| glslang | 16.6.0 (11/09/2026) | `e1b562a8bed273a02f30b59b66a5d499793cede5` | https://github.com/KhronosGroup/glslang | BSD-3-Clause, BSD-2, MIT, Apache-2.0 (LICENSE.txt; sem Bison/GPL) | `LICENSE.txt` | Submódulo `third_party/glslang`; só front-end GLSL + SPIR-V (`cmake/Glslang.cmake`) | 02/10/2026 |
| The Forge | v1.63 (21/03/2025) | upstream `7443530ef7e888771f307de74b7872becfd64450` | https://github.com/ConfettiFX/The-Forge | Apache-2.0 | `LICENSE` | Submódulo `third_party/the-forge` → fork privado https://github.com/kacerato/astra-forge, branch `astra/1.63`: commit-base `945f008` com a árvore idêntica à tag (clone raso, sem histórico) + patches `forge:` | 02/10/2026 |

## Componentes do The Forge usados como vieram

Fonte de `tools/licenses`: todo arquivo de `*/ThirdParty/*` do The Forge que aparece nas dependências de um build (`ninja -t deps`) precisa de uma linha aqui. **Aviso** é o arquivo (relativo ao caminho) cujo texto vai para `THIRD_PARTY_NOTICES.md`; `#Lx-y` recorta as linhas da licença dentro de um header.

| Componente | Caminho no submódulo | Licença | Aviso | Observação |
|---|---|---|---|---|
| Vulkan headers + glslangValidator + validation layers (SDK 1.3.275) | `Common_3/Graphics/ThirdParty/OpenSource/VulkanSDK` | Apache-2.0 / MIT | `LICENSE.txt` | `glslangValidator.exe` compila os shaders FSL no host; camada de validação só em Debug |
| volk | `Common_3/Graphics/ThirdParty/OpenSource/volk` | MIT | `volk.h#L1590-1608` | Compilado dentro do renderer |
| VulkanMemoryAllocator | `Common_3/Graphics/ThirdParty/OpenSource/VulkanMemoryAllocator` | MIT | `LICENSE.txt` | Compilado dentro do renderer |
| AMD AGS | `Common_3/Graphics/ThirdParty/OpenSource/ags` | MIT | `LICENSE.txt` | Só Windows (DLL ao lado do executável) |
| NVAPI | `Common_3/Graphics/ThirdParty/OpenSource/nvapi` | NVIDIA SDK License | `docs/NVAPI_SDKs_Samples_and_Tools_License_Agreement(Public).pdf` | Só Windows; contrato em PDF, citado pelo caminho |
| AGDK (swappy, paddleboat, memory_advice) estáticos | `Common_3/OS/ThirdParty/OpenSource/agdk` | Apache-2.0 | `LICENSE` | Binários pré-compilados pelo upstream |
| cpu_features | `Common_3/OS/ThirdParty/OpenSource/cpu_features` | Apache-2.0 | `LICENSE` | |
| zstd (só descompressão) | `Common_3/Utilities/ThirdParty/OpenSource/zstd` | BSD-3-Clause | `LICENSE` | |
| lz4 | `Common_3/Utilities/ThirdParty/OpenSource/lz4` | BSD-2-Clause | `LICENSE` | |
| bstrlib | `Common_3/Utilities/ThirdParty/OpenSource/bstrlib` | BSD-3-Clause | `LICENSE` | |
| ModifiedSonyMath | `Common_3/Utilities/ThirdParty/OpenSource/ModifiedSonyMath` | BSD-3-Clause | `LICENSE.txt` | |
| stb (Nothings) | `Common_3/Utilities/ThirdParty/OpenSource/Nothings` | MIT ou domínio público | `stb_ds.h#L2389-2425` | Mesmo texto em todos os `stb_*.h` |
| MurmurHash3 | `Common_3/Utilities/ThirdParty/OpenSource/murmurhash3` | Domínio público | `MurmurHash3_32.h#L2-3` | |
| Fluid Studios mmgr | `Common_3/Utilities/ThirdParty/OpenSource/FluidStudios` | Própria (crédito obrigatório; versão modificada exige indicar o original) | `MemoryManager/mmgr.h#L17-31` | Só com rastreio de memória (Debug); o Release não compila |
| tinyimageformat | `Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat` | MIT (DeanoC) | `tinyimageformat_base.h#L39-45` | O header declara MIT sem o texto; texto igual ao de tinyktx |
| tinyktx | `Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyktx` | MIT | `tinyktx.h#L2452-2472` | |
| tinydds | `Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinydds` | MIT | `tinydds.h#L2282-2302` | |
| Dear ImGui | `Common_3/Application/ThirdParty/OpenSource/imgui` | MIT | `LICENSE.txt` | Só `tf_os` (spikes) |
| Fontstash | `Common_3/Application/ThirdParty/OpenSource/Fontstash` | Zlib | `LICENSE.txt` | Só `tf_os` (spikes) |
| Lua 5.3.5 | `Common_3/Game/ThirdParty/OpenSource/lua-5.3.5` | MIT | `src/lua.h#L463-482` | Só `tf_os` (spikes) |
| ozz-animation | `Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation` | MIT | `LICENSE.md` | Só `tf_os` (spikes) |
