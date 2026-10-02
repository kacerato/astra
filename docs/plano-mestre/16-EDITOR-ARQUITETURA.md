# 16 — Arquitetura do editor

Referências estudadas e o que se aproveita de cada uma:

| Referência | O que estudar | O que a Astra 2 adota |
|---|---|---|
| **ezEngine** (release 26.9) — *Editor Documents*, *Editor Plugins*, processo `ezEditorEngineProcess` | Modelo de documento, acesso a objetos sempre via *accessor*/comandos, histórico de comandos, processo de engine separado, processamento de assets em segundo plano | Mutação **só por comandos**; documentos com histórico próprio; assets em fila de jobs. O processo separado fica como evolução (§10.4) |
| **Godot 4.7** — `EditorPlugin`, `EditorInspector`, `EditorProperty`, `EditorInspectorPlugin`, `EditorUndoRedoManager`, telas principais | Inspector gerado da reflexão, widgets por tipo/dica, plugins que registram docks, gizmos, importadores e telas | Inspector gerado do TypeRegistry; registro de editores de propriedade; sistema de plugins; Undo por documento |
| **Unity 6.0** — `SerializedObject`/`SerializedProperty`, `Undo`, `EditorTool`, Overlays, Prefab Mode, Play Mode | Edição sobre representação serializada (multi-objeto, overrides), ferramentas de cena, overlays no viewport | `PropertyPath` + `Variant` como "propriedade serializada"; ferramentas e overlays; semântica de Play/Stop |
| **Stride** — Game Studio (asset view, property grid, prefabs) | Organização de assets e property grid | Detalhes de UX do Asset Browser |

---

## 1. Visão geral

```
┌ Aplicação do editor ────────────────────────────────────────────────────────┐
│ Hub (projetos) · Workspaces (Cena, Animação, Materiais, Script, UI, Perfil, │
│ Build) · barra superior · paleta de comandos · notificações · tarefas        │
├ Núcleo do editor (editor/core) ─────────────────────────────────────────────┤
│ EditorContext · Documentos · Seleção · Comandos/Transações/Undo · WAL ·      │
│ AssetDatabase · Importadores · Plugins · Configurações · Busca global        │
├ Astra UI (editor/ui, sobre RmlUi) ──────────────────────────────────────────┤
│ Dock, abas, gavetas, folhas, árvore virtual, grade de propriedades, campos,   │
│ seletores, curvas, gradientes, grafo, timeline, código, viewport             │
├ Painéis e plugins (editor/panels, editor/plugins) ──────────────────────────┤
│ Hierarquia, Inspector, Cena, Jogo, Assets, Console, Profiler, Animação,      │
│ Animator, Mixer, Navegação, Input Actions, Histórico, Configurações…         │
└──────────────────────────────────────────────────────────────────────────────┘
```

## 2. Documentos

| Documento | Fonte de verdade em memória | Arquivo |
|---|---|---|
| `SceneDocument` | World de edição + registros de instâncias de prefab + dados desconhecidos | `.ascene` |
| `PrefabDocument` | World de pré-visualização (modo prefab) | `.aprefab` |
| `AssetDocument<T>` | Objeto do recurso (Material, AnimatorController, InputActions, AudioMixer, UIDocument…) | Arquivo do asset |
| `SettingsDocument` | Configurações do projeto | `ProjectSettings/*.json` |

- Cada documento tem **histórico de Undo, WAL, estado "modificado" e abas próprias**.
- Painéis **leem** documentos por vistas somente leitura e **escrevem só por comandos**. Nenhum painel altera o World ou um recurso diretamente (padrão *accessor* do ezEngine).
- Fechar um documento modificado pergunta "Salvar / Descartar / Cancelar". Fechar o app nunca perde dados, graças ao WAL.

## 3. Comandos, transações e Undo

### 3.1 Contrato

```cpp
struct Command {
  virtual Result<void> apply(DocumentContext&) = 0;     // aplica (também no redo)
  virtual void         revert(DocumentContext&) = 0;    // desfaz
  virtual bool         mergeWith(const Command&) { return false; }  // gestos contínuos
  virtual void         serialize(BinaryWriter&) const = 0;           // WAL
  virtual Invalidation invalidates() const = 0;
};
```

### 3.2 Comandos base

`SetProperties` (multi-objeto, com valores antigos por objeto), `ResetProperty`, `AddComponent` (aplica o plano de composição inteiro), `RemoveComponent` (captura o estado), `MoveComponent` (ordem), `PasteComponentValues`, `CreateEntity`, `DuplicateEntities` (remapeia referências internas), `DeleteEntities` (captura a subárvore), `Reparent`/`Reorder`, `Rename`, `SetActive`, `InstantiatePrefab`, `CreatePrefab`, `ApplyOverrides`, `RevertOverrides`, `UnpackPrefab`, `SetAssetProperty`.

### 3.3 Transações

- `beginTransaction(nome)` → comandos → `commit()` ou `cancel()` (cancelar reverte tudo).
- **Atômicas e multi-documento** quando preciso (aplicar override altera cena + prefab: tudo ou nada).
- **Fusão de gestos:** arrastar gizmo ou slider gera uma transação por gesto (modo *merge ends* do `UndoRedo` da Godot); o WAL grava só o estado final.
- Nomes legíveis ("Mover Porta", "Adicionar Rigidbody a 3 objetos") para o painel Histórico.
- Limite por memória (orçamento por documento) e por quantidade; os mais antigos são descartados primeiro.
- **Mudanças de gameplay em Play nunca geram Undo** (requisito do AGENTS).
- Seleção não é desfazível por si, mas desfazer uma criação/exclusão restaura a seleção coerente.

## 4. Astra UI (toolkit do editor sobre RmlUi)

| Elemento | Tipo | Motivo |
|---|---|---|
| Dock (divisores, abas, arrastar painel, gavetas, folhas inferiores) | Elemento C++ + RCSS | Layout por aparelho ([18](18-EDITOR-PAINEIS-E-FLUXOS.md) §2) |
| **Árvore virtual** (hierarquia, pastas, outliner) | Elemento C++ | Só as linhas visíveis existem no DOM; 100 mil itens sem custo de layout |
| **Grade de propriedades** (Inspector) | Elemento C++ com linhas RCSS | Centenas de campos, atualização por `PropertyPath` sujo |
| Lista/grade virtual (assets, console) | Elemento C++ | Milhares de itens |
| Campos: número com arrasto no rótulo, vetor, cor, curva, gradiente, enum, máscara, referência de objeto | Templates RML + lógica C++ | Reuso em Inspector, configurações e diálogos |
| Seletor de cor (roda HSV, sliders, HEX, HDR, conta-gotas do viewport) | Elemento C++ | |
| Editor de curvas e gradientes | Elemento C++ | Animação, partículas, curvas de gameplay |
| Editor de grafos (pan, zoom, nós, pinos, conexões) | Elemento C++ | Animator, Shader Graph, visual scripting |
| Timeline/dopesheet | Elemento C++ | Animação |
| Editor de código | Elemento C++ | [14](14-SCRIPTING-LUAU.md) §10 |
| Viewport | Elemento que exibe a textura de uma `View` do render e encaminha entrada | Cena, Jogo, pré-visualizações |
| Menus, menu radial, paleta de comandos, toasts, diálogos, folhas | RML + RCSS | |

- **Tema** em RCSS com variáveis (RmlUi 6.3) geradas dos tokens ([19](19-DESIGN-FERRAMENTAS-EXTERNAS-E-PIPELINE.md) §3).
- **Densidade** `Touch` e `Compact` trocando variáveis, sem RCSS duplicado.
- **Desempenho exigido (S-07):** Inspector com 300 propriedades monta em ≤ 16 ms e atualiza um campo em ≤ 1 ms em T2. A árvore com 10 mil entidades rola no ritmo do display. Abaixo disso, otimização ou plano B ([03](03-PILHA-E-BIBLIOTECAS.md) §6).

## 5. Inspector gerado

- Fonte: `PropertyInfo` do TypeRegistry, inclusive dos tipos de script.
- **Registro de editores de propriedade** (`EditorProperty` da Godot): (tipo, dica) → widget. Exemplos: `Float+Range` → campo com slider; `Float+Angle` → campo em graus com mostrador; `AssetRef(Texture)` → campo com miniatura; `LayerMask` → chips; `Curve` → mini-curva que abre o editor.
- **InspectorPlugins** substituem cabeçalho, rodapé ou propriedades de um tipo: Transform (alternância local/global), Camera (pré-visualização da vista), Light (temperatura → cor, unidade), Material (pré-visualização em esfera e grupos do shader), AudioClip (forma de onda), Animator (estado atual em Play), importadores (barra Aplicar/Reverter).
- **Multi-edição:** componentes em comum, valores mistos mostrados como "—", edição aplicada a todos numa transação.
- **Estados de propriedade:** override de prefab (barra lateral de acento + menu aplicar/reverter), chave de animação (losango vazio/cheio no tempo atual), diferente do padrão (ponto discreto com "Resetar"), inválido (contorno vermelho + mensagem), bloqueado em Play (cadeado com motivo).
- **Play:** valores ao vivo a 10 Hz; edição conforme `playMutability`; ao parar, oferta "Manter alterações" por componente.
- **Modo depuração:** mostra propriedades `RuntimeOnly` e internas, só leitura.

## 6. WAL e sessão

O WAL segue [06](06-CENA-PREFABS-SERIALIZACAO.md) §8.2. A **sessão** (separada do WAL) guarda o layout, os documentos abertos, a câmera do editor por cena, a seleção, os painéis expandidos e o workspace ativo, e é restaurada ao reabrir, inclusive depois de o sistema matar o app.

## 7. Plugins do editor

Um `EditorPlugin` registra qualquer combinação de:

| Ponto de extensão | Exemplo |
|---|---|
| Painel (dock) | Navegação, Mixer |
| Workspace (tela principal) | Animação, Script, UI |
| Editor de propriedade / InspectorPlugin | Curva, Material |
| Plugin de gizmo (desenho + alças + picking por tipo de componente) | Colisores, luzes, câmeras, links de nav |
| Importador | glTF, FBX, textura, áudio |
| Pré-visualizador/thumbnail | Material, mesh, prefab, áudio |
| Entradas do menu Criar | "3D › Cubo", "Luz › Spot" |
| Itens de menu de contexto e comandos da paleta | "Alinhar à vista" |
| Ferramenta de cena | Pincel de terreno (F13) |
| Página de configurações | Física, Qualidade |

**Toda funcionalidade embutida é um plugin** (dogfooding). Extensões do usuário em Luau (janelas e ferramentas próprias, sandboxed) ficam para a F14.

## 8. Ferramentas de cena e gizmos

- Ferramenta ativa: Vista, Mover, Rotacionar, Escalar, Rect, Transformar (universal), como os *EditorTools* da Unity.
- **API de alças:** posição, rotação (arcos), escala, livre, raio, caixa de bounds, ponto em superfície. Hit-test em espaço de tela com **raio de toque** (24 dp) e prioridade da alça mais próxima do dedo.
- Snapping: grade (tamanhos predefinidos), incremento de rotação e escala, **vértice** e **superfície** (alinha à normal). Pivô/centro e local/global.
- Picking: ID buffer ([08](08-RENDERIZACAO-FORGE.md) §9), com ciclo de seleção ao tocar de novo no mesmo ponto (objetos sobrepostos).
- Gizmos de componente pelo plugin do tipo; ícones em billboard com tamanho constante em tela; opção de ocultar por tipo.

## 9. Busca, tarefas e notificações

- **Paleta de comandos / busca global:** índice de entidades das cenas abertas, assets, comandos, configurações e documentação. Busca aproximada com prefixos (`t:Light`, `l:Personagem`, `>` para comandos) e ação direta (selecionar, abrir, executar).
- **Gerenciador de tarefas:** importação, cozimento, bake de nav/probes, compilação, build. Progresso, cancelamento e indicador circular na barra superior.
- **Notificações:** toasts transitórios; o Console guarda tudo; erros que exigem ação ficam fixos até resolvidos.

## 10. Play (D-14)

### 10.1 Entrar

1. Verificar scripts (erros bloqueantes impedem o Play com mensagem e link).
2. Salvar o estado da sessão; gravar WAL pendente.
3. **Clonar o mundo de edição** por serialização binária em memória e carga num mundo novo: o mesmo caminho do player, sem ponteiros compartilhados. O tempo de entrada é medido (orçamento por tamanho de cena).
4. Criar servidores do mundo de Play (física, áudio, nav, scripts); a câmera de jogo vai para a aba Jogo.

### 10.2 Durante

- Hierarquia e Inspector mostram o **mundo de Play** com cabeçalho tingido e o selo PLAY. O Scene View pode inspecionar o mundo de Play com a câmera do editor.
- Pausar, avançar um frame (fixo + update), escala de tempo para depuração.
- Toque na aba Jogo vai para o jogo; um botão alterna "entrada para o editor" quando é preciso manipular objetos durante o Play.

### 10.3 Parar

- Destruir o mundo de Play e seus servidores; invalidar handles.
- "Manter alterações": componentes ou propriedades marcados são copiados para o mundo de edição como **uma transação de Undo**.

### 10.4 Isolamento e evolução

| Falha | Proteção |
|---|---|
| Erro de script | Callback protegido, componente desabilitado, console |
| Laço infinito | `interrupt` do Luau |
| Memória de script | Limite por VM |
| Crash nativo (bug da engine) | WAL + sessão + relatório de crash. O editor reabre no mesmo ponto |

**Processo separado para o Play** (ideia da ADR-08): reavaliar depois da F12, se a taxa de crash nativo em Play for relevante. Custo: memória dupla e compartilhamento de surface (`AHardwareBuffer`/`SurfaceControl`).

Memória: o mundo de Play dobra o custo da cena. Em aparelhos T0, a opção "suspender render do mundo de edição durante o Play" libera parte disso, de forma explícita.

## 11. Bateria e fluidez do editor

- Render do viewport **sob demanda** ([04](04-ARQUITETURA-CAMADAS-E-REPOSITORIO.md) §7); UI redesenhada só quando algo muda ou anima.
- Thumbnails e importações com prioridade baixa e pausadas durante gestos de câmera.
- Modo "Economia" (preferência): limita o viewport a 30 Hz e desliga pós caros na vista do editor, com selo visível.

## 12. Preferências e configurações do projeto

- **Preferências do editor:** densidade, tamanho de fonte, gestos (sensibilidade, inversões), mão dominante (espelha a barra de ferramentas do viewport), vibração, salvamento, idioma (pt-BR e en desde o início), ferramentas externas (PC).
- **Configurações do projeto:** tags e layers, física (matriz), qualidade (tiers), input, áudio, tempo, player (nome, ícone, orientação, versão), build, navegação (tipos de agente, áreas).

## 13. Host (PC)

Docking livre (arrastar abas entre áreas), atalhos no padrão Unity (`Q/W/E/R/T/Y`, `F` enquadra, `Ctrl+D`, `Ctrl+Z/Y`, `Ctrl+P` Play, `Ctrl+Shift+P` Pause), menu de contexto com botão direito, rolagem de precisão, várias janelas fica para depois da 2.0.

## 14. Aceite (F4)

- Criar cena **só pela UI no aparelho**: chão, 3 objetos, luz, câmera; editar no Inspector; organizar na hierarquia; criar prefab; instanciar 3 vezes; override numa instância; aplicar; Undo/Redo de tudo; salvar; matar o app; reabrir sem perda.
- Play/Pause/Step/Stop 100× sem vazamento; "Manter alterações" funciona e é desfazível.
- Inspector com 300 propriedades e hierarquia com 10 mil entidades dentro do orçamento do S-07.
- Editor parado = 0 frames de GPU (contador de present) por 60 s.
- Captura de cada painel no aparelho, avaliada contra o design aprovado ([18](18-EDITOR-PAINEIS-E-FLUXOS.md), [19](19-DESIGN-FERRAMENTAS-EXTERNAS-E-PIPELINE.md)).
