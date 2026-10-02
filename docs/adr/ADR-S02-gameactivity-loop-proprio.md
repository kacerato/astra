# ADR-S02 — Renderer do The Forge com GameActivity e loop próprio

- **Status:** aprovado (02/10/2026)
- **Pergunta (doc 22 §1):** dá para usar o renderer do TF com GameActivity e loop próprio, sem o `IApp`?
- **Prazo:** 5 dias. Usado: 1 sessão.

## Decisão

Opção (b) do doc 15 §1: **a plataforma Astra é dona do ciclo de vida** e só entrega a janela ao renderer. O build do TF foi dividido (`cmake/TheForge.cmake`):

| Alvo | Conteúdo | Quem usa |
|---|---|---|
| `tf_core` | Memória, log, arquivos, threads, tempo, compressão, CPU | Astra |
| `tf_graphics` | Vulkan, Resource Loader | Astra (`backends/forge`) |
| `tf_os` / `tf_renderer` | Camada de app do TF (IApp, ImGui, Lua, input, janela, animação, Memory Advice, Paddleboat) | Só spikes que usam `IApp` |

O renderer pede só **dois símbolos** da camada de janela do TF: `WindowDesc gWindow` (o Swappy lê `vm` e `clazz` da Activity) e `AndroidAttachToCurrentThread`. A plataforma Astra os fornece com uma `ANativeActivity` de fachada montada a partir do `GameActivity`: mesmos campos (`vm`, `clazz`, caminhos, `AAssetManager`). O sistema de arquivos do TF usa a mesma fachada.

## Evidência

| Critério | Resultado |
|---|---|
| Sem dependência do glue do TF | `libastra_spike_s02.so` liga só `tf_core` + `tf_graphics` + GameActivity 4.4.2. Zero símbolos de ImGui, Lua, UI, `ANativeActivity_onCreate`, Memory Advice ou Paddleboat (`llvm-readelf`/`llvm-nm`). APK sem `libmemory_advice.so` nem TensorFlow Lite. 16 KB em todos os `LOAD` |
| 100 ciclos de surface perdida/recriada | Xiaomi 25053PC47G (Adreno 825, Android 16), paisagem: 100 `TERM_WINDOW`, 101 swapchains criadas, **mesmo PID**, **0** linhas `ERR`, **0** erros de validação Vulkan, ~60 fps depois do último ciclo (log completo do TF no aparelho). Roteiro reproduzível: `tools/device/surface_cycles.sh` (rodada extra de 20 ciclos: APROVADO) |
| IME | Toque abre o Gboard em paisagem **sem** a tela cheia de extração (`IME_FLAG_NO_EXTRACT_UI`); *insets* do teclado chegam (`b=755`). Texto por tecla, apagar e Enter (`IME_ACTION_DONE` = 6) funcionam. Digitação manual do usuário entregou UTF-8 correto: `Asta\xc3\xa7\xc3\xa3o \xc3\xa7\xc3\xa9` ("Astação çé") |
| Captura | `docs/validacao/2026-10-02-s02/android-adreno825-paisagem.png`, `android-adreno825-teclado-paisagem.png` |

## Achados que viram tarefa

| Achado | Encaminhamento |
|---|---|
| A cada swapchain, aviso de **desempenho** da validação `UNASSIGNED-CoreValidation-SwapchainPreTransform`: em paisagem o Adreno reporta `currentTransform = ROTATE_90`, e o TF cria com `IDENTITY` (o compositor gira a imagem a cada frame) | **S-05** (pré-rotação): patch no `Vulkan.c` ou rotação no próprio frame |
| O Gboard não abriu região de composição nesse campo (`composingRegion` sempre `[-1,-1)`); acentos chegam já confirmados | F2: testar outros teclados (Samsung, SwiftKey) e o tipo de entrada; a API `GameTextInput` já entrega a região quando existir |
| Emoji não testado (não aparece no log) | F2 (aceite do doc 15 §11) |
| MIUI bloqueia instalar pacote **novo** por ADB sem "Instalar via USB"; atualização funciona | Documentado para a validação no aparelho (doc 21 §5) |
| Barra de navegação visível (spike não pede modo imersivo) | Editor: `WindowInsetsController` imersivo (doc 15 §2) |

## Pendente

- Rotação real (180° entre as duas paisagens), multi-janela e dobra: não exercitados; o spike usa `configChanges` e recria a swapchain por tamanho, mas sem teste.
- Host Windows sem a camada de app do TF ainda não existe (o S-01 no host usa `IApp`); a janela própria do host fica para a F2.
