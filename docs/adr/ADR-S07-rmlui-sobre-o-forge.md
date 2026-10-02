# ADR-S07 — RmlUi 6.3 sobre o The Forge para a Astra UI

- **Status:** **fatia A aprovada** (02/10/2026); fatia B pendente (abaixo). Plano B de UI (doc 03 §6) **não acionado**.
- **Pergunta (doc 22 §1):** a RmlUi 6.3 sobre o TF serve ao editor? `RenderInterface` completo (máscaras de recorte, filtros), toque inercial, IME, variáveis RCSS, Inspector de 300 campos ≤ 16 ms, árvore virtual de 10 mil, ícone MSDF por shader, mini prova da fachada Canvas.
- **Prazo:** 10 dias. Usado até aqui: 1 sessão.

## O que foi construído (`spikes/s07_rmlui`)

| Peça | Como |
|---|---|
| `ForgeRenderInterface` | Geometria em buffers do TF; texturas num *descriptor set* com 1 entrada por textura (troca barata por draw); **dados por draw** (matriz + translação) num buffer estruturado por frame, lido pelo vertex shader via `firstInstance` (o TF 1.63 não tem *push constants*); blend pré-multiplicado; liberação adiada por `kFrames` |
| Máscara de recorte | Stencil, mesmo esquema do backend GL3 da RmlUi (Set/SetInverse reescrevem com quad de tela cheia; Intersect incrementa) |
| Transformações RCSS | `SetTransform` multiplica a projeção |
| Pré-rotação (S-05) | Entra na projeção; o scissor é convertido passando os cantos pela mesma projeção (funciona para 0/90/180/270) |
| Cor | A RmlUi entrega sRGB; o vertex shader converte para linear (despré-multiplica, curva sRGB exata, pré-multiplica), porque a swapchain é sRGB |
| Fontes | FreeType 2.14.3 compilado no projeto; spike usa a fonte do sistema (Roboto); Inter entra pelo pipeline de fontes da F4 |
| Toque | `Context::ProcessTouchStart/Move/End` com os ponteiros do GameActivity |
| IME | `TextInputHandler` + GameTextInput: ao focar um campo, o teclado recebe texto e seleção atuais; cada estado do teclado substitui o texto do campo e aplica seleção e composição (índices em caracteres nos dois lados) |
| Variáveis RCSS | Tokens da Astra 2 (doc 17 §3) como `var(--…)` |

## Evidência (Xiaomi 25053PC47G, Adreno 825, Android 16, paisagem 2772×1280, dp 3,25)

| Critério | Resultado |
|---|---|
| Desenho correto | Painel Astra (cartão arredondado, faixa girada recortada, botão, lista), cores exatas (`#15171B` medido como `(21,23,27)`). Capturas em `docs/validacao/2026-10-02-s07/` |
| Máscara de recorte em uso | 7 operações de máscara por frame (contador), corte do cartão conferido em zoom |
| Validação Vulkan | Sem erros depois de corrigir 2 defeitos meus que a camada pegou (set ligado antes da pipeline; textura de altura 1 virando *view* 1D) |
| Rolagem inercial | Depois de um "flick", a lista andou mais **1066 px** sozinha |
| IME | "Porta" → teclado abre com o texto e o cursor no fim → "Porta Norte" → Concluir confirma; teclado em paisagem sem tela cheia |
| **Inspector 300 campos (Release)** | **10,95 / 11,40 / 13,02 ms** (3 rodadas; orçamento 16 ms) |
| Atualizar 1 campo (Release) | 0,09–0,1 ms (orçamento 1 ms) |
| UI na CPU por frame (Release, 228 draws) | 1,30–1,40 ms |

**Ressalva:** o orçamento do Inspector foi definido para **T2**; o aparelho é **T3**. Em T2 a montagem pode passar de 16 ms. Consequência já prevista no doc 16 §4 e agora obrigatória: Inspector e Hierarquia como **elementos virtualizados em C++** (só o visível existe no DOM), não 300 linhas de RML.

## Defeitos do upstream encontrados (patches no fork)

- `GraphicsConfig.cpp`: em **release**, toda GPU ausente do `gpu.data` era recusada ("Office preset"). Patch `forge: preset padrão de GPU também em release` (ver `third_party/ASTRA_PATCHES.md`).
- `cmake/TheForge.cmake` (nosso): o argumento condicional `--debug` do FSL virava argumento vazio em Release; corrigido com `COMMAND_EXPAND_LISTS`.

## Fatia B (pendente, mesmo prazo)

1. Camadas, filtros e shaders da RmlUi (`PushLayer`, `CompositeLayers`, `CompileFilter` para blur/drop-shadow, gradientes) — hoje esses efeitos não aparecem.
2. Ícone MSDF por shader próprio (decorator) — doc 19 §4.
3. Árvore virtual de 10 mil itens rolando no ritmo do display (elemento C++).
4. Mini prova da fachada Canvas (UI de jogo, doc 13).
5. Inspector montado por elemento C++ virtualizado, medido de novo (e em aparelho T2 quando houver).
6. `LoadTexture` de arquivo (imagens) pelo pipeline de assets.
