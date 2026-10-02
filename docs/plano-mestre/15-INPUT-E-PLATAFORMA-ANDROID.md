# 15 — Input e plataforma Android

Referências: Android Game Development Kit (GameActivity, GameTextInput, Paddleboat, Frame Pacing/Swappy), Android Dynamic Performance Framework (ADPF), *Storage Access Framework*; Unity 6.0 **Input System 1.11** (actions, bindings, interações, processadores); Godot 4.7 `InputMap`, `InputEvent`.

---

## 1. Camada de plataforma

| Item | Decisão |
|---|---|
| Atividade | **GameActivity** (AGDK): thread nativa própria, filas de eventos de toque e tecla, integração com GameTextInput |
| Relação com o TF | O TF 1.63 tem sua camada `OS/Android` (app glue próprio). O **S-02** decide entre (a) adaptar o OS do TF para a GameActivity ou (b) camada de plataforma Astra que só entrega o `ANativeWindow` ao renderer do TF. A preferência é (b), por controle do loop e do ciclo de vida |
| Thread principal Astra | A thread nativa da GameActivity; a thread Java recebe apenas o que exige a UI do sistema (IME, seletores, diálogos) |
| JNI | Somente fora do caminho quente; chamadas assíncronas com resposta por fila (lição da entrada de texto da Astra atual) |

## 2. Ciclo de vida e surface

| Evento | Ação |
|---|---|
| `INIT_WINDOW` | Criar/recriar swapchain; retomar render |
| `TERM_WINDOW` | Parar render; destruir swapchain; **manter** recursos de GPU |
| `WINDOW_RESIZED`, `CONFIG_CHANGED`, `CONTENT_RECT_CHANGED`, `WINDOW_INSETS_CHANGED` | Recriar swapchain se o tamanho mudou; atualizar insets (área segura) e densidade |
| `PAUSE` | Salvar sessão do editor, pausar áudio, parar Play se configurado, `fsync` do WAL |
| `RESUME` | Revarrer assets, retomar áudio, render sob demanda |
| `LOW_MEMORY` / `onTrimMemory` | Liberar caches (thumbnails, mips fora de vista, bytecode de depuração) |
| `VK_ERROR_DEVICE_LOST` | Reinicializar o renderer e recarregar recursos de GPU a partir dos recursos de CPU/disco, com aviso ao usuário |

- `android:configChanges` cobre orientação, tamanho, densidade, layout, teclado e `uiMode`: rotação, dobra e multi-janela **não reiniciam** a Activity.
- Orientação por workspace: paisagem para cena e animação, retrato para a IDE de código (herdado), via JNI `setRequestedOrientation`.
- Taxa de atualização: escolha 60/90/120 Hz com `ANativeWindow_setFrameRate` + Swappy; no editor, a taxa só vale quando há frames (render sob demanda).

## 3. Toque, caneta e mouse

- `MotionEvent` → eventos de ponteiro Astra: id, posição em px e dp, pressão, tamanho, tipo de ferramenta (dedo, caneta, mouse, borracha), botões, **amostras históricas** (toques a 120–240 Hz deixam o arrasto de gizmo suave).
- Caneta (S Pen e similares): *hover* com pré-visualização de alvo no viewport, pressão para ferramentas de pintura (terreno, F13).
- Mouse e teclado (DeX, Chromebook, USB/Bluetooth): hover, rolagem, botão direito = menu de contexto, atalhos do host.
- Reconhecimento de gestos na camada do editor (thresholds em dp, configuráveis): toque, toque duplo, toque longo, arrasto, pinça, pan com dois dedos, toque com três dedos.

## 4. Texto (IME)

- **GameTextInput**: composição (acentos do pt-BR, previsão), seleção, clipboard, cursor.
- `IME_FLAG_NO_EXTRACT_UI` em paisagem (lição da Astra atual: o teclado em tela cheia escondia Aplicar/Cancelar).
- Tipos de campo: texto, número com sinal e decimal, multilinha, senha, busca. O teclado numérico aparece em campos numéricos do Inspector.
- Insets do teclado → o painel com foco rola para manter o campo visível.
- Teclado físico: atalhos, Tab entre campos, Enter confirma, Esc cancela.

## 5. Controles, sensores e vibração

| Dispositivo | Implementação |
|---|---|
| Gamepads | **Paddleboat** (AGDK): mapeamento de controles, vibração de controle |
| Acelerômetro, giroscópio, atitude | `ASensorManager` → dispositivos do Input |
| Vibração | `Vibrator`/`VibratorManager` via JNI; primitivas táteis (Android 11+) quando houver; API de jogo (`Haptics.Play(...)`) e retorno sutil no editor (snap, seleção) |

## 6. Armazenamento

| Uso | Local | Observação |
|---|---|---|
| Projetos ativos | `Android/data/<pacote>/files/Projects/` | Rápido, sem permissão. **Apagado ao desinstalar** |
| Importar arquivos | SAF `ACTION_OPEN_DOCUMENT` (múltiplos) | Copia para `Assets/` e importa |
| Exportar/backup de projeto | SAF `ACTION_CREATE_DOCUMENT` (`.zip`) | Também restaura (importar projeto) |
| Pasta de backup vinculada | SAF `ACTION_OPEN_DOCUMENT_TREE` | Exportação automática periódica opcional (zip) |
| Exportar APK de jogo | SAF `ACTION_CREATE_DOCUMENT` | [20](20-BUILD-EXPORTACAO-E-DISTRIBUICAO.md) |

- `MANAGE_EXTERNAL_STORAGE` **não** é usado (restrição de política do Google Play).
- Projetos abertos direto de URI SAF não são suportados (I/O lento e semântica diferente). Fica registrado como limite L-10.
- O hub mostra o aviso permanente "projetos vivem no app; exporte backups" e a data do último backup por projeto.
- Antes de importar ou cozinhar, o editor verifica o espaço livre e recusa com mensagem clara se não couber (lição desta própria sessão de planejamento, em que o disco do PC encheu).

## 7. Desempenho e temperatura

| API | Uso |
|---|---|
| `APerformanceHintManager` (ADPF) | Sessão com as threads principal e de render; duração real e alvo por frame |
| `AThermal_getThermalHeadroom` + listener de status | Resolução dinâmica, redução de taxa de frames do editor, aviso visível ao chegar a `SEVERE` |
| Game Mode API | Respeitar os modos bateria/desempenho escolhidos pelo usuário |
| Swappy | Frame pacing |

Toda adaptação automática de qualidade aparece no indicador de desempenho do editor ("resolução reduzida por temperatura"), nunca silenciosa.

## 8. Manifesto e requisitos

- `minSdk 29`, `targetSdk 36`, `arm64-v8a`, bibliotecas com alinhamento de **16 KB** (NDK r28+, AGP 8.5.1+).
- `<uses-feature android:name="android.hardware.vulkan.version" android:version="0x401000" android:required="true"/>` (Vulkan 1.1).
- Permissões: nenhuma para o núcleo. `VIBRATE` (normal) para vibração; `INTERNET` só se houver documentação online ou serviço explícito.

## 9. Input Actions (gameplay)

Referência: Unity Input System 1.11. Herança dos planos `INPUT-*` da Astra atual.

| Conceito | Detalhe |
|---|---|
| Asset `.ainput` | Mapas de ações → ações → bindings; esquemas de controle (Toque, Gamepad, Teclado+Mouse) |
| Tipos de ação | `Button`, `Value` (com tipo: float, Vector2…), `PassThrough` |
| Bindings | Caminhos de controle (`<Gamepad>/leftStick`, `<Touchscreen>/primaryTouch/position`, `<Keyboard>/space`), compostos (2DVector WASD, eixo com dois botões) |
| Processadores | Zona morta (stick/eixo), inverter, normalizar, escalar, clamp |
| Interações | `Press`, `Hold`, `Tap`, `MultiTap`, `SlowTap` |
| Runtime | Habilitar/desabilitar mapas, **rebinding** interativo com persistência em `user://` |
| Script | `action.performed:Connect(fn)`, `action:ReadValue()`, `WasPressedThisFrame()`, `WasReleasedThisFrame()`, `IsPressed()` |
| Controles na tela | Componentes `OnScreenStick` e `OnScreenButton` (UI F11) que alimentam o mesmo sistema como dispositivo virtual |
| Toque avançado | API de toques múltiplos com fases (o *EnhancedTouch* da Unity) |
| Editor | Editor de actions (mapas, ações, bindings com "ouvir entrada"), depurador de entrada ao vivo em Play |

## 10. Host Windows/Linux

Janela Win32/X11/Wayland (via OS do TF ou camada própria, decidido junto com o S-02), Vulkan, mouse/teclado, diálogos de arquivo nativos, observador de arquivos para `Assets/` (reimport automático), DPI por monitor, **simulação de toque** pelo mouse para testar fluxos de toque no PC.

## 11. Aceite (F2 e F6)

- 100 ciclos de pausa/retomada + rotação + multi-janela + dobra (quando houver aparelho) sem erro de validação e sem perder estado.
- Digitar "ação", "ç", emoji e texto colado em campos do Inspector, com composição correta.
- Gamepad Bluetooth controlando personagem via actions; rebinding salvo e restaurado.
- Exportar projeto em zip, desinstalar **num aparelho de teste**, reinstalar, importar e abrir sem perda.
- Log de *thermal headroom* durante 30 min de edição, com a reação de qualidade registrada.
