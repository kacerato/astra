# 02 — Herança da Astra atual

A engine atual (`atchengine`) tem cerca de 142 mil linhas nativas próprias e 25 mil gerenciadas (contagem em `native/` e `managed/`, sem `third_party`, bin e obj). Ela custou muito aprendizado. Este documento separa **o que vira conhecimento para a Astra 2** do que fica para trás. A regra é simples: **leva-se a decisão e a lição, não o código**. Se um trecho for reaproveitado, ele passa por revisão como qualquer código novo.

Fonte: leitura de `docs/adr/`, `docs/design/`, `docs/CONVENCOES.md`, `docs/ESTADO.md`, `native/scene/component_schema.h` e dos planos em `docs/planos/` em 01/10/2026.

---

## 1. Conceitos e decisões que continuam valendo

| Herança | Origem na Astra atual | Como entra na Astra 2 |
|---|---|---|
| **Formato duplo texto + binário** do mesmo modelo, com ids de campo persistentes e aliases para renomear | ADR-07; `TextSerializer.cs`, `BinarySerializer.cs` | D-09. Texto determinístico é a verdade; binário é derivado e descartável ([06](06-CENA-PREFABS-SERIALIZACAO.md)) |
| **WAL de edição** com `fsync` antes de aplicar, checksum por registro e recuperação até o último registro válido | ADR-12; `WriteAheadLog.cs` | Reimplementado em C++ no núcleo do editor ([16](16-EDITOR-ARQUITETURA.md) §6) |
| **Gravação atômica** (temporário → fsync → rename → fsync do diretório) | `ASTRA-SHELL-UI.md` (índice de projetos) | Regra de todo arquivo de projeto ([06](06-CENA-PREFABS-SERIALIZACAO.md) §8) |
| **Mutabilidade em Play por propriedade e por estrutura** (`Never`, `SafePoint`) | `component_schema.h`: `PlayMutability`, `structuralInPlay`, `propertiesInPlay` | Vira campo do `PropertyInfo` e do `ComponentInfo` ([05](05-MODELO-DE-OBJETOS-E-REFLEXAO.md) §5) |
| **Composição com regras** (dependências, conflitos, plano de composição atômico) | `ComponentRule`, `ComponentCompositionPlan` | `ComponentInfo.requires/conflicts`, comando composto atômico ([05](05-MODELO-DE-OBJETOS-E-REFLEXAO.md) §6) |
| **Pipeline único escalável** (sem URP/HDRP) | ADR-10 | D-15: um renderer, tiers por dados |
| **Menu radial** por toque-e-segura como substituto dos atalhos | ADR-11 (protótipo validado, não produto) | Comando de contexto do viewport e da hierarquia ([18](18-EDITOR-PAINEIS-E-FLUXOS.md) §4.6) |
| **AST única** para NoCode: blocos, grafo e código como vistas da mesma árvore | ADR-06 (AetherFlow) | Visual scripting da F14 gera **Luau** a partir da mesma AST ([14](14-SCRIPTING-LUAU.md) §12) |
| **Play isolado do editor** | ADR-08 (decidido, nunca implementado) | D-14: mundo separado no mesmo processo; processo separado reavaliado com critério ([16](16-EDITOR-ARQUITETURA.md) §9) |
| **Dados do projeto ficam com o usuário** | ADR-09 | Mantido. O empacotamento do APK de teste passa a ser **no aparelho** (não exige NDK, só zip + assinatura); AAB e loja vão para o PC ([20](20-BUILD-EXPORTACAO-E-DISTRIBUICAO.md)) |
| **Paridade por inventário Unity**, item por item, com caminho de Inspector | Memória `completude-por-referencia-unity`, galeria `docs/referencias/unity-visual-2026-09-26/` | Cada família tem uma tabela de inventário com link Unity 6.0/Godot 4.7 e classificação |
| **Atlas de referências** Unity/Godot | `docs/planos/ampliacao-2026-09-23/` | Usado como universo de pesquisa, não como lista de implementados |

## 2. Lições do aparelho (Android)

São lições que custaram horas. Viram requisitos ou testes da Astra 2.

| Lição | Requisito na Astra 2 |
|---|---|
| O sistema mata o processo sem aviso | WAL + gravação atômica + retomada do estado do editor (cena aberta, câmera, seleção) |
| Desinstalar o app **apaga os projetos** em `Android/data` | Exportar/importar projeto em `.zip` via seletor do sistema; alerta no hub; nunca sugerir desinstalar ([15](15-INPUT-E-PLATAFORMA-ANDROID.md) §6) |
| Teclado desenhado não tem composição, seleção nem clipboard; em paisagem o IME abre em tela cheia | Entrada de texto real via **GameTextInput** (AGDK) com `IME_FLAG_NO_EXTRACT_UI` ([15](15-INPUT-E-PLATAFORMA-ANDROID.md) §4) |
| Driver Adreno sem descriptor indexing exigiu fallback do bindless | Toda feature opcional passa por capability e tem caminho alternativo testado ([08](08-RENDERIZACAO-FORGE.md) §10) |
| `VK_EXT_memory_budget` ausente no aparelho de teste | Orçamento de memória próprio, sem depender da extensão |
| APK debug com camada de validação Vulkan pegou erros reais (`independentBlend`, FSR ausente) | Validação ligada em todo debug; `logcat` de erros de validação faz parte do gate de cada fase |
| GameTurbo, Game Mode e estados de clock invalidam medições | Bancada de medição com estado controlado ([21](21-QUALIDADE-TESTES-PERFORMANCE.md) §6) |
| Toques "às cegas" por ADB erram (listas mudam de ordem) | Automação por ids semânticos e captura antes de cada toque ([21](21-QUALIDADE-TESTES-PERFORMANCE.md) §5) |
| Margem de toque escondia sobreposição de widgets | Teste de toque dentro do pai e inspeção do retângulo de toque no modo debug da UI |
| Clang do NDK pega avisos que o GCC do host não pega | CI compila Android em todo PR que toca `engine/` |
| 100 retomadas + mudança de configuração revelaram bugs de surface | Gate de ciclo de vida em toda fase com renderização ([21](21-QUALIDADE-TESTES-PERFORMANCE.md) §4) |
| Wi-Fi ADB tem serial com espaço; o transport id muda | Scripts de dispositivo tratam serial por tabulação e listam antes de usar |
| Release e debug com a mesma chave permitem `install -r` sem apagar dados | Mantido para os APKs do editor |

## 3. Lições de renderização e desempenho

| Lição | Uso |
|---|---|
| FrameProfile com CPU, GPU e memória na mesma janela; `/proc` lido raramente | Modelo do painel Profiler e do relatório de captura ([21](21-QUALIDADE-TESTES-PERFORMANCE.md) §7) |
| Arm ASR e FSR 2 testados como temporais; o chip de modo alternava a configuração | Upscaler temporal como opção de tier com estado explícito no editor ([08](08-RENDERIZACAO-FORGE.md) §7) |
| Oceano FFT em cascatas com Jolt | Família Água da F13 reaproveita o **conhecimento** (cascatas, empuxo) com dados próprios |
| Mapas grandes (estrada de terra) exigiram vértices compactos (ADR-015) | Quantização via meshoptimizer e formatos cozidos compactos ([07](07-RECURSOS-E-PIPELINE-DE-ASSETS.md) §6) |

## 4. Gameplay e componentes que valem reimplementar

Os planos recentes cobriram utilitários que os criadores usam muito. Eles entram como **famílias** da Astra 2, com contrato novo:

| Família | Planos de origem | Fase na Astra 2 |
|---|---|---|
| Input profiles, captura, interações (estilo Input System) | `INPUT-*.md` | F2 (núcleo) + F6 (assets de actions) |
| Tween, Timer, curvas e gradientes | `TWEEN-*`, `TIMER-*`, `CURVAS-GRADIENTES-*` | F6 (API de script) + F8 (curvas compartilhadas com animação) |
| Paths, splines (Curve3D, PathFollow) | `PATHS-*`, `SPLINE-FRAME-*`, `P15A-*` | F6 |
| Constraints (look at, parent, position…) | `CONSTRAINTS-*` | F8 (avaliadas no passo de animação) |
| Character (chão, plataformas, estados) | `CHARACTER-*` | F5 (CharacterController sobre Jolt `CharacterVirtual`) |
| Consultas físicas, conexões, mecanismos | `CONSULTAS-FISICAS-*`, `PHYSICS-CONNECTIONS-*`, `MECANISMOS-*` | F5 |
| Save, Time, Audio API | `API-SAVE-*`, `API-TIME-*`, `AUDIO-*` | F6/F9 |
| Prefab: overrides seletivos, apply com propagação, comparação com a fonte | `P-OVERRIDES-*`, `PREFAB-*` | F4 (semântica completa desde o início) |

## 5. Identidade visual que continua

| Ativo | Caminho atual | Uso na Astra 2 |
|---|---|---|
| Logo (mark), lockup e wordmark | `assets/astra-visual/brand/` | Mesmos ativos; versão vetorial redesenhada fiel ao raster ([17](17-EDITOR-DESIGN-SYSTEM.md) §2) |
| Verde-limão `#CAFB04` medido no mark | `docs/design/ASTRA-SHELL-UI.md` | Cor de acento da Astra 2 |
| Telas de referência do shell (splash, carregamento, projetos) | `assets/astra-visual/reference/screen-0*.png` | Base do Hub da Astra 2, com evolução |
| Inter Variable (OFL) | `assets/astra-visual/fonts/` | Fonte de UI |
| Motivos de chrome: colchetes de canto, quadrados de status, rótulos espaçados | `astra.tokens.json → chrome` | Linguagem "HUD" do novo design system |
| Proposta de layout aprovada (3 colunas, Inspector à direita) | `docs/editor-layout/proposta-aprovada.png` | Ponto de partida do layout tablet/desktop, superado no celular |

> Atenção: `astra.tokens.json` tem hoje `accent: #70ACF5` (azul), enquanto o shell e a marca usam `#CAFB04`. A Astra 2 resolve isso com **um único** arquivo de tokens, gerado do Figma ([19](19-DESIGN-FERRAMENTAS-EXTERNAS-E-PIPELINE.md) §3).

## 6. O que fica para trás (e por quê)

| Item | Motivo |
|---|---|
| RHI Vulkan e render graph próprios (`native/rhi`, `native/rendergraph`) | Substituídos pelo The Forge + frame graph fino da Astra 2. O conhecimento de capabilities fica |
| Runtime C# (CoreCLR via hostfxr) + Roslyn no aparelho | Funcionou, mas custa memória, tamanho de APK, latência de compilação e bloqueia iOS. D-12 adota Luau |
| UI do editor desenhada à mão (`editor_screen.*`, faixas de id de widget com `static_assert`) | Substituída pela Astra UI sobre RmlUi, retida e declarativa |
| `EditorEntity` com arrays fixos de água no objeto genérico | Violava a composição universal; na Astra 2 água é família própria |
| Shaders SPIR-V embutidos em headers (`*_spirv.h`) | Substituídos por compilação FSL offline + cache de pipelines |
| Godot empacotado no APK como segundo editor | Fora; Godot é referência de estudo |
| Templates de cena de demonstração no núcleo | Fora; amostras vivem em projetos de exemplo |
| P/Invoke escrito à mão por subsistema (dívida do IDL) | Fora; bindings Luau são **gerados** do TypeRegistry |

## 7. Como estudar a engine atual sem se prender a ela

1. Congelar uma cópia somente leitura (`git worktree` ou clone separado) e registrar o commit.
2. Antes de implementar uma família na Astra 2, ler o plano correspondente da Astra atual e anotar na tarefa: **o que deu certo, o que quebrou no aparelho e qual teste pegou o problema**.
3. Não copiar arquivos. Quando um algoritmo valer (ex.: parsing de expressões numéricas do Inspector, `editor_numeric_expression.h`), reescrevê-lo no estilo novo, com teste novo.
4. Projetos de exemplo (`games/`, `samples/`) servem como **corpus de validação** para a importação da F3, convertidos para o formato novo por um conversor de uso único, nunca por compatibilidade permanente.
