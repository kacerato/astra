# Astra 2

Engine e editor mobile-first (Android, Vulkan) sobre The Forge 1.63. Plano mestre em [`docs/plano-mestre/00-INDICE.md`](docs/plano-mestre/00-INDICE.md); regras para agentes em [`AGENTS.md`](AGENTS.md).

Fase atual: **F0** (spikes de viabilidade). Resultados em [`docs/adr/`](docs/adr/).

## Preparar

```bash
git submodule update --init third_party/the-forge
```

Ferramentas: CMake ≥ 3.28, Python ≥ 3.8, Visual Studio 2022+ (host Windows), Android SDK com NDK 29.0.14206865 e build-tools 36 (Android).

## Host Windows (Vulkan)

```bash
cmake --preset host-windows
cmake --build --preset host-windows-debug
```

O spike S-01 roda a partir da pasta de assets: `build/host-windows/spikes/s01_forge_triangle/assets` (o depurador do Visual Studio já usa essa pasta).

## Android (arm64)

```bash
export ANDROID_NDK_ROOT="$LOCALAPPDATA/Android/Sdk/ndk/29.0.14206865"
cmake --preset android-arm64-debug
cmake --build --preset android-arm64-debug
cd spikes/s01_forge_triangle/android && ./gradlew assembleDebug
```
