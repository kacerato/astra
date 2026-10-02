# 13 — UI de runtime com RmlUi

Backend: **RmlUi 6.3** (MIT) + FreeType (FTL) + HarfBuzz (MIT) + LunaSVG (MIT). Referências: Unity 6.0 *UI Toolkit* (`UIDocument`, UXML/USS, *Panel Settings*) e *uGUI* (`Canvas`, `RectTransform`, `Image`, `Text`, `Button`, *Layout Groups*, *Canvas Scaler*); Godot 4.7 `Control` (âncoras, contêineres, temas).

O mesmo backend serve a **UI do editor** ([16](16-EDITOR-ARQUITETURA.md)–[18](18-EDITOR-PAINEIS-E-FLUXOS.md)). Tudo que o editor exige da RmlUi (desempenho, texto, toque) também beneficia os jogos.

---

## 1. Decisão: um runtime, duas formas de autoria

| Forma | Para quem | Como funciona |
|---|---|---|
| **UIDocument** (RML + RCSS) | Telas complexas, menus, HUDs ricos, quem conhece HTML/CSS | Asset `.rml` + `.rcss`, como o UIDocument/UXML/USS da Unity. Data binding com modelos de script |
| **Fachada Canvas** (entidades com `RectTransform`, `Image`, `Text`, `Button`…) | Quem vem do uGUI e quer montar a UI na hierarquia | Cada entidade de UI vira um elemento RmlUi gerado e mantido em sincronia. Âncoras e pivô viram posicionamento absoluto em %/px; layout groups viram flexbox |

As duas convergem no **mesmo runtime** (contextos e elementos RmlUi), sem implementações paralelas de renderização, eventos ou texto. A viabilidade da fachada (fidelidade de âncoras, desempenho de sincronia) é verificada no **S-07**. Se a fachada falhar, só o UIDocument segue, com a falta registrada como pendência.

## 2. Integração com a engine

| Interface RmlUi | Implementação Astra |
|---|---|
| `RenderInterface` (geometria compilada, texturas, scissor, transformações, máscaras de recorte, camadas, filtros e shaders da 6.x) | Pass de UI no `ForgeRenderer` via `render::World::submitUi` |
| `SystemInterface` (tempo, log, clipboard, tradução) | `foundation` + plataforma; `TranslateString` → tabela de localização |
| `FileInterface` | VFS (`res://`, `project://`) |
| Motor de fontes | FreeType por padrão; HarfBuzz para scripts complexos e ligaduras (F11) |
| Entrada | `ProcessTouchStart/Move/End/Cancel` (6.2: toque nativo e rolagem inercial), mouse, teclas e texto via GameTextInput |
| Elementos customizados | C++ (`Rml::Element`): minimapa, `RenderTexture` em UI, gráficos |

### 2.1 Contextos

- Um contexto por **UIDocument de tela** (ordenados por `sortOrder`), ou compartilhado quando configurado.
- **UI em espaço de mundo:** contexto renderizado numa `RenderTexture` aplicada a um quad; a entrada vem de raycast do toque contra o quad, convertido em coordenadas do documento.
- Escala: `dp-ratio` da RmlUi + regra de escala do projeto (constante em pixels, escala com tamanho de tela com resolução de referência e *match* largura/altura, como o *Canvas Scaler*).

### 2.2 Área segura

Insets do Android (recortes, barras de gesto) expostos como **variáveis RCSS** (`--safe-left`, `--safe-top`…), recurso da RmlUi 6.3. A fachada Canvas tem a opção "respeitar área segura" por raiz.

## 3. UIDocument

- Asset `.rml` com `<link>` para `.rcss`; templates RmlUi para componentes reutilizáveis.
- **Data binding:** o script declara um modelo (tabela tipada Luau); o ScriptHost cria o `DataModel` RmlUi com variáveis, arrays e eventos. Mudanças no modelo marcam só as variáveis sujas.
- Eventos (`data-event-click`, `onclick` via handler) → funções do script do mesmo objeto.
- Componente `UIDocument { document, styleSheets[], sortOrder, panelSettings, model }`.

## 4. Fachada Canvas (estilo uGUI)

| Componente Astra | Elemento gerado | Notas |
|---|---|---|
| `Canvas` (screen/world, sortOrder, área segura) | Contexto ou documento raiz | Render mode `ScreenSpace` e `WorldSpace`; `ScreenSpaceCamera` = adaptação (camada sobre a câmera) |
| `CanvasScaler` | Parâmetros de escala do contexto | §2.1 |
| `RectTransform` (âncoras min/max, pivô, offsets, tamanho) | `position: absolute` + left/top/right/bottom em % + px; transformações para rotação/escala | Ferramenta Rect no Scene View (modo 2D) |
| `Image` (sprite, cor, tipo `Simple/Sliced/Tiled/Filled`) | `<img>` ou decorator `image`/`ninepatch`/`tiled-*`; `Filled` via shader de recorte | `Filled` radial pode ficar como pendência se o custo não couber |
| `Text` (texto, fonte, tamanho, cor, alinhamento, quebra, overflow, rich text básico) | Elemento de texto com estilos | Rich text: subconjunto de tags documentado |
| `Button`, `Toggle`, `Slider`, `Scrollbar`, `ScrollView`, `InputField`, `Dropdown` | Elementos e controles de formulário RmlUi + estilos | Eventos `OnClick`/`OnValueChanged` → scripts |
| `HorizontalLayoutGroup`, `VerticalLayoutGroup`, `GridLayoutGroup` | Flexbox (grid via flex-wrap) | A RmlUi não tem CSS Grid |
| `LayoutElement`, `ContentSizeFitter` | `flex`, `min/max-width/height` | |
| `Mask`, `RectMask2D` | `overflow: hidden` / máscara de recorte | |
| `CanvasGroup` (alpha, interativo, bloqueia raycast) | `opacity`, `pointer-events` | |

## 5. Localização

- Asset `StringTable` (chave → texto por idioma; importa CSV/JSON).
- `TranslateString` resolve `{{chave}}`/atributo de localização no documento; a fachada tem `Text.localizationKey`.
- Idioma ativo em runtime com troca a quente; fontes de fallback por idioma (CJK, emoji) via faces de fallback.
- Plural e formatação de números/datas: pendente (F11 entrega texto simples por chave).
- RTL: depende do HarfBuzz e da direção na RmlUi; verificar e registrar.

## 6. Limites da RmlUi a respeitar

| Limite | Consequência |
|---|---|
| RCSS é um subconjunto de CSS (flexbox sim, **grid não**) | Layouts e protótipos de design usam só o subconjunto ([19](19-DESIGN-FERRAMENTAS-EXTERNAS-E-PIPELINE.md) §5) |
| Sem acessibilidade nativa | Ponte com o `AccessibilityNodeProvider` do Android fica pendente (registrada em [23](23-RISCOS-LIMITES-E-PLANO-B.md)) |
| Listas muito grandes no DOM são caras | Elementos virtualizados em C++ para listas longas (o editor depende disso, [16](16-EDITOR-ARQUITETURA.md) §4) |
| `calc()` e seletores avançados | Verificar suporte na 6.3 durante o S-07; registrar o subconjunto permitido |

## 7. Editor de UI (F11)

- Workspace **UI**: árvore de elementos, Inspector de estilos (box model visual, flex, tipografia, cores), visualização ao vivo com molduras de aparelho e proporções, alternância entre visual e código (RML/RCSS com destaque de sintaxe).
- Ferramenta **Rect** no Scene View em modo 2D para a fachada Canvas: alças de âncora, pivô e tamanho.
- Pré-visualização de idioma e de área segura.

## 8. Inventário

| Capacidade | Unity 6.0 | Godot 4.7 | Fase | Classificação planejada |
|---|---|---|---|---|
| UIDocument + estilos | UI Toolkit | Control + Theme | F11 | Equivalente (RML/RCSS no lugar de UXML/USS) |
| Canvas, RectTransform, Image, Text, Button, Toggle, Slider, ScrollView, InputField, Dropdown | uGUI | Control e derivados | F11 | Equivalente via fachada (sujeito ao S-07) |
| Layout groups, ContentSizeFitter | uGUI | Containers | F11 | Adaptação (flexbox) |
| UI em espaço de mundo | Canvas World Space | SubViewport | F11 | Equivalente |
| Localização | Localization package | TranslationServer | F11 | Parcial (sem plural) |
| Data binding | UI Toolkit runtime binding | — | F11 | Equivalente |

## 9. Aceite (F11)

- HUD com vida, munição (data binding) e botão de pausa feito por UIDocument; menu de pausa com fachada Canvas; os dois na mesma cena.
- Troca de idioma em runtime atualiza todos os textos.
- UI em espaço de mundo clicável (terminal no cenário).
- Área segura correta em aparelho com recorte de câmera, em retrato e paisagem.
- Medição: tempo de update+render da UI no Profiler, abaixo do orçamento do tier.
