# 01 — Visão, princípios e escopo

## 1. Missão

Criar, editar, testar e exportar jogos 3D **inteiramente no celular**, com a fluidez de uma engine grande: Inspector completo, hierarquia, prefabs, física, animação, áudio, navegação, UI e scripts. Tecnologia madura por baixo (The Forge, Jolt, Ozz, miniaudio, Recast, RmlUi, Luau). Por cima, uma API própria, estável e familiar para quem vem da Unity.

> A Astra 2 não é um "port" de engine desktop. A edição acontece onde o jogo roda: no aparelho, com toque, bateria e calor como restrições de projeto.

## 2. Para quem

| Perfil | O que espera | Consequência no plano |
|---|---|---|
| Criador mobile sem PC | Criar tudo no aparelho | Editor completo no Android; importação via seletor do sistema; export APK no aparelho |
| Dev vindo da Unity | Conceitos e nomes familiares | GameObject, Transform, Rigidbody, prefabs, Inspector com caminhos de propriedade parecidos ([05](05-MODELO-DE-OBJETOS-E-REFLEXAO.md)) |
| Indie com PC | Iterar rápido, versionar, publicar | Editor também no host Windows/Linux, texto diffável, CLI de build |
| Técnico/artista técnico | Shaders, VFX, pós | Shader Graph, materiais tipados, tiers explícitos ([08](08-RENDERIZACAO-FORGE.md)) |

## 3. Plataformas

| Papel | Plataforma | API gráfica | Quando |
|---|---|---|---|
| Alvo principal (editor + jogos) | Android 10+ (API 29+), arm64-v8a | Vulkan 1.1+ (perfis Android 15/16 pedem 1.3) | F0 em diante |
| Host de desenvolvimento | Windows 10/11 x64 | Vulkan (reativado no fork; ver S-03) | F0 em diante |
| Host secundário | Linux x64 | Vulkan | Quando houver CI Linux |
| Futuro | iOS/iPadOS | Metal (backend da 1.63, congelado) | F15, sob decisão |
| Fora | Consoles, Web, XR | — | Não planejado |

### Classes de aparelho (tiers)

Os tiers são **perfis de qualidade e de capacidade**, não promessas de FPS. Os números de orçamento entram depois da medição (ver [21](21-QUALIDADE-TESTES-PERFORMANCE.md)).

| Tier | Perfil típico | Uso |
|---|---|---|
| T0 Mínimo | Vulkan 1.1, 4 GB RAM, GPU classe Adreno 610 / Mali-G57 | Jogos simples; editor funcional com pós reduzido declarado |
| T1 Médio | 6–8 GB, Adreno 6xx alto / Mali-G68–G78 | Alvo padrão de jogos |
| T2 Alto | 8–12 GB, Adreno 7xx / Mali-G7xx / Xclipse | Editor confortável, sombras e pós completos |
| T3 Topo | 12 GB+, Adreno 8xx / Immortalis | Experimentos: VB, RT queries, DDGI |

O aparelho de validação atual (Xiaomi `25053PC47G`, 2772×1280) fica em T3. **É obrigatório adicionar ao menos um aparelho Mali e um T0/T1** antes de fechar F7 (ver L-12 em [23](23-RISCOS-LIMITES-E-PLANO-B.md)).

## 4. Princípios de engenharia

| ID | Princípio | Regra prática |
|---|---|---|
| P-01 | **API própria acima de tudo** | Gameplay, editor e formatos só enxergam tipos Astra. Nenhum `JPH::`, `Renderer*`, `ma_` ou `Rml::` fora de `backends/` |
| P-02 | **Composição universal** | Porta = objeto + transform + colisor + script. Nenhum caso especial no núcleo para uma demo |
| P-03 | **Um contrato por propriedade** | Toda propriedade nasce no TypeRegistry; Inspector, script, serialização, Undo e animação leem dali |
| P-04 | **Autor ≠ execução** | Documento de cena, mundo de Play e câmera do editor são estados distintos; Stop descarta o Play, salvo ação explícita |
| P-05 | **Nunca perder trabalho** | WAL antes de aplicar comando, gravação atômica, lixeira do projeto, export de projeto |
| P-06 | **Thread principal nunca trava > 100 ms** | Importação, cozimento, compilação, bake e I/O fora da thread principal, com progresso e cancelamento |
| P-07 | **Conteúdo do usuário não derruba o editor** | Sandbox e limite de instruções do Luau, importadores com fuzzing, validação de dados na carga |
| P-08 | **Erro que o usuário entende** | Mensagem com causa, objeto afetado e ação sugerida; nada de código de erro solto |
| P-09 | **Orçamento térmico** | Editor renderiza sob demanda (0 frames parado); tiers explícitos; ADPF para dicas de desempenho |
| P-10 | **Sem sucesso falso** | Controle sem efeito real não aparece; capacidade ausente recusa e explica |
| P-11 | **Evidência antes de afirmação** | Níveis: leitura → compilou → teste → aparelho → medido |
| P-12 | **Toque primeiro, mouse e teclado também** | Toda ação tem caminho por toque; no host, atalhos e mouse aceleram, não substituem |
| P-13 | **Dados antes de código** | Tiers, input, materiais, cenas, prefabs e configurações são assets editáveis, não `if` no código |
| P-14 | **Dependência mínima e substituível** | Biblioteca nova só com necessidade, licença verificada e um backend que a isole |

## 5. Escopo da Astra 2.0

O que precisa existir, **implementado** no sentido de [00 §5](00-INDICE.md), para a versão ser chamada de 2.0:

| Família | Mínimo para 2.0 | Fase |
|---|---|---|
| Objetos | Entity com nome, ativo próprio/efetivo, tags, layers, hierarquia ordenada, criação/destruição/duplicação, `ProcessMode` | F1 |
| Transform | Local/mundo, reparent com política de pose, ordem dos filhos, rejeição de ciclos | F1 |
| Cena e prefabs | Texto + binário, multi-cena aditiva, prefab com overrides/aninhamento/variantes/apply/revert/unpack | F1–F4 |
| Assets | `.meta`, GUID, importação glTF/FBX/imagens/áudio, cozimento, reimport, referência ausente preservada | F3 |
| Render | Câmera (persp/orto), MeshRenderer, SkinnedMeshRenderer, materiais Lit/Unlit, luzes (dir/point/spot), sombras, céu/IBL, probes, pós (bloom, tonemap, grading, AA, GTAO), partículas GPU, decals | F2, F7, F13 |
| Física | Rigidbody, colisores (box/sphere/capsule/cylinder/convex/mesh/heightfield), juntas, CharacterController, consultas, triggers, layers | F5 |
| Animação | Clipes, Animator (estados, transições, blend 1D/2D, camadas, máscaras), IK, root motion, eventos, morph targets | F8 |
| Áudio | AudioSource, AudioListener, mixer com grupos e efeitos, 3D, streaming | F9 |
| Navegação | NavMeshSurface, NavMeshAgent, Obstacle com carve, links, consultas | F10 |
| UI de jogo | UIDocument (RML/RCSS), fachada Canvas/Image/Text/Button, data binding, fontes, localização | F11 |
| Scripts | Luau com API gerada, ciclo de vida Unity-like, corrotinas, hot reload, depurador, IDE no editor | F6 |
| Input | Input Actions (actions, bindings, esquemas, processadores, interações), toque, gamepad, teclado | F2, F6 |
| Editor | Hub, workspaces, hierarquia, Inspector, Scene View, Game View, Assets, Console, Profiler, Undo/History, busca global, Play/Pause/Step | F4 em diante |
| Exportação | Player separado, pak cozido, APK de teste assinado no aparelho, AAB via CLI | F12 |

## 6. Fora do escopo da 2.0

Cada item fica registrado com motivo, para não virar dívida invisível.

| Item | Motivo | Quando reavaliar |
|---|---|---|
| Multiplayer/rede | Sistema inteiro à parte (replicação, previsão) | Depois da 2.0, com referência (Godot MultiplayerAPI / Netcode for GameObjects) |
| Física 2D e renderer 2D dedicado | A 2.0 é 3D; 2D exige outro backend (Box2D v3) e ferramentas próprias | Família própria depois de F12 |
| Lightmapping progressivo | Bake por path tracing no celular é caro; há alternativas (probes, DDGI em T3) | Depois de F13, com medição |
| Terreno com escultura completa | Grande; F13 entrega terreno por heightmap com camadas | F14 |
| Retargeting humanoide (Avatar) | Complexo; depende de Animator estável | F14 |
| C# como linguagem de script | Ver D-12 em [14](14-SCRIPTING-LUAU.md) | Se Luau falhar no S-08 ou por demanda comprovada |
| iOS | Backend Metal congelado na 1.63; exige Mac/assinatura | F15 |

## 7. Indicadores de sucesso

Viram metas numéricas só depois da primeira medição comparável (ver [21](21-QUALIDADE-TESTES-PERFORMANCE.md)).

| Indicador | Como medir |
|---|---|
| Projeto vazio → cena jogável (chão, personagem com física, câmera, uma porta com script) **só pela UI** | Cronometrado no aparelho, roteiro fixo |
| Editor parado | GPU sem frames novos (contador de present) e CPU ociosa (Perfetto) |
| Abrir projeto médio | Tempo até a primeira interação, frio e quente |
| Importar GLB de referência | Tempo, memória de pico, fidelidade contra o Khronos glTF Sample Viewer |
| Play/Stop 100× | Sem vazamento (RSS e memória Vulkan estáveis), sem erro de validação |
| Sessão de 30 min de edição | Temperatura e estado térmico ADPF, sem throttling severo em T2 |

## 8. Barra de qualidade

Herdada da Astra atual (`docs/CONVENCOES.md §8`), sem exceções:

1. Nunca perder trabalho do usuário.
2. Nunca travar a thread principal por mais de 100 ms.
3. Nunca crashar o editor por culpa do conteúdo do usuário.
4. Nunca mostrar um erro que o usuário não entenda.
5. Nunca esquentar o aparelho além do orçamento térmico.
