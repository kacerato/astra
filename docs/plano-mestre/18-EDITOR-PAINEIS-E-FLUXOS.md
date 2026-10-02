# 18 — Painéis e fluxos do editor

Os wireframes abaixo são **estruturais**: definem posições, hierarquia e comportamento. O visual final sai do Figma ([19](19-DESIGN-FERRAMENTAS-EXTERNAS-E-PIPELINE.md)) com os tokens de [17](17-EDITOR-DESIGN-SYSTEM.md). Todo controle listado precisa de consumidor real antes de ir para a UI (DP-08).

Legenda dos wireframes: `[▶]` botão, `▾` abre menu, `⋮` mais opções, `◆` mark da Astra, `▣` painel recolhível.

---

## 1. Workspaces

| Workspace | Ícone | Painéis padrão | Fase |
|---|---|---|---|
| **Cena** | `scene` | Viewport, Hierarquia, Inspector, Assets, Console | F4 |
| **Animação** | `animation` | Viewport, Animação (dopesheet/curvas), Animator, Inspector | F8 |
| **Materiais** | `material` | Pré-visualização, Inspector de material, Shader Graph (F14), Assets | F7 |
| **Script** | `script` | IDE, Arquivos, Console, Depurador | F6 |
| **UI** | `ui-builder` | Canvas de UI, Árvore de elementos, Estilos, Assets | F11 |
| **Perfil** | `profiler` | Profiler, Memória, Gráficos do aparelho, Console | F4 |
| **Build** | `build` | Alvos, Configurações do player, Relatório de build | F12 |

Cada workspace guarda seu layout por classe de aparelho. Trocar de workspace mantém documento e seleção.

## 2. Layouts por aparelho

### 2.1 Celular em paisagem (principal)

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│ ◆ │ cena_01 ▾ │ ⌂Cena ▾ │          ⟨▶⟩ ❚❚ ▶❚           │ ↶ ↷ │ ⌕ │ ◔ │ ⋮ │   44 dp
├────┬─────────────────────────────────────────────────────────────────────────────┬────┤
│ ▤  │ [Persp ▾] [Lit ▾] [◎ gizmos] [⚙ câmera]                       ┌─────┐       │ ▣  │
│ Hier│                                                               │ cube│       │Insp│
│ ▦  │ ┌──┐                                                          └─────┘       │    │
│ Ass │ │✥ │◄ ferramenta ativa (limão chanfrado)                                    │ ⓘ  │
│ ⌕  │ │⟳ │                                                                         │Comp│
│     │ │⤢ │                         VIEWPORT                                        │    │
│ ≣  │ │▭ │                                                                         │    │
│ Cons│ │⊕ │                                                                         │    │
│     │ └──┘                                                                         │    │
│     │ [▦ 0,5 m] [∠ 15°] [⇲ 0,1]   [⦿ pivô] [⊕ global]          [60 fps · 4,1 ms]  │    │
└────┴─────────────────────────────────────────────────────────────────────────────┴────┘
  trilho esquerdo 52 dp                                                    trilho direito 52 dp
```

- **Trilho esquerdo:** Hierarquia, Assets, Busca e Console abrem **gavetas** de 300 dp sobre o viewport. Arrastar a borda redimensiona; arrastar até a borda oposta fixa a gaveta (empurra o viewport).
- **Trilho direito:** Inspector (gaveta de 340 dp) e atalho "Adicionar componente".
- **Folha inferior** (Console, Timeline, Profiler): alturas de pouso 0 / 36 dp (linha de status) / 45% / tela cheia.
- **Paleta de ferramentas** na lateral do polegar dominante (espelhável); overlays reposicionáveis.
- Com Inspector e Hierarquia abertos ao mesmo tempo, o viewport fica no meio, com pelo menos 40% da largura.

### 2.2 Celular em retrato (só workspace Script)

```
┌──────────────────────────────┐
│ ◆ cena_01 ▾      ⟨▶⟩  ↶ ↷  ⋮ │
├──────────────────────────────┤
│                              │
│          VIEWPORT            │  ~45%
│   (ferramentas em linha)     │
├──[Hier]──[Insp]──[Ass]──[≣]──┤  abas da folha
│                              │
│  painel ativo em tela cheia  │  ~55%, arrastável
│                              │
└──────────────────────────────┘
```

O editor roda em paisagem (decisão do usuário, 02/10/2026). Este layout existe **só** no workspace Script: a IDE de código pode girar para retrato (teclado aberto, leitura longa); as abas inferiores mostram Arquivos, Console e Depurador. Os demais workspaces ficam travados em paisagem.

### 2.3 Tablet

Três colunas fixas (Hierarquia 280 | Viewport | Inspector 340) + painel inferior de 220 dp com abas (Assets, Console, Animação). As colunas recolhem para trilhos com um toque.

### 2.4 PC

Docking livre: abas arrastáveis entre áreas, divisores, layouts salvos por workspace, densidade Compact, atalhos de teclado ([16](16-EDITOR-ARQUITETURA.md) §13).

### 2.5 Barra superior

| Zona | Conteúdo | Comportamento |
|---|---|---|
| Esquerda | Mark ◆ (menu do app: projeto, salvar, configurações, hub), seletor de documento (cenas/prefabs abertos, com ponto de "modificado") | Toque no nome abre a lista de documentos |
| Centro-esquerda | Workspace atual | Toque abre a lista de workspaces |
| **Centro** | **Play** (limão chanfrado), Pause, Step; segurar Play abre opções (escala de tempo, iniciar na cena atual/inicial) | Durante o Play, a barra ganha a moldura limão |
| Direita | Desfazer, Refazer, Busca/paleta, anel de tarefas (aparece só com tarefa em andamento), chip térmico (só quando há adaptação), ⋮ | Segurar Desfazer abre o Histórico |

## 3. Hub (projetos)

Evolução do shell atual (`screen-03-projects.png`): mantém a silhueta do cometa, a barra lateral com trilho limão no item ativo e os cartões fotográficos.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ ◆ASTRA™                                         CRIE SEM LIMITES   ■ ■ ■   │
│                                                                             │
│ ▌Projetos        PROJETOS  [⌕ buscar] [Recentes ▾]          [＋ Novo projeto]│
│  Novo            ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  Importar        │  (capa   │ │  (capa   │ │  (capa   │ │  (capa   │        │
│  Aprender        │  da cena)│ │          │ │          │ │          │        │
│  Configurações   ├──────────┤ ├──────────┤ ├──────────┤ ├──────────┤        │
│                  │Floresta ⋮│ │Oceano   ⋮│ │Backroom ⋮│ │Veículo  ⋮│        │
│                  │2 h · 1,2G│ │ontem     │ │…         │ │…         │        │
│                  │⚠ sem bkp │ │✓ bkp hoje│ │          │ │          │        │
│ ASTRA ENGINE 2.0 └──────────┘ └──────────┘ └──────────┘ └──────────┘ PRONTO│
└─────────────────────────────────────────────────────────────────────────────┘
```

- Capa capturada automaticamente da última vista da câmera ao salvar.
- Cada cartão mostra a última edição, o tamanho e o **estado de backup** (aviso se não houver backup recente, ver [15](15-INPUT-E-PLATAFORMA-ANDROID.md) §6).
- ⋮ do cartão: abrir, duplicar, exportar `.zip`, renomear, mover para a lixeira.
- **Novo projeto:** nome, local, template. Só entram templates que funcionam de ponta a ponta: Vazio 3D, Terceira pessoa, Primeira pessoa, Veículo e Vitrine de materiais, cada um entrando conforme sua fase. Os outros aparecem como "em breve" sem criar nada.
- Importar: `.zip` de projeto ou pasta via SAF.
- Recuperação: se houver WAL pendente, o cartão mostra "Recuperar alterações".

## 4. Scene View

### 4.1 Overlays

| Overlay | Posição padrão | Conteúdo |
|---|---|---|
| Ferramentas | Lateral do polegar | Vista, Mover, Rotacionar, Escalar, Rect, Transformar, Criar (⊕) |
| Vista | Topo esquerdo | Projeção, modo de vista ([08](08-RENDERIZACAO-FORGE.md) §9), gizmos (por tipo), configurações da câmera (velocidade, FOV, near/far) |
| Cubo de vista | Topo direito | Toque numa face alinha a vista; toque no centro alterna perspectiva/ortográfica |
| Snapping | Base esquerda | Grade (passos predefinidos), ângulo, escala, vértice, superfície; pivô/centro; local/global |
| Estatísticas | Base direita | FPS, ms de CPU/GPU, draws, triângulos; toque expande |
| Contexto | Junto da seleção | Ações rápidas do objeto selecionado (enquadrar, duplicar, excluir, ⋮) |

### 4.2 Gestos

| Gesto | Ação |
|---|---|
| Toque | Selecionar; toque de novo no mesmo ponto alterna entre objetos sobrepostos |
| Toque duplo em objeto | Enquadrar |
| Toque duplo no vazio | Limpar seleção |
| Arrastar com 1 dedo no vazio | Orbitar (modo órbita) ou olhar (modo voo) |
| Arrastar com 2 dedos | Pan |
| Pinça | Zoom/dolly (velocidade proporcional à distância do pivô) |
| Toque e segura (320 ms) | Menu radial (§4.6) |
| Toque com 3 dedos | Desfazer; para a direita, refazer (configurável) |
| Arrastar alça do gizmo | Manipular; com segundo dedo apoiado, ajuste fino |
| Arrastar asset dos Assets para a vista | Instanciar na superfície sob o dedo, com pré-visualização |

**Modo voo:** joystick virtual à esquerda (mover) e arrasto à direita (olhar), com subir/descer e velocidade. Ideal para cenas grandes.

**Manipulação deslocada (inovação para toque):** opção em que a alça ativa é controlada por um *trackpad* no canto da tela, para o dedo não cobrir o objeto. Ligável nas preferências.

### 4.3 Seleção

- Seleção múltipla: chip "＋ Seleção" na barra de contexto alterna o modo aditivo; caixa de seleção por arrasto com o modo ativo.
- Contorno limão no objeto ativo e limão a 50% nos demais da seleção.
- Selecionar na vista sincroniza Hierarquia (rola até o item) e Inspector.

### 4.4 Câmera do editor

Por documento e salva na sessão: órbita (pivô no objeto enquadrado), voo, 2D (ortográfica frontal para UI/2D), favoritos de vista (salvar e voltar), "Alinhar objeto à vista" e "Alinhar vista ao objeto".

### 4.5 Colocação

Novos objetos nascem no ponto de mira (raycast na superfície), com a normal opcional; snap à grade se ativo; "Soltar no chão" no menu de contexto.

### 4.6 Menu radial (herança ADR-11)

Oito direções fixas (memória muscular), contextuais ao alvo:

| Direção | Sobre objeto | Sobre o vazio |
|---|---|---|
| ↑ | Enquadrar | Criar… (abre a grade de criação) |
| ↗ | Duplicar | Colar |
| → | Adicionar componente | Modo de vista |
| ↘ | Criar filho | Grade/snap |
| ↓ | Excluir | Vista de cima |
| ↙ | Isolar (mostrar só a seleção) | Mostrar tudo |
| ← | Selecionar pai | Trocar projeção |
| ↖ | Renomear | Configurações da câmera |

Arrastar na direção e soltar executa; um rótulo aparece só após 400 ms parado sobre a fatia (aprendizado sem poluir).

## 5. Hierarquia

```
 HIERARQUIA                                  [⌕] [＋] [⋮]
 ▾ ▣ cena_01                                         ⋮
   ▾ ◇ Ambiente
       ☀ Sol                                      👁  ⌖
       ◫ Chão                                     👁  ⌖
   ▾ ⬢ Jogador          (prefab, azul)             👁  ⌖
      ▌ ◎ Câmera        (override: barra limão)    👁  ⌖
   ▸ ⬢ Inimigo (3)
     ⚠ Porta            (selo de erro de script)   👁  ⌖
```

- Linhas virtualizadas (100 mil itens), guias de indentação, ícone do componente principal, nome, selos (prefab, erro, ausente, desabilitado em cinza), olho (visibilidade **no editor**, não `activeSelf`) e cadeado de pick.
- **Arrastar e soltar** com indicadores: linha entre itens (reordenar) e realce (tornar filho); a pose é mantida (KeepWorld), com opção nas preferências.
- Deslizar a linha para a esquerda: Duplicar, Excluir, Isolar. Toque e segura: modo de seleção múltipla com caixas.
- Busca com filtros: nome, `t:Componente`, `l:Layer`, `tag:`, `prefab:`, `erro:`; resultados em lista plana com caminho.
- Cena como raiz (multi-cena aditiva: uma raiz por cena, com estado carregada/descarregada e ⋮ para salvar/descarregar/definir ativa).
- Durante o Play: mostra o mundo de Play com cabeçalho tingido; objetos criados em runtime são marcados.

## 6. Inspector

```
 INSPECTOR                               [🔒] [⌕ filtrar] [⋮]
 ┌─────────────────────────────────────────────────────────┐
 │ [⬢] Jogador                         ☑ ativo   tag ▾  L ▾│  cabeçalho da entidade
 │     Prefab: Jogador.aprefab   [Abrir] [Overrides ▾]      │  (só em instâncias)
 └─────────────────────────────────────────────────────────┘
 ┌─● TRANSFORM ───────────────────────────────── ⓘ ⋮ ┐  cabeçalho fixo ao rolar
 │ Posição   ▏x 0,00 m ▏y 1,20 m ▏z −4,00 m           │  tique de eixo colorido
 │ Rotação   ▏x 0,0°   ▏y 90,0°  ▏z 0,0°              │
 │ Escala    ▏x 1,00   ▏y 1,00   ▏z 1,00    [🔗]       │
 └────────────────────────────────────────────────────┘
 ┌─● RIGIDBODY ─────────────────────────── [◉] ⓘ ⋮ ┐   ponto laranja = física
 │▌Massa              [ 20,0      kg ]   •          │   barra limão = override
 │ Amortecimento lin. [ 0,05         ]──○────────   │   slider com faixa suave
 │ Gravidade          [●━━]                         │   toggle
 │ Cinemático         [━━○]                         │
 │ Detecção           [Discreta|Contínua|Especul.]  │   segmentado (≤ 4)
 │ ▸ Restrições                                     │   seção recolhida
 │ ▸ Info (somente leitura)                         │
 └──────────────────────────────────────────────────┘
            [ ＋ Adicionar componente ]
```

- Cabeçalho da entidade: ícone, nome editável, ativo, tag, layer, estática (se houver consumidor), barra de prefab com Overrides (lista com aplicar/reverter por item).
- **Cartões de componente** com ponto de categoria, toggle `enabled`, ajuda (abre a doc), ⋮ (Resetar, Copiar valores, Colar valores, Copiar como novo, Remover, Mover para cima/baixo, Editar script).
- **Filtro do Inspector:** digitar "massa" mostra só as propriedades correspondentes em todos os componentes.
- **Trava (🔒):** o Inspector fica preso ao objeto atual enquanto você seleciona outros (para arrastar referências).
- Arrastar um objeto da hierarquia ou um asset para um campo de referência atribui; campos compatíveis brilham durante o arrasto.
- Estados de propriedade conforme [16](16-EDITOR-ARQUITETURA.md) §5.
- Asset selecionado: o Inspector mostra o Inspector do asset (material, importação com Aplicar/Reverter, áudio com forma de onda, prefab com pré-visualização e "Abrir").

## 7. Assets

```
 ASSETS   Projeto › Personagens › Jogador         [⌕] [▦/≣] [⇅] [Importar]
 [Todos][Cenas][Prefabs][Modelos][Materiais][Texturas][Áudio][Scripts]  ← chips
 ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐
 │ thumb│ │ thumb│ │ thumb│ │ thumb│ │  ⏳   │  importando (anel)
 │ ⬢    │ │ ◒    │ │ ▦    │ │ ♪    │ │      │  selo de tipo
 └──────┘ └──────┘ └──────┘ └──────┘ └──────┘
 Jogador  Metal    Pele_D   Passo    casa.glb
```

- Grade de miniaturas (96–128 dp, controle de tamanho por pinça) ou lista; breadcrumb clicável; árvore de pastas recolhível no tablet/PC.
- Chips de tipo, rótulos, favoritos, busca com `t:` e `label:`.
- Toque abre o Inspector do asset; toque duplo abre (cena, prefab, script, material); arrastar leva para a vista, a hierarquia ou um campo.
- ⋮ / toque e segura: Renomear (inline), Duplicar, Reimportar, **Usado por**, Localizar na pasta, Mover para a lixeira, Criar (material, script, pasta, prefab, input actions…).
- Importar: seletor do sistema (múltiplos arquivos); progresso por item; erro com relatório.
- Selos: "compressão pendente", "importando", "erro de import", "ausente".

## 8. Console, Jogo, Profiler e Histórico

**Console:** chips com contagem por nível (info/aviso/erro) que filtram; agrupar repetidos; busca; filtro por canal e por objeto selecionado; linha compacta (ícone, hora, mensagem); toque expande a pilha com quadros clicáveis (abre o script na linha); "Limpar ao dar Play"; "Pausar no erro".

**Jogo:** proporção e resolução (presets de aparelho, resolução nativa, tier simulado), escala, "Maximizar no Play", estatísticas; o toque vai para o jogo; botão para alternar a entrada para o editor.

**Profiler:** gráfico de frames com linha de meta (16,6/11,1/8,3 ms), linha do tempo por thread (principal, render, workers, áudio), trilha de GPU por pass, contadores (draws, triângulos, memória por tag, memória Vulkan, vozes, corpos ativos, *thermal headroom*), captura de N frames, comparação entre capturas, botão "Abrir no Tracy" (builds de desenvolvimento).

**Histórico:** lista de transações por documento com nome legível; tocar num item volta até ele (Undo/Redo em lote).

## 9. Partículas: módulos e editor (F13)

Referências: Unity 6.0 *Particle System* (módulos) e Godot 4.7 `GPUParticles3D`/`ParticleProcessMaterial`. Simulação na GPU ([08](08-RENDERIZACAO-FORGE.md) §12).

| Módulo | Estado planejado |
|---|---|
| Principal (duração, loop, pré-aquecimento, vida, velocidade, tamanho, rotação, cor inicial, gravidade, espaço de simulação, máximo de partículas) | F13 |
| Emissão (taxa no tempo, taxa por distância, rajadas) | F13 |
| Forma (esfera, hemisfério, cone, caixa, círculo, borda, malha) | F13 |
| Velocidade, limite de velocidade, força ao longo da vida | F13 |
| Cor, tamanho e rotação ao longo da vida (curvas/gradientes) | F13 |
| Ruído (curl noise) | F13 |
| Colisão com o depth buffer (GPU) | F13 |
| Animação de folha de textura | F13 |
| Renderer (billboard, alongado, horizontal/vertical, malha; material; ordenação) | F13 |
| Sub-emissores, trilhas, luzes, colisão com o mundo físico | Pendente |

Editor: pré-visualização tocando no Scene View com controles (play/pausa/reiniciar/velocidade) e contagem de partículas; cada módulo é um cartão recolhível com toggle; curvas e gradientes inline.

## 10. Editores especializados

| Editor | Essência | Doc |
|---|---|---|
| Material | Pré-visualização em esfera/cubo/malha com fundo HDRI, grupos do shader, slots de textura com miniatura, "Usado por" | [08](08-RENDERIZACAO-FORGE.md) §4 |
| Shader Graph (F14) | Grafo com pré-visualização por nó, tipos coloridos por pino, compilação no aparelho | [08](08-RENDERIZACAO-FORGE.md) §4.2 |
| Animação / Animator | Dopesheet, curvas, gravação; grafo de estados ao vivo | [10](10-ANIMACAO-OZZ.md) §7 |
| Mixer | Faixas com medidores, faders, efeitos, snapshots | [11](11-AUDIO-MINIAUDIO.md) §8 |
| Navegação | Bake, áreas, tipos de agente, overlay | [12](12-NAVEGACAO-RECAST.md) §6 |
| Input Actions | Mapas → ações → bindings, "ouvir entrada", depurador | [15](15-INPUT-E-PLATAFORMA-ANDROID.md) §9 |
| UI | Árvore de elementos, estilos, pré-visualização por aparelho | [13](13-UI-RUNTIME-RMLUI.md) §7 |
| Script (IDE) | Código, depurador, diagnósticos | [14](14-SCRIPTING-LUAU.md) §10 |

## 11. Criar, adicionar componente e paleta

- **Criar (⊕):** grade de ícones por categoria (Vazio, Grupo, Primitivas, Luzes, Câmera, Áudio, Efeitos, UI, Navegação) com recentes no topo. Só mostra o que está implementado.
- **Adicionar componente:** folha com busca e foco no campo; categorias com ícone e cor; recentes e favoritos; componentes incompatíveis (conflitos) aparecem desabilitados com o motivo; dependências são adicionadas juntas e anunciadas ("Adicionado também: Transform").
- **Paleta de comandos:** busca global ([16](16-EDITOR-ARQUITETURA.md) §9) com resultados agrupados (Comandos, Objetos, Assets, Configurações, Docs).

## 12. Fluxos completos (com meta de toques)

A meta de toques vale para o celular em paisagem, com usuário experiente. Ela é medida por roteiro gravado.

| Fluxo | Passos | Meta |
|---|---|---|
| **F-01 Criar projeto e cena** | Hub → Novo projeto → nome → template Vazio 3D → Criar → editor aberto com câmera, luz e chão | ≤ 5 toques + digitação |
| **F-02 Adicionar e posicionar objeto** | ⊕ → Cubo (nasce na superfície mirada) → arrastar alça Y → soltar com snap | ≤ 4 |
| **F-03 Dar física ao objeto** | Selecionar → trilho "＋ Componente" → digitar "rig" → Rigidbody → ajustar massa por arrasto no rótulo | ≤ 5 |
| **F-04 Importar GLB do celular** | Assets → Importar → escolher arquivo → (import em segundo plano) → arrastar o modelo para a vista | ≤ 5 |
| **F-05 Prefab com override** | Arrastar objeto da hierarquia para Assets (cria prefab) → arrastar prefab 3× para a vista → mudar cor numa instância → Overrides → Aplicar | ≤ 9 |
| **F-06 Script em objeto** | Selecionar → ＋ Componente → "Novo script" → nome → IDE abre com o modelo → salvar → campo aparece no Inspector | ≤ 6 + código |
| **F-07 Ajuste ao vivo** | Play → mudar velocidade no Inspector → Stop → "Manter alterações" na velocidade | ≤ 5 |
| **F-08 Animar uma luz** | Selecionar luz → workspace Animação → Criar clipe → Gravar → mover tempo → mudar intensidade → parar gravação | ≤ 8 |
| **F-09 Exportar APK de teste** | Workspace Build → Android → Exportar APK → escolher destino → instalar | ≤ 6 |
| **F-10 Recuperar após o sistema matar o app** | Abrir app → Hub mostra "Recuperar alterações" → Recuperar → editor no mesmo ponto | ≤ 2 |

## 13. Captura e aceite de cada painel

Para cada painel e fluxo (regra do AGENTS.md visual):

1. Captura no aparelho (paisagem e retrato) e no tablet/PC, em estado vazio, normal, carregando, erro e Play.
2. Comparação com o frame aprovado no Figma (sobreposição por diferença, [19](19-DESIGN-FERRAMENTAS-EXTERNAS-E-PIPELINE.md) §7).
3. Revisão pelo checklist de [17](17-EDITOR-DESIGN-SYSTEM.md) §14.
4. Registro separado de **conceito** (imagem gerada ou frame), **implementado** (código) e **testado no aparelho** (captura + roteiro).
