# Patches da Astra sobre bibliotecas de terceiros

Cada alteração em código de terceiros entra aqui (AGENTS.md §6). A Apache-2.0 exige marcar os arquivos modificados.

## The Forge (branch `astra/1.63` do submódulo)

| Commit | Arquivo(s) | Motivo | Risco |
|---|---|---|---|
| `e22a9e3` | `Common_3/Application/Config.h` | Aceitar MSVC ≥ 19.29 (upstream exigia exatamente VS 2019; o host tem VS 2026) | Baixo: só remove a trava de versão; compilação validada com MSVC 19.51 |
| `e22a9e3` | `Common_3/OS/Windows/WindowsBase.cpp` | `VK_LAYER_PATH` usava `GetResourceMount(RM_DEBUG)`, removido na 1.63 (código Vulkan-no-Windows apodrecido, evidência do S-03). Agora usa a pasta do executável, como o `LinuxBase.cpp` | Baixo: só afeta builds de debug com validação |
| `9cc2602` | `Common_3/OS/Android/AndroidBase.cpp` | `ALooper_pollAll` foi removido no NDK r27+; trocado por `ALooper_pollOnce` nos dois laços de eventos (sem callbacks no looper, mesma semântica) | Baixo: validado com 20 ciclos de pausa/retomada no aparelho |
| `333f865` | `Common_3/OS/Interfaces/IOperatingSystem.h`, `Common_3/OS/Android/AndroidBase.cpp`, `Common_3/OS/Android/AndroidWindow.cpp`, `Common_3/Application/Interfaces/IApp.h` | O header público incluía `android_native_app_glue.h` só pelos tipos de `WindowHandle`, o que impedia usar o renderer com o GameActivity (glue de mesmo nome). Agora inclui `android/configuration.h`, `android/native_activity.h` e `android/native_window.h`; quem usa o glue o inclui direto | Baixo: só move includes; S-01 (IApp) e S-02 (GameActivity) compilam |
| `333f865` | `Common_3/Application/Config.h` | `ENABLE_FORGE_RELOAD_SHADER` desligável por `ASTRA_FORGE_NO_RELOAD_SHADER` (definido em todo o build). O cliente de recarga depende da UI/input da camada IApp | Baixo: o recurso já era todo protegido por `#ifdef` no upstream |
