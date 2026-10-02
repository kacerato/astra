# Patches da Astra sobre bibliotecas de terceiros

Cada alteração em código de terceiros entra aqui (AGENTS.md §6). A Apache-2.0 exige marcar os arquivos modificados.

## The Forge (branch `astra/1.63` do submódulo)

| Commit | Arquivo(s) | Motivo | Risco |
|---|---|---|---|
| `ade4360` | `Common_3/Application/Config.h` | Aceitar MSVC ≥ 19.29 (upstream exigia exatamente VS 2019; o host tem VS 2026) | Baixo: só remove a trava de versão; compilação validada com MSVC 19.51 |
| `ade4360` | `Common_3/OS/Windows/WindowsBase.cpp` | `VK_LAYER_PATH` usava `GetResourceMount(RM_DEBUG)`, removido na 1.63 (código Vulkan-no-Windows apodrecido, evidência do S-03). Agora usa a pasta do executável, como o `LinuxBase.cpp` | Baixo: só afeta builds de debug com validação |
| `c331f38` | `Common_3/OS/Android/AndroidBase.cpp` | `ALooper_pollAll` foi removido no NDK r27+; trocado por `ALooper_pollOnce` nos dois laços de eventos (sem callbacks no looper, mesma semântica) | Baixo: validado com 20 ciclos de pausa/retomada no aparelho |
