# ADR-S05 — Controles para GPUs de tile (TBDR) e pré-rotação

- **Status:** aprovado, com a medição de banda pendente (02/10/2026)
- **Pergunta (doc 22 §1):** o TF 1.63 oferece load/store discard, memória *lazily allocated* e pré-rotação da surface? Onde não oferece, qual patch?
- **Prazo:** 5 dias. Usado: 1 sessão.

## Resultado por item

| Item | Situação no TF 1.63 | Ação | Evidência |
|---|---|---|---|
| Load/store actions | `LOAD_ACTION_{DONTCARE,LOAD,CLEAR}` e `STORE_ACTION_{STORE,DONTCARE}` mapeiam direto para `VK_ATTACHMENT_*_OP_*` (`Vulkan.c`, tradutores na linha ~212) | Nenhuma | Código; S-02 usa `CLEAR` + `DONTCARE` no depth |
| `STORE_ACTION_NONE` | Mapeado para `STORE_OP_DONT_CARE`, não para `VK_ATTACHMENT_STORE_OP_NONE` (que preserva sem escrever) | Registrado; patch só quando houver consumidor (não usar `NONE` contando com preservação) | Código |
| Memória *lazily allocated* | `TEXTURE_CREATION_FLAG_ON_TILE` → `VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED` + `TRANSIENT_ATTACHMENT`, com fallback para memória comum e `Texture::mLazilyAllocated`. O TF força `STORE_DONTCARE` nesses anexos | Nenhuma | Adreno 825: depth `D32` 1280×2772 `ON_TILE` → **memória lazily allocated CONCEDIDA**, sem erro de validação |
| Pré-rotação | Sempre `IDENTITY` (`#TODO` no código); em paisagem o Adreno reporta `ROTATE_90` e a validação acusa `SwapchainPreTransform` (o compositor gira a imagem a cada frame) | **Patch** `3730ff6` no fork: `SWAP_CHAIN_CREATION_FLAG_PRE_ROTATION`, `SwapChain::mPreRotationDegrees`, `SwapChain::mSuboptimal` | Ver abaixo |

## Pré-rotação: evidência (Xiaomi 25053PC47G, Adreno 825, Android 16)

- Com o flag: swapchain 1280×2772 (orientação nativa do painel) para janela 2772×1280, pré-rotação 90°, **0** avisos `SwapchainPreTransform`. Sem o flag: 2772×1280 e 1 aviso por swapchain.
- O app gira o clip-space em **−N graus** (sentido confirmado por captura). Com o triângulo congelado, as capturas com e sem pré-rotação diferem em 32 de 3.548.160 pixels, todos na borda serrilhada: `docs/validacao/2026-10-02-s05/prerotacao-com-vs-sem.png`.
- Giro de 180° entre as duas paisagens (feito à mão pelo usuário, nos dois sentidos): o Android não muda o tamanho da surface nem envia mudança de configuração; o present devolve `SUBOPTIMAL`, guardado em `mSuboptimal`, e o app recria a swapchain (90° ↔ 270°). Imagem correta nas duas posições, segundo o usuário; 0 erros no log.
- Regra para o renderer Astra (F2): **só o último passe (o que escreve na swapchain) aplica a pré-rotação**; os demais passes renderizam na orientação lógica em alvos próprios.

## Pendente

- **Medição de banda com o Android GPU Inspector** (parte do aceite original): o AGI não está instalado nesta máquina. Fica para a bancada da F2/F7 (doc 21 §6), comparando pré-rotação ligada/desligada e depth `ON_TILE` contra memória comum.
- Mali e PowerVR não testados (só há o aparelho Adreno).
