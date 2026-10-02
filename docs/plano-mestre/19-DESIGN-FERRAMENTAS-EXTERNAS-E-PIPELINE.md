# 19 — Ferramentas externas de design e pipeline

Objetivo: que o design **saia das ferramentas e chegue ao aparelho sem reinterpretação manual**. Tokens, ícones e estilos têm uma única fonte, geram o código do tema e são verificados por captura.

```
Referências ─► Conceito (imagem gerada) ─► Figma (tokens, componentes, frames)
                                              │
                         ┌────────────────────┼─────────────────────┐
                         ▼                    ▼                     ▼
                 tokens.json            ícones SVG            frames aprovados
                         │                    │                     │
                 tools/tokens          SVGO + msdf-atlas-gen        │
                         ▼                    ▼                     │
         tokens.rcss · tokens.h       icons.png + icons.json        │
                         └──────────┬─────────┘                     │
                                    ▼                               │
              Protótipo RML/RCSS no Astra UI Preview (PC/aparelho)  │
                                    ▼                               │
                     Implementação no editor (RmlUi)                │
                                    ▼                               ▼
                      Captura no aparelho ──── comparação por diferença ──► ajuste
```

---

## 1. Ferramentas

| Ferramenta | Papel | Licença/conta | Observação |
|---|---|---|---|
| **Figma** | Variáveis (tokens), componentes com variantes, frames por aparelho, protótipos de fluxo | Conta do usuário | Há um **servidor MCP oficial do Figma** disponível nesta sessão (ler/escrever designs, variáveis, Code Connect). O conector `design:figma` exige autorização nas configurações de conectores do claude.ai |
| **Geração de imagem** (ChatGPT/gpt-image, Midjourney, modelos de imagem do Figma) | Conceitos visuais e exploração de direção | Conta do usuário | Conceito ≠ evidência (§6) |
| **SVGO** | Normalizar SVG de ícones | MIT | Remove cores fixas → `currentColor` |
| **msdf-atlas-gen 1.4** | Atlas MSDF de ícones | MIT | Ícones nítidos e tingíveis |
| **Inter / JetBrains Mono** | Fontes | OFL | Inter já está no repositório |
| **Lucide / Phosphor** | Base opcional para ações genéricas | ISC / MIT | Com atribuição; ícones de engine são próprios |
| **Astra UI Preview** (ferramenta própria, §5) | Rodar RML/RCSS reais com hot reload | — | Construída na F0/F4 com os backends de exemplo da RmlUi |
| **Debugger da RmlUi** | Inspecionar elementos, caixas e estilos ao vivo | MIT (plugin oficial) | Só em builds de desenvolvimento |
| **adb + scrcpy** | Captura e espelhamento do aparelho | Apache-2.0 | `adb exec-out screencap -p` |
| **Ferramenta de sobreposição** | Diferença entre captura e frame aprovado | — | Técnica herdada do protótipo do shell (tecla `R`) |
| **Verificador de contraste** | Contraste WCAG dos pares de tokens | — | Script em `tools/tokens` (mesmo cálculo de [17](17-EDITOR-DESIGN-SYSTEM.md) §3) |
| **Simulação de daltonismo** | Conferir cores de categoria e status | Plugins do Figma | — |
| **Blender** | Capas do hub, cenas de exemplo, malhas de pré-visualização | GPL (uso como ferramenta) | Os arquivos gerados são do usuário |
| **Poly Haven** | HDRIs e texturas para pré-visualização de material | CC0 | — |

## 2. Estrutura do arquivo Figma "Astra Editor 2"

| Página | Conteúdo |
|---|---|
| `00 Fundamentos` | Coleções de variáveis: **Primitivas**, **Semânticas**, **Densidade** (modos Touch/Compact), **Tipografia**, **Movimento** (documental) |
| `01 Marca` | Mark/glyph/wordmark vetorizados e comparados com os masters raster |
| `02 Ícones` | Um componente por ícone (24×24, nome `familia-nome`), variantes contorno/preenchido |
| `03 Componentes` | Catálogo de [17](17-EDITOR-DESIGN-SYSTEM.md) §7 com variantes e **todos os estados** |
| `04 Layouts` | Celular paisagem, retrato, tablet, PC, por workspace |
| `05 Painéis` | Hierarquia, Inspector, Assets, Console, Profiler, Animação… em estados vazio/normal/carregando/erro/Play |
| `06 Fluxos` | F-01 a F-10 ([18](18-EDITOR-PAINEIS-E-FLUXOS.md) §12) como protótipos clicáveis |
| `07 Conceitos` | Imagens geradas, com prompt e data (nunca misturadas com frames aprovados) |
| `08 Handoff` | Medidas, mapeamento componente Figma → template RML/classe RCSS (Code Connect quando útil) |

Convenção: nomes de componente e de variante iguais às classes RCSS (`ast-button--primary`, estado `:disabled`), para o mapeamento ser mecânico.

## 3. Pipeline de tokens

1. As variáveis do Figma são a fonte. Exportação em JSON no formato **W3C Design Tokens** via MCP (`get_variable_defs`) ou plugin de exportação.
2. Arquivo único versionado: `design/tokens/astra.tokens.json`. Ele **substitui** o `astra.tokens.json` atual, cujo acento azul diverge da marca.
3. `tools/tokens` gera:
   - `editor/ui/theme/tokens.rcss`: variáveis RCSS (recurso da RmlUi 6.3) para cores, espaços, raios e tipografia, com um bloco por densidade;
   - `engine/.../ui_tokens.h`: constantes C++ para o que o renderer desenha fora da RmlUi (gizmos, grade, contornos, moldura de Play);
   - `design/prototype/tokens.css`: para protótipos HTML de exploração;
   - relatório de contraste (falha se um par de texto essencial ficar abaixo de 4,5:1).
4. O CI confere se os arquivos gerados estão em dia com o JSON.

```json
{
  "color": {
    "accent": { "$type": "color", "$value": "{lime.400}" },
    "bg": { "panel": { "$type": "color", "$value": "{graphite.200}" } },
    "text": { "secondary": { "$type": "color", "$value": "{graphite.700}" } }
  },
  "density": {
    "touch":   { "row": { "$type": "dimension", "$value": "40dp" } },
    "compact": { "row": { "$type": "dimension", "$value": "26dp" } }
  }
}
```

```css
/* gerado: editor/ui/theme/tokens.rcss */
body {
  --color-accent: #CAFB04;
  --color-bg-panel: #15171B;
  --color-text-secondary: #9AA3AE;
  --row-height: 40dp;
}
body.compact { --row-height: 26dp; }
.ast-row { height: var(--row-height); color: var(--color-text-secondary); }
```

A sintaxe exata de variáveis RCSS (escopo e herança) é confirmada no S-07 antes de fixar o gerador.

## 4. Pipeline de ícones

1. Desenho no Figma (página `02 Ícones`), grade 24, traço 1,75, contorno e preenchido.
2. Exportação SVG (MCP ou REST) → `design/icons/svg/<familia>-<nome>[-filled].svg`.
3. **SVGO** com configuração do projeto: `viewBox="0 0 24 24"`, cores → `currentColor`, sem raster embutido, contornos convertidos quando o MSDF exigir.
4. Validação automática: tamanho, nome no inventário de [17](17-EDITOR-DESIGN-SYSTEM.md) §6.2, ausência de elementos proibidos.
5. **msdf-atlas-gen** → `icons.msdf.png` + `icons.json` (métricas, UV por nome).
6. Na engine: `IconAtlas` + decorator RCSS `icon(nome)` desenhado com shader MSDF e cor do texto/estado. A viabilidade do shader customizado no `RenderInterface` da RmlUi é item do S-07. **Plano B:** rasterizar SVG por densidade com o plugin SVG da RmlUi (com cache).
7. Folha de contato HTML gerada automaticamente para revisão (como o `catalog.html` da Astra atual).

## 5. Protótipos em RML/RCSS e o Astra UI Preview

- O design vira **os mesmos arquivos `.rml`/`.rcss` que o editor usa**. Não existe "protótipo HTML que alguém traduz depois".
- **Astra UI Preview:** executável de PC (e modo dentro do editor no aparelho) que carrega um documento RML com os tokens e ícones reais, com **hot reload** ao salvar, molduras de aparelho (celular paisagem/retrato, tablet, PC), `dp-ratio` ajustável, densidade Touch/Compact, simulação de toque pelo mouse, exportação de captura e o Debugger da RmlUi.
- No aparelho: `adb push` dos arquivos + recarregar UI (comando de desenvolvimento), sem recompilar o APK.

**Regras para quem prototipa** (pessoas e agentes):

| Regra | Motivo |
|---|---|
| Só o subconjunto RCSS suportado: flexbox sim, **grid não**; transições com tweens nomeados | Limites da RmlUi ([13](13-UI-RUNTIME-RMLUI.md) §6) |
| Cores, espaços e fontes só por variável de token | Fonte única |
| Classes `ast-<componente>[__parte][--variante]` | Mapeamento mecânico Figma ↔ RCSS |
| Listas longas usam os elementos virtuais (árvore, grade, lista) | Desempenho |
| Todo estado desenhado (vazio, carregando, erro, desabilitado com motivo, Play) | Checklist de [17](17-EDITOR-DESIGN-SYSTEM.md) §14 |

## 6. Conceitos com geração de imagem (regra do AGENTS.md visual)

**Quando:** função nova de editor, ou quando a captura real de uma tela parecer superficial.

**Entradas:** captura real da tela atual, tokens e ativos de marca, 2–3 referências de engines maduras (para princípios de descoberta, seleção e edição, **não para copiar aparência**) e a lista de propriedades e ações reais da função.

**Modelo de prompt:**

```
Interface de editor de jogos mobile, celular em paisagem 2400×1080, tema escuro grafite (#0B0C0E, #15171B),
acento limão #CAFB04 usado só em itens ativos/selecionados/ação primária, tipografia Inter com números
tabulares, ícones de contorno 1,75 px. Tela: <FUNÇÃO>. Conteúdo obrigatório: <LISTA DE PROPRIEDADES E AÇÕES
REAIS>. Hierarquia: viewport 3D dominante, painéis como gavetas laterais, uma única ação primária.
Estilo: instrumento de precisão, denso mas legível, sem texto decorativo, sem glassmorphism, sem gradientes
roxos. Estados visíveis: <ESTADOS>. Marca: mark ASTRA (quadrado limão com estrela e órbita) no canto superior esquerdo.
```

**Saídas:** `design/concepts/<função>/<data>/` com imagem, prompt, ferramenta e versão.

**Confronto obrigatório antes de codar:**

| Elemento do conceito | Existe no modelo/serialização/runtime? | Decisão |
|---|---|---|
| (cada controle, número, botão, diagnóstico) | Sim / Não / Parcial | Implementar / Remover / Pendente com motivo |

Ícones novos para conceitos novos entram no pipeline de §4. Uma imagem conceitual nunca é entregue como ícone.

## 7. Captura e comparação no aparelho

1. `tools/device/capture` tira a captura com o app em primeiro plano, registrando aparelho, resolução, densidade, tier, workspace e estado. Nada de toque às cegas: captura antes de cada toque de navegação (lição da Astra atual).
2. Frame do Figma exportado na mesma resolução lógica.
3. Sobreposição por **diferença** (o que coincide fica preto) e por opacidade ajustável. Desvios > 2 dp em posição ou tamanho são marcados.
4. Revisão pelo checklist; ajustes no RCSS/tokens; nova captura.
5. Registro em `docs/validacao/<data>-<função>/`: capturas, diferença e decisão. Cada entrega relata separadamente **conceito**, **implementado** e **testado no aparelho**.

## 8. Ciclo de aprovação

| Estado do artefato | Quem move | Critério |
|---|---|---|
| Conceito | Agente/designer | Prompt e confronto registrados |
| Proposta (frames no Figma) | Agente/designer | Todos os estados, tokens aplicados |
| **Aprovado** | **Usuário** | Revisão dos frames e do fluxo clicável |
| Implementado | Agente | RML/RCSS + lógica + consumidor real |
| Validado | Agente + usuário | Captura no aparelho dentro da tolerância + roteiro do fluxo |

## 9. Limites e dependências externas

| Item | Limite | Mitigação |
|---|---|---|
| Figma | Exige conta e autorização do conector | Sem Figma, os tokens são editados direto no JSON e os frames viram capturas do Astra UI Preview |
| Geração de imagem | Exige conta; resultados não determinísticos | Prompts versionados; conceitos são opcionais para funções pequenas |
| Shader MSDF na RmlUi | Depende do `RenderInterface` customizado | Plano B: SVG rasterizado por densidade |
| Variáveis RCSS | Sintaxe e escopo da 6.3 a confirmar | Gerador ajustado após o S-07 |
| Fidelidade da marca vetorizada | Masters são raster | Comparação por diferença de pixels, como no shell |
