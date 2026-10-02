# ADR-S03 — Vulkan no host Windows

- **Status:** aprovado com patch (02/10/2026)
- **Pergunta (doc 22 §1):** o fork aceita Vulkan no Windows (a 1.60 removeu a troca DX12/Vulkan no PC)?

## Decisão

O host Windows usa Vulkan, como o aparelho. A seleção é por definições de compilação (`FORGE_EXPLICIT_RENDERER_API` + `FORGE_EXPLICIT_RENDERER_API_VULKAN`, em `cmake/TheForge.cmake`), sem D3D12.

## Evidência

- O spike S-01 compila e roda no Windows com Vulkan (Intel UHD), mesma textura e shaders FSL do Android. Validação ativa e sem mensagens (ver ADR-S01).
- O caminho estava parcialmente apodrecido: `WindowsBase.cpp` chamava uma API removida. Um patch de 8 linhas corrigiu (`third_party/ASTRA_PATCHES.md`).
- O renderer Vulkan no Windows ainda liga AMD AGS e NVAPI (como o upstream); a DLL `amd_ags_x64.dll` é copiada ao lado do executável.
