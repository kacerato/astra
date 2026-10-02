# 04 — Arquitetura, camadas e repositório

## 1. Camadas

```
┌───────────────────────────────────────────────────────────────────────────┐
│ Aplicações: editor (Android/host), player (jogo exportado), ferramentas   │
├───────────────────────────────────────────────────────────────────────────┤
│ Editor: documento, comandos, Undo/WAL, AssetDatabase, importadores,       │
│         plugins, Astra UI (painéis)                                        │
├───────────────────────────────────────────────────────────────────────────┤
│ Astra API: Entity, Component, Transform, Resource, Prefab, Scene, Input,  │
│            componentes built-in (Camera, Light, MeshRenderer, Rigidbody…)  │
├───────────────────────────────────────────────────────────────────────────┤
│ Servidores: RenderWorld · PhysicsWorld · AudioWorld · AnimationWorld ·    │
│             NavWorld · UIWorld · ScriptHost      (handles opacos, filas)   │
├───────────────────────────────────────────────────────────────────────────┤
│ Backends: forge · jolt · miniaudio · ozz · recast · rmlui · luau · null    │
├───────────────────────────────────────────────────────────────────────────┤
│ Núcleo: foundation (memória, jobs, VFS, log, math, tempo, plataforma),    │
│         object (TypeRegistry, Variant, Resource), world (flecs)            │
└───────────────────────────────────────────────────────────────────────────┘
```

A fronteira **Servidor ↔ Backend** é a que permite trocar tecnologia, no mesmo papel que os *servers* da Godot (RenderingServer, PhysicsServer3D, AudioServer, NavigationServer3D). Cada servidor tem **um** backend linkado por build; não há despacho virtual por chamada no caminho quente. O backend `null` existe porque há consumidores reais: testes headless e ferramentas de linha de comando.

## 2. Módulos

| Módulo | Responsabilidade | Pode depender de | Nunca depende de |
|---|---|---|---|
| `foundation` | Tipos base, memória, contêineres, strings, math, log, asserts, tempo, jobs, VFS, perfil, plataforma abstrata | (nada da engine) | qualquer outro módulo |
| `object` | TypeRegistry, Variant, PropertyInfo, Object, Resource, sinais, UID | foundation | world, servidores |
| `world` | World (flecs), Entity, hierarquia, ProcessMode, ciclo de vida, Transform, pipeline de sistemas | foundation, object | servidores, backends |
| `servers/*` | APIs de RenderWorld, PhysicsWorld etc.: handles, descritores, comandos, consultas | foundation, object | world (servidor não conhece entidade), backends |
| `components` | Componentes built-in + sistemas de sincronização componente → servidor | world, servers/* | backends |
| `scene` | Serialização texto/binário, prefabs, carga de cena, migração | object, world, components | backends, editor |
| `resources` | ResourceLoader/Saver, cache, formatos cozidos | object, foundation | editor |
| `input` | Dispositivos, estado, Input Actions | foundation, object | world |
| `script` | ScriptHost: ciclo de vida de comportamentos, ponte de reflexão | world, object, components | backend concreto (fala com `backends/luau` via interface do servidor) |
| `backends/<x>` | Implementação de um servidor sobre uma biblioteca | servers/<x>, foundation, a biblioteca | world, components, editor |
| `editor/*` | Documento, comandos, UI, painéis, importadores, plugins | tudo acima | — |
| `player` | Composição mínima para rodar um jogo exportado | tudo menos editor | editor |
| `platform/<os>` | Ponto de entrada, ciclo de vida, janela/surface, IME, armazenamento, térmico | foundation | world, editor |

### Verificação automática

- No CMake, cada módulo é um alvo com `target_link_libraries(... PRIVATE ...)` declarado a partir de uma tabela única em `cmake/AstraModules.cmake`. Uma dependência fora da tabela falha a configuração.
- `tools/check-layering` (script do CI) falha se algum arquivo fora de `backends/<x>` incluir headers de terceiros (`Jolt/`, `IGraphics.h`, `miniaudio.h`, `RmlUi/`, `lua.h`, `ozz/`, `Detour*`, `flecs.h` fora de `world`).
- Headers públicos de servidores compilam num teste que **não** tem o include path de nenhuma biblioteca de terceiros.

## 3. Do gameplay ao backend: dois exemplos completos

### 3.1 Física

```cpp
// Gameplay em C++ (API Astra)
astra::Entity crate = world.create("Crate");
auto& body = crate.add<astra::RigidBody>();     // adiciona dependências obrigatórias (ComponentInfo::requires)
body.setMass(20.0f);                             // setter marca a propriedade como alterada
body.setLinearDamping(0.05f);
body.addForce({0.0f, 100.0f, 0.0f}, astra::ForceMode::Force);   // enfileira um comando
```

```lua
-- Gameplay em Luau (mesmo contrato, bindings gerados)
local body = self.entity:GetComponent(RigidBody)
body.mass = 20
body.linearDamping = 0.05
body:AddForce(Vector3.new(0, 100, 0))
```

Caminho interno:

```
RigidBody (componente, dados + handle BodyId)
   │  setter → marca "alterado" (contador de geração por componente)
   ▼
PhysicsSyncSystem (fase PrePhysics, ponto seguro)
   │  converte dados Astra → descritores do servidor; aplica fila de forças
   ▼
physics::World  (servidor: createBody/setMass/addForce/step/raycast…, handles opacos)
   ▼
JoltPhysicsBackend (backends/jolt: JPH::BodyInterface, camadas, listeners)
   ▼
Jolt
```

### 3.2 Renderização

```
MeshRenderer (componente: Mesh, materiais[], flags de sombra, layer, bounds)
   ▼  RenderSyncSystem cria/atualiza RenderInstance quando algo muda
render::World  (servidor: instâncias, luzes, câmeras, probes, decals, partículas, debug draw)
   ▼  extração por frame → FramePacket imutável
ForgeRenderer (backends/forge: frame graph, passes, pipelines, descriptors)
   ▼
The Forge (Vulkan)
```

**Regra:** o servidor nunca lê o ECS. Os sistemas de sincronização em `components/` empurram dados para ele. Assim o mesmo servidor atende o editor, o player, ferramentas e testes.

## 4. Handles e tempo de vida

- `Handle<T>` = índice de 32 bits + geração de 32 bits num `u64` (geração curta, como 8 bits, dá a volta rápido e reabre o problema ABA). Pools com reaproveitamento de slot e incremento de geração. Handle inválido é detectado sem ponteiro pendurado.
- Entidades usam o id do flecs (64 bits, com geração) **somente** dentro de `world`. A API pública expõe `astra::Entity` (id + ponteiro do `World` + verificação de vida).
- Referências persistentes (cena, prefab, asset) **nunca** usam handles de runtime: usam `LocalId` (64 bits por objeto no arquivo) e `AssetGuid` (128 bits). Ver [06](06-CENA-PREFABS-SERIALIZACAO.md) §2.
- Handles não atravessam sessões: ao parar o Play, todos os handles do mundo de Play são invalidados, e quem os guardou (scripts do editor, painéis) recebe "referência inválida", nunca o objeto errado.

## 5. Threads

| Thread | Responsabilidade | Observação |
|---|---|---|
| **Principal (game/editor)** | Eventos de plataforma, input, scripts, pipeline de sistemas, comandos do editor, UI (RmlUi) | No Android, é a thread nativa da GameActivity, não a thread Java |
| **Render** | Consome o `FramePacket` do frame N−1, grava e submete comandos | Gravação paralela de passes grandes via jobs |
| **Workers (jobs)** | Física (Jolt), animação, culling, sistemas paralelos do flecs, importação, cozimento | N = núcleos grandes + médios − 1, configurável por tier |
| **I/O** | Threads do Resource Loader (TF) + leitura assíncrona da VFS | Prioridade baixa; cancelamento |
| **Áudio** | Callback do device miniaudio | Tempo real: sem alocação nem lock bloqueante; comandos por fila sem lock |
| **Java/UI do Android** | Activity, IME, seletores (SAF), diálogos do sistema | Comunicação por filas; nenhuma chamada JNI por frame no caminho quente |

### Sistema de jobs (D-19)

- API mínima: `jobs::submit(fn, priority)`, grupos com contador, `parallelFor(range, grain, fn)`, espera cooperativa (quem espera executa outros jobs).
- Implementação própria com filas por thread e roubo de trabalho, sobre as primitivas de thread da `foundation`.
- Adaptadores: `JPH::JobSystemWithBarrier` para o Jolt; `ecs_os_api` task threads para o flecs; importação e cozimento como jobs de baixa prioridade.
- Afinidade: dicas de núcleo via `APerformanceHint` (ADPF) para a thread principal e a de render; sem fixar núcleo à força.

## 6. Loop de frame do runtime

Fases do pipeline de sistemas (implementadas como fases customizadas do pipeline do flecs):

| # | Fase | O que acontece | Equivalente Unity (referência U9) |
|---|---|---|---|
| 1 | `Platform` | Eventos de janela/ciclo de vida, surface, IME | — |
| 2 | `Input` | Atualização de dispositivos e actions | Input System update |
| 3 | `Time` | `deltaTime`, `timeScale`, acumulador fixo | Time |
| 4 | `Initialization` | `Awake`/`OnEnable`/`Start` pendentes (objetos criados no frame anterior) | Initialization |
| 5 | `FixedUpdate ×k` | Scripts `FixedUpdate` → `PhysicsSync` → `physics.step` → eventos de trigger/colisão → write-back de poses | FixedUpdate, Internal physics update, OnTrigger/OnCollision |
| 6 | `Update` | Scripts `Update`, corrotinas, tweens/timers | Update, yield |
| 7 | `Animation` | Animator (estados, parâmetros) → jobs Ozz → root motion → constraints | Internal animation update |
| 8 | `Navigation` | Crowd do Detour, atualização de agentes | — (NavMeshAgent interno) |
| 9 | `LateUpdate` | Scripts `LateUpdate` | LateUpdate |
| 10 | `Transform` | Propagação de matrizes de mundo por dirty flags | — |
| 11 | `AudioSync` | Posições de listener/fontes, comandos para o miniaudio | — |
| 12 | `UI` | Layout e eventos da RmlUi para os UIDocuments | — |
| 13 | `RenderSync` | Componentes → `render::World` (só o que mudou) | — |
| 14 | `Extract` | `render::World` → `FramePacket` imutável → thread de render | Rendering |
| 15 | `Cleanup` | Destruições adiadas, mudanças estruturais pendentes, `OnDestroy` | OnDestroy |

- **Interpolação de física:** corpos com `interpolation = Interpolate` são renderizados com pose interpolada entre os dois últimos passos fixos (mesma semântica do `Rigidbody.interpolation`).
- **Pontos seguros:** mudanças estruturais (adicionar/remover componente, reparent, destruir) feitas por scripts no meio de uma fase são enfileiradas e aplicadas no fim da fase, como o *deferred mode* do flecs.
- **Diferenças com a Unity** ficam registradas no doc de scripting ([14](14-SCRIPTING-LUAU.md) §4) como "adaptação explícita".

## 7. Loop do editor

- O mundo de edição roda só os sistemas de edição: `Transform`, `RenderSync`, `Extract`, gizmos, pré-visualizações (animação, partículas) quando ativadas.
- **Render sob demanda:** o editor só pede frame quando há motivo (input no viewport, propriedade alterada, pré-visualização tocando, import concluído, Play rodando). Parado, 0 frames de GPU. Isso é requisito de bateria e calor (P-09).
- Durante o Play, os dois mundos coexistem: o de edição para de atualizar e o de Play roda o pipeline completo.

## 8. Memória

| Item | Regra |
|---|---|
| Alocadores | Heap geral com tags; linear por frame (reset a cada frame); pools por tipo de componente (flecs gerencia o armazenamento); arenas por importação |
| Tags | `Core, World, Render, RenderGPU, Physics, Audio, Anim, Nav, UI, Script, Assets, Editor, Import` |
| Relato | Painel de memória com uso e pico por tag, mais memória Vulkan por categoria (buffer, textura, render target, staging) |
| Orçamentos | Definidos por tier **depois** da medição da F2/F7; alarme a 80%, recusa explícita acima do limite (ex.: importar textura que estoura o orçamento avisa antes) |
| Pressão do sistema | `onTrimMemory` do Android → liberar caches (thumbnails, mips não visíveis, bytecode de debug) |

## 9. Erros, log, asserts e crash

- `Result<T, Error>` com `Error { code, message, context }`; mensagens para o usuário saem de um catálogo (localizável) com causa e ação.
- Log estruturado: nível, canal (`render`, `physics`…), objeto (`LocalId`/`AssetGuid`), mensagem. Console do editor filtra por canal e objeto.
- `ASTRA_ASSERT` (debug), `ASTRA_VERIFY` (sempre avalia), `ASTRA_FATAL` (para o processo com relatório).
- Crash no Android: handler de sinal grava o último trecho do log e garante o `fsync` do WAL. Na próxima abertura, o hub oferece recuperar e mostra o relatório.

## 10. Sistema de arquivos virtual

| Esquema | Aponta para | Escrita |
|---|---|---|
| `res://` | Conteúdo empacotado do jogo/editor (APK assets, pak) | Não |
| `project://` | Raiz do projeto aberto | Sim (editor) |
| `user://` | Dados persistentes do jogo (saves, configurações) | Sim |
| `cache://` | `Library/` do projeto ou cache do app | Sim, descartável |
| `temp://` | Temporários da sessão | Sim |

Implementado sobre o FileSystem do TF (disco, memória, zip e AAssetManager). Caminhos sempre com `/`, UTF-8, sensíveis a maiúsculas (o Android é).

## 11. Estrutura do repositório

```
astra/
├── AGENTS.md                      regras para agentes (derivadas das atuais, com o mapa novo)
├── CMakeLists.txt  CMakePresets.json
├── cmake/                         AstraModules.cmake, toolchains, opções, avisos
├── engine/
│   ├── foundation/                core, memory, containers, math, jobs, vfs, log, time, profile
│   ├── object/                    type_registry, variant, property_info, object, resource, signal, uid
│   ├── world/                     world (flecs), entity, hierarchy, transform, process_mode, lifecycle
│   ├── servers/                   render/, physics/, audio/, animation/, navigation/, ui/, script/
│   ├── components/                camera, light, mesh_renderer, rigidbody, colliders, audio_source, animator…
│   ├── scene/                     text_format, binary_format, prefab, scene_loader, migration
│   ├── resources/                 loader, saver, cache, formats/
│   ├── input/                     devices, actions
│   └── script/                    script_host, behavior lifecycle, reflection bridge
├── backends/                      forge/ jolt/ miniaudio/ ozz/ recast/ rmlui/ luau/ null/
├── platform/                      android/ windows/ linux/
├── editor/
│   ├── core/                      document, commands, undo_redo, wal, selection, asset_database, plugin
│   ├── ui/                        Astra UI: custom elements, dock, property grid, theme (RCSS)
│   ├── panels/                    hierarchy, inspector, scene_view, game_view, assets, console, profiler…
│   ├── plugins/                   inspectors e gizmos por componente, previews
│   └── importers/                 gltf, fbx, texture, audio, script
├── player/                        main do jogo exportado
├── apps/
│   ├── android-editor/            Gradle (AGP) do editor
│   ├── android-player/            Gradle-modelo do jogo exportado
│   └── host-editor/               executável Windows/Linux
├── shaders/                       FSL das passes internas
├── tools/                         cooker CLI, shader build, api dump, bindings, licenses, check-layering
├── design/                        tokens, ícones SVG, atlas gerado, protótipos RCSS (ver doc 19)
├── third_party/                   bibliotecas fixadas + VERSIONS.md + ASTRA_PATCHES.md
├── tests/                         unit/, integration/, golden/, device/, fuzz/
└── docs/                          adr/, plano-mestre/ (estes documentos), referências
```

## 12. Build

| Item | Decisão |
|---|---|
| Gerador | CMake ≥ 3.28 + Ninja; presets `host-debug`, `host-release`, `host-asan`, `android-arm64-debug`, `android-arm64-profile`, `android-arm64-release` |
| Compiladores | clang do NDK (Android); clang-cl ou MSVC 2022 (Windows); clang/gcc (Linux) |
| Padrão | C++20 no código Astra; C99/C11 nas bibliotecas C |
| Avisos | `-Wall -Wextra -Werror` só nos alvos Astra; terceiros com avisos silenciados |
| Android | NDK r28+ (alinhamento de 16 KB), AGP 8.5.1+, `targetSdk 36`, `minSdk 29`, `abiFilters arm64-v8a` |
| Shaders | Passo de build chama o toolchain FSL (Python) → SPIR-V em `build/shaders`, empacotado em `res://shaders` |
| Código gerado | `tools/api-dump` (host) exporta `api.json` do TypeRegistry → gera `.d.luau`, docs de API e thunks C++ de alto desempenho para os bindings |
| Cache | sccache/ccache opcional; nenhuma etapa depende de rede (build offline reproduzível, lição da Astra atual) |
| Gradle | Só empacota: chama o preset CMake, copia `.so`, assets e shaders. O AGDE/VS2019 do TF não é usado |

## 13. Convenções de código

- Namespaces `astra::`, `astra::render`, `astra::physics`…; tipos `PascalCase`, funções e variáveis `camelCase`, constantes `kPascalCase`.
- Header público mínimo; detalhes em `*_impl.h`/`.cpp`. Sem `using namespace` em header.
- Comentários explicam contrato, ownership, unidades e thread, não o óbvio.
- Toda função que cruza thread documenta em qual thread pode ser chamada (`// thread: main` / `any` / `render`).
- Unidades no nome ou no tipo quando houver ambiguidade (`angleDeg`, `Seconds`).
