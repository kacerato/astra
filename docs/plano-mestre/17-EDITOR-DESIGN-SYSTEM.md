# 17 — Design system do editor: "Astra 2"

O editor tem que ter cara de produto profissional, com a eficácia das engines grandes, e mais agressivo que a Astra 1. Ele mantém o **nome ASTRA, a logo e o limão `#CAFB04`**. Este documento define a linguagem. Os painéis estão em [18](18-EDITOR-PAINEIS-E-FLUXOS.md) e o fluxo com ferramentas externas em [19](19-DESIGN-FERRAMENTAS-EXTERNAS-E-PIPELINE.md).

> Status: **proposta**. A aprovação acontece sobre os frames do Figma e as capturas no aparelho, não sobre este texto. Concepções geradas por imagem são hipóteses, nunca evidência de implementação (regra do AGENTS.md).

---

## 1. Direção

**"Instrumento de precisão."** Preto profundo, superfícies grafite, tipografia firme e números tabulares. O limão aparece **como sinal**: ativo, selecionado, primário, alterado por você. Nunca como decoração. Momentos de marca (hub, splash, Play) usam a linguagem de HUD do shell atual: colchetes de canto, quadrados de status e rótulos espaçados.

### Princípios de design

| ID | Princípio | Na prática |
|---|---|---|
| DP-01 | **Ícone antes de texto** | Barras de ferramentas só com ícones; rótulos de 1–2 palavras quando inevitáveis; nome completo ao tocar e segurar |
| DP-02 | **O valor é o herói** | Campos com números grandes e tabulares; rótulos discretos; unidade em cinza dentro do campo |
| DP-03 | **Estado sem ler** | Cor + forma + posição: barra de override, losango de keyframe, moldura de Play, selo de erro |
| DP-04 | **Viewport máximo** | No celular, painéis são gavetas e folhas por cima do viewport; nada fica fixo sem necessidade |
| DP-05 | **Uma ação primária por contexto** | Um botão limão por tela/folha; o resto é secundário ou ícone |
| DP-06 | **Toque generoso, visual denso** | Área de toque ≥ 44 dp mesmo quando o desenho é menor; densidade visual alta |
| DP-07 | **Movimento com propósito** | ≤ 220 ms, só para orientar (abrir, ancorar, confirmar) |
| DP-08 | **Nada de controle morto** | Se não funciona neste aparelho/contexto: some, ou aparece desabilitado com o motivo (P-10) |
| DP-09 | **Familiar para quem vem da Unity** | Mesmos nomes de componente e de propriedade (tooltip com o nome da API), mesma lógica de Inspector/hierarquia/prefab |
| DP-10 | **Rápido de alcançar com o polegar** | Ferramentas na lateral do polegar dominante (espelhável), ações frequentes a no máximo 1 toque |

## 2. Marca

| Ativo | Uso | Regra |
|---|---|---|
| **Mark** (quadrado arredondado limão, estrela + órbita pretas) | Barra superior (24 dp, abre o menu do app/hub), ícone do launcher, splash | Nunca como ícone de comando; área de respiro = 25% do lado |
| **Lockup** (mark + wordmark itálico com contorno) | Hub, splash, "Sobre" | Tamanho mínimo 120 dp de largura |
| **Glyph** (cometa isolado) | Silhueta de fundo do hub e de estados vazios grandes | A 4–8% de opacidade, tingida de grafite |
| **Motivos de HUD** (colchetes de canto, 3 quadrados de status, rótulo espaçado) | Hub, splash, moldura do Play, carregamento | Só em momentos de marca e estado global |

**Tarefa de marca (F4):** os masters atuais são raster (`assets/astra-visual/brand/`). É preciso redesenhar mark, glyph e wordmark em **vetor fiel** no Figma (comparação por diferença de pixels com o master, como já foi feito no shell) para obter SVGs nítidos em qualquer densidade.

## 3. Cor

Só tema escuro na v1, com variante "alto contraste" como preferência. Contrastes calculados (WCAG 2.x) sobre `bg.panel #15171B`.

### 3.1 Primitivas

| Token | Hex | Token | Hex |
|---|---|---|---|
| `graphite.0` | `#000000` | `graphite.350` | `#2A2F36` |
| `graphite.50` | `#07080A` | `graphite.400` | `#343A42` |
| `graphite.100` | `#0B0C0E` | `graphite.500` | `#4A515B` |
| `graphite.150` | `#101215` | `graphite.600` | `#7D8692` |
| `graphite.200` | `#15171B` | `graphite.700` | `#9AA3AE` |
| `graphite.250` | `#1B1E23` | `graphite.800` | `#C9D0D8` |
| `graphite.300` | `#22262C` | `graphite.900` | `#F2F4F7` |
| `lime.300` | `#E1FF5C` | `lime.400` | **`#CAFB04`** (marca) |
| `lime.500` | `#B2DE00` | `lime.a10 / a18 / a40` | `rgba(202,251,4,.10/.18/.40)` |
| `red.400` | `#FF4D5E` | `amber.400` | `#FFB224` |
| `blue.400` | `#4CB2FF` | `prefab.400` | `#5AA9FF` |
| `axis.x` | `#FF3D55` | `axis.y` | `#3BD671` |
| `axis.z` | `#3B8BFF` | `missing` | `#FF2BD6` (material de erro) |

### 3.2 Semânticos

| Token | Valor | Contraste | Uso |
|---|---|---|---|
| `bg.void` | `graphite.0` | — | Fundo atrás do viewport, letterbox |
| `bg.canvas` | `graphite.100` | — | Fundo do app |
| `bg.panel` | `graphite.200` | — | Painéis, gavetas |
| `bg.raised` | `graphite.250` | — | Cartões de componente, cabeçalhos |
| `bg.overlay` | `graphite.300` | — | Menus, folhas, tooltips |
| `bg.field` | `graphite.350` | — | Campos de entrada |
| `bg.selected` | `lime.a10` | — | Linha selecionada |
| `bg.selectedStrong` | `lime.a18` | — | Seleção ativa com foco |
| `line.subtle` / `line.default` / `line.strong` | `#24282E` / `graphite.400` / `graphite.500` | — | Divisores, bordas, contornos |
| `line.focus` | `lime.400` | 14,8:1 | Anel de foco (2 dp) |
| `text.primary` | `graphite.900` | **16,3:1** | Valores, títulos |
| `text.secondary` | `graphite.700` | **7,0:1** | Rótulos |
| `text.tertiary` | `graphite.600` | **4,9:1** | Unidades, dicas (o `#6B7480` foi descartado: 3,8:1) |
| `text.disabled` | `graphite.500` | 2,2:1 | Só desabilitado, sempre com motivo acessível |
| `text.onAccent` | `graphite.100` | **16,1:1** | Texto sobre limão |
| `accent` / `accent.hover` / `accent.pressed` | `lime.400` / `lime.300` / `lime.500` | — | Primário, ativo |
| `state.error` / `warning` / `info` | `red.400` 5,5:1 / `amber.400` 10,0:1 / `blue.400` 7,8:1 | — | Sempre com ícone de forma distinta |
| `prefab` | `prefab.400` 7,3:1 | — | Ícone e nome de instâncias de prefab (convenção Unity: azul) |
| `override` | `lime.400` | — | Barra de 2 dp à esquerda de propriedade/componente sobrescrito |
| `play.frame` | `lime.400` | — | Moldura do viewport e selo PLAY |
| `record` | `red.400` | — | Modo gravar animação |

### 3.3 Cores de categoria (leitura rápida do Inspector)

Um ponto de 6 dp no cabeçalho do cartão e o traço secundário do ícone. Nunca em texto.

| Categoria | Cor | Categoria | Cor |
|---|---|---|---|
| Renderização | `#4CB2FF` | Áudio | `#FF6FB5` |
| Física | `#FF8A3D` | Animação | `#A78BFA` |
| Navegação | `#2DD4BF` | UI | `#FACC15` |
| Script | `lime.400` | Núcleo (Transform) | `graphite.700` |

## 4. Tipografia

**Inter Variable** (OFL, já no repositório) com numerais tabulares (`tnum`) nos valores. **JetBrains Mono** (OFL) para código, console, GUIDs e caminhos.

| Estilo | Tamanho/linha (dp) | Peso | Uso |
|---|---|---|---|
| `display` | 32/36 | 700 | Só hub e splash |
| `title` | 16/20 | 600 | Título de painel e de cartão |
| `body` | 13/18 | 400 | Padrão |
| `label` | 12/16 | 500 | Rótulo de propriedade |
| `value` | 13/18 | 500, `tnum` | Valor em campo |
| `section` | 11/14 | 600, maiúsculas, espaçamento 0,08 em | Seções ("TRANSFORM", "PROJETOS"), herdado do shell |
| `micro` | 10/12 | 600 | Contadores, selos |
| `code` | 13/20 (Mono) | 400 | IDE, console |

Escala de fonte do usuário de 85% a 130%. O layout não quebra: rótulos truncam com reticências e o nome completo aparece ao tocar e segurar.

Nomes de componente ficam **em inglês** (`Rigidbody`, `MeshRenderer`), porque são nomes de API. Rótulos de propriedade são **localizados** (pt-BR/en), e a tooltip mostra o nome da API ("Massa · `mass`"). Isso liga o Inspector ao script.

## 5. Espaço, forma e elevação

| Token | Valores |
|---|---|
| Unidade | 4 dp; escala `2, 4, 6, 8, 12, 16, 20, 24, 32, 40, 48` |
| Altura de linha | `Touch` 40 dp (campo visual 32, toque 44) · `Compact` 26 dp |
| Ícones | 16 (inline), 20 (linha), 24 (barra) |
| Alvo de toque | ≥ 44 dp; ações primárias 48 dp |
| Raios | 3 (selos), 6 (campos, botões), 10 (cartões), 14 (folhas, menus), pílula (toggles, chips) |
| **Chanfro** (assinatura) | Canto cortado a 45° de 6 dp em **três** elementos: botão Play, CTA primário, ferramenta selecionada. Implementado por decorator `ninepatch` da RmlUi a partir de SVG |
| Elevação | Tonal (troca de superfície) + linha de 1 dp. Sombra só em camadas flutuantes: `0 12 32 rgba(0,0,0,.55)` (box-shadow da RmlUi 6, com cache) |
| Transparência e blur | Não por padrão (custo de GPU em mobile). Opcional em T2+ para menus |

## 6. Iconografia — "Astra Icons 2"

### 6.1 Regras

- Grade de 24 dp, área viva de 20 dp, traço de **1,75 dp**, pontas e junções arredondadas (continuidade com o sistema visual atual).
- Variante **contorno** (padrão) e **preenchida** (ativo/selecionado). Estados de hover/pressed/disabled são tokens de cor, não desenhos.
- Componentes: contorno monocromático + traço secundário na cor da categoria.
- Status nunca depende só de cor: erro (octógono), aviso (triângulo), info (círculo), sucesso (check).
- Nomes semânticos `familia-nome` (`tool-move`, `comp-rigidbody`, `asset-prefab-variant`).
- Base para ações genéricas: pode partir de **Lucide** (ISC) ou **Phosphor** (MIT), com atribuição. Conceitos de engine são desenhados do zero.

### 6.2 Inventário (≈ 200 ícones; produção em lotes por fase)

| Família | Ícones | Lote |
|---|---|---|
| Painéis/workspaces | hierarchy, inspector, scene, game, assets, console, profiler, animation, animator, mixer, navigation, history, search, settings, ui-builder, script, build | F4 |
| Ferramentas | tool-view, tool-move, tool-rotate, tool-scale, tool-rect, tool-transform, pivot-pivot, pivot-center, space-local, space-global, snap-grid, snap-vertex, snap-surface, snap-angle | F4 |
| Viewport | view-lit, view-unlit, view-wire, view-overdraw, view-normals, view-lighting, cam-persp, cam-ortho, frame-selected, gizmos, grid, stats, fly-mode, joystick, view-cube | F4 |
| Play | play, pause, step, stop, record, loop, speed | F4 |
| Ações | add, remove, duplicate, delete, rename, copy, paste, undo, redo, save, save-all, import, export, refresh, more, filter, sort, lock, unlock, eye, eye-off, pick, pick-off, link, unlink, reset, apply, revert, external, locate, favorite, trash, restore | F4 |
| Criar | entity-empty, group, cube, sphere, capsule, cylinder, plane, quad | F4 |
| Componentes — núcleo/render | transform, camera, light-dir, light-point, light-spot, light-area, probe-reflection, probe-light, mesh-renderer, skinned-mesh, lod-group, decal, particles, line, trail, volume, sky, fog, terrain, water | F4 / F7 / F13 |
| Componentes — física | rigidbody, col-box, col-sphere, col-capsule, col-cylinder, col-mesh, col-heightfield, character, joint-fixed, joint-hinge, joint-slider, joint-spring, joint-cone, joint-6dof, vehicle, wheel, phys-material | F5 |
| Componentes — outros | audio-source, audio-listener, reverb-zone, animator, constraint, ik, nav-surface, nav-agent, nav-obstacle, nav-link, nav-modifier, ui-document, canvas, rect, image, text, button, toggle, slider, scroll, input-field, dropdown, layout, behavior, onscreen-stick, onscreen-button | F6–F11 |
| Assets | folder, folder-open, scene, prefab, prefab-variant, model, mesh, material, shader, shader-graph, texture, cubemap, render-texture, audio-clip, mixer, anim-clip, anim-controller, avatar-mask, script, font, ui-doc, stylesheet, input-actions, navmesh-data, string-table, missing | F3–F11 |
| Estados | error, warning, info, success, missing, override, keyframe, keyframe-empty, locked-play, compress-pending, importing, thermal, battery | F4 |

### 6.3 Entrega técnica

SVG → atlas **MSDF** (msdf-atlas-gen) para ícones monocromáticos (nítidos em qualquer escala, tingíveis por token, um só draw). Ilustrações multicor e estados vazios usam o plugin SVG da RmlUi (LunaSVG). Ver [19](19-DESIGN-FERRAMENTAS-EXTERNAS-E-PIPELINE.md) §4.

## 7. Componentes de interface

Cada componente tem anatomia, variantes e **todos os estados**: padrão, hover (só ponteiro), pressionado, foco (anel limão de 2 dp), selecionado, desabilitado (com motivo), misto (multi-edição "—"), inválido, somente leitura, sobrescrito (prefab), animado (keyframe), carregando.

| Componente | Variantes e detalhes |
|---|---|
| Botão | Primário (limão, texto escuro, chanfro opcional), secundário (contorno), fantasma, perigo, só-ícone; com indicador de progresso |
| Botão de ícone / alternância | 36–44 dp; ativo = quadrado limão com ícone escuro |
| Controle segmentado | 2–5 opções com ícone; substitui dropdown quando há ≤ 4 opções |
| Abas | Topo de painel (sublinhado limão de 2 dp) e pílula (workspaces) |
| Chip / selo | Filtro (alternável), contagem, layer, tag |
| Campo de texto | Com ícone, limpar, erro inline |
| **Campo numérico** | Valor tabular; arrastar no rótulo ou no campo altera (horizontal); dois dedos = ajuste fino; toque duplo = digitar; expressões (`2*pi`, `+=0.5`) herdadas de `editor_numeric_expression.h` |
| **Vetor 2/3/4** | Três campos com tique colorido do eixo (sem letra "X"), arrasto por eixo, botão de travar proporção na escala |
| Slider | Trilho fino, cabo de 20 dp, valor editável ao lado |
| Toggle | Pílula; checkbox só em listas de seleção múltipla |
| Seletor (dropdown) | Folha inferior no celular, popover no tablet/PC, com busca a partir de 8 itens |
| **Campo de objeto** | Miniatura + ícone de tipo + nome + botão de escolher + soltar arrastando + "localizar"; ausente = contorno vermelho com GUID |
| Cor | Amostra + HEX; abre seletor (roda HSV, sliders, HDR em EV, conta-gotas, paletas do projeto) |
| Curva / gradiente | Prévia inline; abre editor em folha |
| Máscara de layers | Chips com contagem ("3 layers") |
| Busca | Com chips de filtro e prefixos |
| Linha de árvore | Indentação com guias, chevron, ícone, nome, selos, olho, cadeado de pick |
| **Cartão de componente** | Cabeçalho fixo ao rolar: ponto da categoria, ícone, nome, toggle de ativação, ajuda, menu ⋮; corpo com seções |
| Overlay de viewport | Barra flutuante com sombra, arrastável e encaixável nas bordas |
| Menu / menu radial | Radial: 8 direções, abre ao tocar e segurar 320 ms (herança ADR-11) |
| Folha / gaveta / diálogo | Folha inferior com alças e alturas de pouso; gaveta lateral redimensionável |
| Toast, tooltip, progresso (barra/anel), vazio, esqueleto de carregamento, breadcrumb, divisor | — |

## 8. Movimento

A RmlUi anima com funções de *tween* nomeadas (não aceita `cubic-bezier`). Por isso os tokens já apontam funções suportadas:

| Token | Duração | Tween RmlUi | Uso |
|---|---|---|---|
| `motion.instant` | 0 | — | Troca de cor de estado |
| `motion.fast` | 90 ms | `quadratic-out` | Pressionar, hover |
| `motion.base` | 160 ms | `cubic-out` | Expandir seção, trocar aba |
| `motion.panel` | 220 ms | `cubic-out` / saída `quadratic-in` | Gaveta, folha |
| `motion.emphasis` | 320 ms | `quintic-out` | Hub, entrar/sair do Play |

Preferência "reduzir movimento" zera durações (exceto progresso).

## 9. Toque e vibração

| Token | Quando |
|---|---|
| `haptic.tick` | Seleção, troca de ferramenta |
| `haptic.snap` | Encaixe de grade/vértice/superfície |
| `haptic.confirm` | Aplicar, salvar |
| `haptic.error` | Ação recusada |

Desligável nas preferências.

## 10. Densidade e pontos de quebra

| Classe | Largura (dp) | Densidade padrão | Layout ([18](18-EDITOR-PAINEIS-E-FLUXOS.md) §2) |
|---|---|---|---|
| Celular retrato (só IDE de código) | < 600 | Touch | Pilha: código + folha com abas |
| Celular paisagem | 600–839 (altura < 480) | Touch | Viewport cheio + trilho + gavetas |
| Tablet | 840–1199 | Touch | Três colunas fixas + painel inferior |
| PC / tela grande | ≥ 1200 | Compact (mouse) | Docking livre |

A densidade muda automaticamente pelo dispositivo de entrada principal (toque × mouse), com override do usuário.

## 11. Acessibilidade

- Texto essencial ≥ 4,5:1 (tabela §3.2); `text.tertiary` limitado a unidades e dicas.
- Cor nunca é o único sinal (ícones de status com formas distintas; eixos com posição fixa X/Y/Z).
- Alvo de toque ≥ 44 dp; espaço entre alvos ≥ 8 dp.
- Escala de fonte, reduzir movimento, alto contraste, modo canhoto (espelha ferramentas).
- Leitor de tela: pendente (a RmlUi não tem acessibilidade nativa; ponte com o Android registrada em [23](23-RISCOS-LIMITES-E-PLANO-B.md)).

## 12. Texto de interface

- pt-BR e en desde a F4. Botões com verbo ("Aplicar", "Criar prefab"); títulos com substantivo.
- Erro = **o quê + por quê + o que fazer** ("Não foi possível importar *casa.glb*: textura *telhado.png* não encontrada. Coloque a imagem ao lado do arquivo ou remapeie no Inspector.").
- Números sempre com unidade; nada de códigos internos para o usuário.

## 13. Elementos-assinatura (o lado "agressivo")

1. **Moldura de Play:** colchetes de canto limão nos quatro cantos do viewport + 3 quadrados de status pulsando no topo, como no shell.
2. **Ferramenta ativa** em quadrado limão chanfrado com ícone preto.
3. **Botão Play** chanfrado, limão, sempre no centro da barra superior.
4. **Barra de override** limão de 2 dp: "você mudou isto".
5. **Rótulos de seção** em maiúsculas espaçadas (TRANSFORM, FÍSICA).
6. **Hub** com a silhueta do cometa e os cartões fotográficos do shell atual, evoluídos ([18](18-EDITOR-PAINEIS-E-FLUXOS.md) §3).
7. **Números protagonistas:** campos com valor grande, tabular, e tique de eixo colorido.

## 14. Checklist de revisão por tela

- [ ] Hierarquia clara: título, conteúdo, ação primária única.
- [ ] Nenhum texto que um ícone + tooltip resolva; nenhum rótulo com mais de 2 palavras sem motivo.
- [ ] Todos os estados desenhados (vazio, carregando, erro, desabilitado com motivo, Play).
- [ ] Alvos ≥ 44 dp; ações frequentes no alcance do polegar.
- [ ] Contraste conferido; cor nunca sozinha.
- [ ] Cada controle mapeado para propriedade/ação real (modelo, serialização, runtime). Controle sem efeito sai.
- [ ] Captura no aparelho comparada com o frame aprovado ([19](19-DESIGN-FERRAMENTAS-EXTERNAS-E-PIPELINE.md) §7).
