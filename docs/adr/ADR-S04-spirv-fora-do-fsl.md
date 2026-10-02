# ADR-S04 — SPIR-V gerado fora do FSL (GLSL próprio, compilado no PC ou no aparelho)

- **Status:** aprovado (02/10/2026)
- **Pergunta (doc 22 §1):** o TF consome SPIR-V gerado fora do FSL, com o layout de descriptors correto? (Base para shaders do usuário e Shader Graph compilados no aparelho, doc 08 §4.)
- **Prazo:** 3 dias. Usado: 1 sessão. **Sem patch no TF.**

## Decisão

- Shaders vindos de fora do FSL entram por `addShaderBinary` (o TF só usa o bytecode para criar os `VkShaderModule`; não reflete o SPIR-V, porque o layout vem do SRT em C++).
- O GLSL do usuário/Shader Graph precisa seguir o **mesmo layout que o FSL gera**:

| Recurso (SRT) | GLSL |
|---|---|
| Set `Persistent` | `set = 0` |
| Set `PerFrame` | `set = 1` (`PerBatch` = 2, `PerDraw` = 3) |
| Texturas e buffers | `binding` = índice no set |
| Samplers | `binding` = índice + **200** |
| Samplers estáticos do `DefaultRootSignature` | `set = 0, binding = 100 + N` |
| Constant buffers | `std140`; matrizes coluna-maior; `mul(a, b)` do FSL = `a * b` |

- Compilação no aparelho: **glslang 16.6.0** (`third_party/glslang`, submódulo fixado, só front-end GLSL + gerador SPIR-V, sem HLSL nem SPIRV-Tools) pela API C (`glslang_c_interface.h`), alvo **SPIR-V 1.0 / Vulkan 1.0**, igual ao FSL.

## Evidência (Xiaomi 25053PC47G, Adreno 825, Android 16, build de debug)

| Modo (`debug.astra.s02.shader`) | Origem do shader | Resultado |
|---|---|---|
| 0 | FSL (`CompiledShaders`) | Referência |
| 1 | `glsl/triangle.{vert,frag}` compilados no PC (`glslangValidator` do TF, `spirv1.0`) | Captura **idêntica** à referência (0 pixels diferentes), 0 erros |
| 2 | O mesmo GLSL compilado **no aparelho** pelo glslang | Captura **idêntica** (0 pixels diferentes), 0 erros; SPIR-V com o mesmo tamanho do PC (1316 B / 1060 B) |

Tempo no aparelho (debug, `-O0`): 472 ms no primeiro shader (inicialização interna do glslang) e 5,5 ms no segundo.

## Pendente / consequências

- Medir tempo e tamanho em **release** (a `.so` de debug passou de 14 para 49 MB com o glslang e os símbolos). Orçamento de tamanho do player: o glslang só entra no **editor**; jogos exportados recebem SPIR-V já compilado (doc 20).
- A primeira compilação deve rodar fora da thread principal, com aquecimento ao abrir o editor de materiais.
- Gerador do Shader Graph (F14) deve produzir o layout da tabela acima; um teste de contrato compara a assinatura do layout com o SRT correspondente.
