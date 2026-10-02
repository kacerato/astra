# ADR-S01 — The Forge 1.63 por CMake + NDK r29 (16 KB)

- **Status:** aprovado (02/10/2026)
- **Pergunta (doc 22 §1):** o TF 1.63 (Vulkan) compila por CMake + NDK r28+ com 16 KB e roda no aparelho e no host?
- **Prazo:** 10 dias úteis. Usado: 1 sessão.

## Decisão

Seguir com o The Forge 1.63 como backend gráfico. O build do upstream (VS2019 + AGDE + NDK r21e) foi substituído por `cmake/TheForge.cmake` (alvos `tf_os`, `tf_renderer`, `tf_app_glue` e a função `astra_fsl_shaders`).

## Evidência

| Item | Resultado | Como |
|---|---|---|
| Compila Android arm64 | Sim, NDK 29.0.14206865, `android-29`, `c++_static` | `cmake --preset android-arm64-debug && cmake --build --preset android-arm64-debug` |
| Compila Windows x64 (Vulkan) | Sim, MSVC 19.51 (VS 2026) | `cmake --preset host-windows && cmake --build --preset host-windows-debug` |
| 16 KB | `libForgeGame.so`: todos os `LOAD` com `Align 0x4000`; APK passa em `zipalign -c -P 16 -v 4` | `llvm-readelf -lW`, build-tools 36.0.0 |
| Shader FSL | `triangle.vert/frag` gerados e compilados para SPIR-V (glslangValidator do VulkanSDK 1.3.275 embutido no TF) | `astra_fsl_shaders` |
| Textura pelo Resource Loader | `astra_checker.ktx` (KTX 1.1, RGBA8 sRGB, 9 mips) carregada nos dois alvos | log `S01: textura 256x256, 9 mips, formato R8G8B8A8_SRGB` |
| Roda no aparelho | Xiaomi 25053PC47G, Android 16 (API 36), Adreno 825, páginas de 4 KB: swapchain 2772×1280 (paisagem), 4 imagens, ~60 fps com vsync | `docs/validacao/2026-10-02-s01/android-xiaomi-adreno825-paisagem.png` |
| Roda no host | Intel UHD Graphics (0x8086/0xa7a8), janela 640×480 | `docs/validacao/2026-10-02-s01/host-windows-intel-uhd.png` |
| Validação Vulkan | Camada `VK_LAYER_KHRONOS_validation` ativa nos dois alvos; **nenhuma** mensagem de validação | logcat / `AstraSpikeS01.log` |
| Pausa/retomada | 20 ciclos Home → reabrir: mesmo PID, 20 recriações de swapchain, 0 erros do TF | script ADB (sessão de 02/10/2026) |

## Patches no fork (ver `third_party/ASTRA_PATCHES.md`)

1. `Config.h`: aceitar MSVC ≥ 19.29 (o upstream exige exatamente VS 2019).
2. `WindowsBase.cpp`: `VK_LAYER_PATH` usava API removida (`GetResourceMount`).
3. `AndroidBase.cpp`: `ALooper_pollAll` (removido no NDK r27+) → `ALooper_pollOnce`.

## Achados que viram tarefa

| Achado | Encaminhamento |
|---|---|
| `games-memory-advice:2.0.0-beta04` traz `libmemory_advice.so` alinhada a **4 KB** e o TensorFlow Lite (~3,4 MB + modelo) | S-02: remover o Memory Advice (o plano usa `onTrimMemory`, doc 15 §2). Bloqueia release no Play enquanto existir |
| Camada de validação embutida (1.3.275) é alinhada a 4 KB e exige `libc++_shared.so` | Só no APK de debug. Trocar por um build recente dos Vulkan-ValidationLayers (Android) antes de testar em aparelho com páginas de 16 KB |
| `gpu.data` do TF não conhece Adreno 825 nem Intel UHD → preset `low` | Tiers próprios (doc 08 §2) na F7 |
| `ReloadClient` loga erro por falta de `reload-server.txt` | Ruído; some quando o recarregamento de shaders do TF for desligado ou configurado (S-02) |
| Build de host usa o gerador Visual Studio (multi-config) em vez de Ninja | Adaptação explícita do doc 04 §12: evita depender do ambiente `vcvars` |
| Projeto de empacotamento usa AGP 9.4.0 + Gradle 9.7.1 (já em cache) | Mantido; versões registradas aqui |

## Pendente (fora do aceite do S-01)

- Host Linux não testado.
- Rotação e multi-janela: o spike foi fixado em paisagem (`sensorLandscape`) a pedido do usuário; 100 ciclos completos ficam no S-02.
- APK de release (sem validação) não gerado.
