# 03 — Pilha tecnológica e bibliotecas

Versões, licenças e datas foram consultadas em 01/10/2026 nos repositórios oficiais (API do GitHub, README do The Forge no GitHub e no Codeberg, changelog da RmlUi). Cada biblioteca entra **fixada por commit** em `third_party/VERSIONS.md` e só é atualizada por tarefa explícita.

---

## 1. Visão geral

```
                         Astra API (gameplay, editor, formatos)
                                        │
   ┌──────────┬──────────┬──────────┬───┴──────┬──────────┬──────────┬──────────┐
   │ Render   │ Physics  │ Audio    │ Anim     │ Nav      │ UI       │ Script   │  Servidores Astra
   ├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
   │ forge    │ jolt     │ miniaudio│ ozz      │ recast   │ rmlui    │ luau     │  backends/ (únicos a incluir terceiros)
   └────┬─────┴────┬─────┴────┬─────┴────┬─────┴────┬─────┴────┬─────┴────┬─────┘
   The Forge 1.63  Jolt 5.6  miniaudio  Ozz 0.17  Recast 1.6  RmlUi 6.3  Luau 0.740
   (Vulkan, RL,                0.11.25
    FSL, OS, gpu.data)
                                        │
        flecs 4.1 (armazenamento ECS)  ·  fastgltf · ufbx · meshoptimizer · KTX/BasisU/astcenc  (importação)
```

O flecs fica **no núcleo**, não num backend. Ele é a estrutura de dados do mundo, não um serviço trocável. Mesmo assim, nenhum tipo `flecs::` aparece na API pública (ver [05](05-MODELO-DE-OBJETOS-E-REFLEXAO.md) §3).

## 2. Tabela mestra

| Biblioteca | Versão a fixar | Licença | Papel | Camada | Fase |
|---|---|---|---|---|---|
| **The Forge** | **v1.63** (GitHub, 21/03/2025) | Apache-2.0 | Vulkan, Resource Loader, FSL, OS/FS/threads, gpu.data, Swappy | `backends/forge` + `foundation` (OS) | F0 |
| **flecs** | v4.1.6 (29/06/2026) | MIT | Armazenamento ECS, hierarquia, consultas, agendamento | `engine/world` | F1 |
| **Jolt Physics** | v5.6.0 (11/07/2026) | MIT | Física 3D | `backends/jolt` | F5 |
| **ozz-animation** | 0.17.0 (01/08/2026) | MIT | Amostragem, blending, IK, offline builder | `backends/ozz` | F8 |
| **meshoptimizer** | v1.3 (25/09/2026) | MIT | Otimização, quantização, LOD, meshlets, decodificação EXT_meshopt | `editor/importers`, `backends/forge` | F3 |
| **miniaudio** | 0.11.25 (03/03/2026) | Domínio público ou MIT-0 | Áudio (engine, mixer, 3D, decodificação, AAudio/OpenSL) | `backends/miniaudio` | F9 |
| **Recast/Detour** | v1.6.0 + commit do `main` | Zlib | NavMesh, pathfinding, crowd, tile cache | `backends/recast` | F10 |
| **RmlUi** | 6.3 (22/08/2026) | MIT | UI do editor e do jogo | `backends/rmlui` + `editor/ui` | F0 (spike), F4 |
| **FreeType** | estável atual | FTL (escolhida no lugar da GPLv2) | Rasterização de glifos para a RmlUi | `backends/rmlui` | F4 |
| **HarfBuzz** | estável atual | MIT | Shaping de texto (motor HarfBuzz da RmlUi 6.2+) | `backends/rmlui` | F11 |
| **LunaSVG** | compatível com RmlUi 6.3 | MIT | Plugin SVG da RmlUi (ilustrações multicor) | `backends/rmlui` | F4 |
| **fastgltf** | v0.9.1 (27/09/2026) | MIT | Leitura de glTF/GLB | `editor/importers` | F3 |
| **simdjson** | exigida pelo fastgltf | Apache-2.0 | Parser JSON do fastgltf | idem | F3 |
| **ufbx** | estável atual | MIT ou Unlicense | FBX e OBJ/MTL | `editor/importers` | F3 |
| **Luau** | 0.740 (25/09/2026) | MIT | VM, compilador, analisador de tipos | `backends/luau` + `editor/script` | F6 |
| **KTX-Software (libktx)** | v4.4.2 (04/10/2025) | Apache-2.0 | Contêiner KTX2 | importação + runtime | F3 |
| **basis_universal** | v2_50 (03/08/2026) | Apache-2.0 | UASTC/ETC1S e transcodificação | importação + runtime | F3 |
| **astc-encoder** | estável atual | Apache-2.0 | Compressão ASTC direta | importação | F3 |
| **stb_image / stb_image_resize** | atual | MIT ou domínio público | PNG/JPG/TGA/HDR e redimensionamento | importação | F3 |
| **tinyexr** | atual | BSD-3 | EXR | importação | F3 |
| **MikkTSpace** | atual | Zlib | Tangentes | importação | F3 |
| **glslang** | estável atual | BSD-3 e outras permissivas | Compilar no aparelho o GLSL gerado pelo Shader Graph | `backends/forge` | F14 (S-04 em F0) |
| **SPIRV-Cross** | a da 1.63 | Apache-2.0 | Reflexão/tradução (já usada pelo TF) | `third_party/the-forge` | F0 |
| **Tracy** | v0.14.1 (22/08/2026) | BSD-3 | Profiler de desenvolvimento | `foundation/profile` | F1 |
| **doctest** | atual | MIT | Testes unitários | `tests/` | F1 |
| **AGDK**: GameActivity, GameTextInput, Paddleboat, Swappy | versões do AGDK atual | Apache-2.0 | Atividade nativa, IME, controles, frame pacing | `platform/android` | F0/F2 |
| **apksig** (AOSP) | atual | Apache-2.0 | Assinatura de APK no aparelho | `apps/android-editor` (Java/Kotlin) | F12 |
| **msdf-atlas-gen** | v1.4 | MIT | Ferramenta offline: atlas MSDF de ícones | `tools/` | F4 |
| **xatlas** | atual | MIT | UV2 para lightmaps (futuro) | importação | após F13 |
| **Inter / JetBrains Mono** | atuais | OFL-1.1 | Fontes de UI e código | `assets` | F4 |

### Avaliadas e não adotadas

| Biblioteca | Motivo |
|---|---|
| **Yoga** | A RmlUi já tem flexbox. Dois motores de layout para a mesma árvore = duplicação |
| **Dear ImGui / Nuklear** (UI do TF) | Sem tema, docking ou retenção adequados ao produto. O módulo de UI do TF é **excluído do build** |
| **Lua 5.3.5 do TF** | Substituída por Luau (tipos, sandbox, desempenho) |
| **flecs e Ozz embutidos no TF** | Versões antigas. Usamos upstream; as cópias do TF são **excluídas do build** para evitar símbolos duplicados |
| **The Forge 1.64** | Só PC/DX12 no repositório público |
| **cgltf** | Boa, mas o fastgltf é mais rápido e mais completo em extensões; cgltf fica como plano B |
| **PhysX 5** | Mais pesado e menos alinhado a mobile que o Jolt |
| **Draco** | Só se surgir demanda por `KHR_draco_mesh_compression`; `EXT_meshopt_compression` cobre o caso comum |

---

## 3. The Forge — o que é, o que usamos, o que falta

### 3.1 Fatos verificados (01/10/2026)

- **GitHub `ConfettiFX/The-Forge`:** último release **v1.63**, de 21/03/2025. O README diz que a 1.64 continua no Codeberg. A árvore tem `Common_3/Graphics/{Vulkan, Metal, Direct3D12, OpenXR, Quest}`, `Examples_3/Unit_Tests/{Android_VS2019, PC_VS2019, macOS_Xcode, SteamOS_CodeLite, Quest_VS2019}`.
- **Codeberg `The-Forge/The-Forge`:** release **1.64** (12/08/2026). Trocou ImGui por Nuklear e reconstruiu Aura, PixelPuzzle, Ephemeris e Gladiator. *"This repository only holds the PC / DirectX 12 runtime."* `Common_3/Graphics` tem apenas `Direct3D12`.
- **Android na 1.63:** Android 9+ com Vulkan 1.1, API 23+. Build pelo Visual Studio 2019 + **AGDE 23.1.82** + **NDK r21e**. Swappy integrado desde a 1.56.
- **O que o próprio TF diz que não tem:** *"Renderer / Scene (not provided) / Resource Streaming (not provided)"* e *"What is not there: Physics / Networking / Sound"*.
- **Recursos de alto nível na 1.63:** Resource Loader assíncrono, Lua (para testes automáticos), animação baseada em Ozz, Vectormath com NEON, VMA/D3D12MA, input próprio com gestos, ECS baseado em flecs, FileSystem com zip, UI baseada em ImGui, FSL 2, configuração de GPU por `gpu.data`/`gpu.cfg`.
- **Licença:** Apache-2.0 para as plataformas abertas; consoles exigem licença comercial.

### 3.2 Módulos usados

| Módulo do TF | Uso na Astra 2 | Observação |
|---|---|---|
| `Graphics/Interfaces` + `Graphics/Vulkan` | Base do `ForgeRenderer` | Só o backend Vulkan entra no build |
| `Graphics/FSL` + ferramentas | Compilação offline dos shaders internos | Exige Python no host; não roda no aparelho |
| `GraphicsConfig` + `gpu.data`/`gpu.cfg` | Classificação de GPU e presets de tier | Dados versionados pela Astra |
| `Resources/ResourceLoader` | Upload assíncrono de buffers e texturas | A Astra controla **o que** carregar; o RL controla **como** subir para a GPU |
| `OS/Android`, `OS/Windows`, `OS/Linux` | Arquivo, threads, log, tempo, memória | Encapsulados pela `foundation`; ver S-02 sobre o loop de app |
| `OS/Input` | **A decidir no S-02** | Se depender do `IApp`/janela do TF, usamos entrada própria via GameActivity |
| `Utilities` (math, containers C) | Interno ao backend | A API Astra tem tipos matemáticos próprios |
| Swappy | Frame pacing no Android | Mantido |

### 3.3 Módulos excluídos do build

`Application/UI` (ImGui), `Application/Fonts` (Fontstash), `Game/Scripting` (Lua 5.3.5), `Game/ThirdParty/flecs`, `Resources/AnimationSystem` (Ozz embutido), `Application/Profiler` (substituído por Tracy), backends `Direct3D12`, `Metal` (até a F15), `OpenXR`/`Quest`.

### 3.4 O fork `astra-forge`

- Fonte: commit da tag `v1.63`, registrado em `third_party/VERSIONS.md`.
- Toda alteração fica em commits pequenos com prefixo `forge:` e é listada em `third_party/the-forge/ASTRA_PATCHES.md` (arquivo, motivo, risco). A Apache-2.0 exige marcar arquivos modificados.
- Patches esperados: build CMake, NDK r28+/clang novo, Vulkan no Windows (S-03), remoção dos módulos excluídos, ganchos de log e memória, GameActivity (S-02), correções de validação encontradas no aparelho.
- **Política de upstream:** nenhuma sincronização automática. Correções da 1.64 (PC) podem ser estudadas e portadas manualmente quando tocarem código comum.

## 4. Integração de cada biblioteca

Cada backend segue o mesmo contrato: **ganchos de memória, log, jobs, asserts e perfil**; nenhum tipo externo na interface pública; teste próprio no host.

| Biblioteca | Memória | Threads/jobs | Log/assert | Particularidades |
|---|---|---|---|---|
| The Forge | `tf_malloc` redirecionado para a `foundation` (mmgr só em debug) | Threads de I/O próprias do RL | Callback de log → `astra::log` | Estado global do renderer; uma instância por processo |
| flecs | `ecs_os_set_api` (malloc/free) | `ecs_os_api` task threads → jobs Astra | `ecs_os_api.log_` → `astra::log` | Desligar addons não usados (REST/HTTP só em debug para o Explorer) |
| Jolt | `JPH::Allocate`/`Free`/`AlignedAllocate` | `JPH::JobSystemWithBarrier` implementado sobre jobs Astra | `JPH::Trace`, `AssertFailed` | `JPH_CROSS_PLATFORM_DETERMINISTIC` opcional (custo); `JPH_DOUBLE_PRECISION` não na v1 |
| Ozz | `ozz::memory::Allocator` próprio | Jobs de amostragem por personagem | — | A lib offline (`ozz_animation_offline`) só entra no editor |
| miniaudio | `ma_allocation_callbacks` | Thread do device (callback de tempo real) | `ma_log` | Callback de áudio sem alocação nem lock bloqueante |
| Recast/Detour | `dtAllocSetCustom`, `rcAllocSetCustom` | Bake de tiles em jobs | `rcContext` → log | Detour não é thread-safe por `dtNavMeshQuery`: uma query por thread |
| RmlUi | Sem gancho global (usa new/delete) | Uma thread (UI) | `Rml::SystemInterface::LogMessage` | `RenderInterface` sobre o servidor de render; `SystemInterface` sobre tempo e clipboard da plataforma |
| fastgltf | Alocador padrão; buffers grandes via mapeamento | Importação em job | Erros por `fastgltf::Error` → relatório | Compilar sem exceções (usa `Expected`) |
| ufbx | `ufbx_allocator_opts` | Importação em job | `ufbx_error` → relatório | Limites de memória por arquivo (proteção contra arquivos maliciosos) |
| Luau | `lua_Alloc` com limite por VM | Uma VM por mundo; analisador em job no editor | `lua_callbacks()->panic`/erro → console | `interrupt` para limite de instruções; ver [14](14-SCRIPTING-LUAU.md) |
| KTX/BasisU/astcenc | Alocador padrão | Codificação em jobs | Códigos de erro → relatório | Codificação ASTC é cara: presets por qualidade, medição no S-09 |
| Tracy | — | Zonas em todas as threads | — | Desligado em release do player |

### Exceções e RTTI

- Código Astra **não lança exceções**: usa `Result<T, Error>` e códigos de erro.
- O build mantém `-fexceptions` e RTTI ligados, para compatibilizar o Luau (que usa exceções C++ por padrão e assim garante a execução dos destrutores nos bindings), a RmlUi e o SPIRV-Cross. Confirmar custo de tamanho no S-01.
- Regra de binding: nenhuma função C++ chamada pelo Luau deixa uma exceção atravessar código C do TF ou do flecs.

## 5. Conformidade de licenças

| Obrigação | Como cumprir |
|---|---|
| Apache-2.0 (The Forge, KTX, BasisU, astcenc, AGDK, apksig, simdjson) | Incluir LICENSE e NOTICE; marcar arquivos modificados no fork; não usar marcas registradas como se fossem nossas |
| MIT/BSD/Zlib/ISC | Manter avisos de copyright no código e na tela de licenças |
| FTL (FreeType) | Crédito na documentação e na tela "Sobre/Licenças" |
| OFL (Inter, JetBrains Mono) | Distribuir a licença com a fonte; não vender a fonte isolada |
| Código portado de Godot, Wicked, Stride ou ezEngine (todos MIT) | Comentário de origem com link e commit + entrada em `THIRD_PARTY_NOTICES.md` |
| Jogos exportados | O player inclui só as licenças das bibliotecas linkadas; tela de licenças gerada automaticamente |

Ferramenta: `tools/licenses` gera `THIRD_PARTY_NOTICES.md` a partir de `third_party/*/LICENSE*` e falha o CI se uma pasta nova não tiver licença registrada.

## 6. Plano B por biblioteca

| Biblioteca | Gatilho de troca | Alternativa | Custo da troca graças às camadas |
|---|---|---|---|
| The Forge | S-01/S-02/S-03/S-04 reprovados no prazo, ou bloqueio grave no aparelho sem correção viável | **Diligent Engine** (Apache-2.0, CMake nativo, Android Vulkan/GLES, glslang em runtime) ou RHI Vulkan próprio enxuto | Reescrever `backends/forge`; servidores, editor e formatos intactos |
| flecs | Custo de mudanças estruturais ou de hierarquia inaceitável no S-06 | EnTT (MIT) + hierarquia própria | Reescrever `engine/world` internamente; API pública intacta |
| RmlUi | S-07 reprovado (desempenho do Inspector/árvore ou texto) | UI própria retida sobre o servidor de render (custo alto) ou ImGui com tema pesado (custo baixo, qualidade menor) | `editor/ui` muda; modelo do editor intacto |
| Luau | S-08 reprovado (bindings, depuração, desempenho) | Lua 5.4 (mesma camada de bindings) ou C# com interpretador Mono | `backends/luau` e gerador de bindings |
| Jolt | Improvável | PhysX 5 (BSD-3) | `backends/jolt` |
| miniaudio | Latência ou estabilidade no Android | Oboe (Apache-2.0) como saída + mixer próprio | `backends/miniaudio` |
| fastgltf | Falha em arquivos reais do corpus | cgltf | Importador |
| Ozz | Necessidade de compressão avançada | ACL (MIT) para compressão + runtime próprio | `backends/ozz` |
