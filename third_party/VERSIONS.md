# Bibliotecas de terceiros fixadas

Toda biblioteca entra fixada por commit. Atualização só por tarefa explícita (AGENTS.md §6).

| Biblioteca | Versão | Commit | Origem | Licença | Forma | Desde |
|---|---|---|---|---|---|---|
| GameActivity (androidx.games:games-activity) | 4.4.2 | AAR sha256 `16069bb61a34e9cc7a4f6b4cef0ddb9e2f3f61fe781b282787f574f8880ba38e` | https://dl.google.com/android/maven2/androidx/games/games-activity/4.4.2/ | Apache-2.0 | `third_party/games-activity`: headers e `libgame-activity_static.a` (arm64) do prefab do AAR; o Java vem do mesmo AAR pelo Gradle | 02/10/2026 |
| The Forge | v1.63 (21/03/2025) | `7443530ef7e888771f307de74b7872becfd64450` | https://github.com/ConfettiFX/The-Forge | Apache-2.0 | Submódulo `third_party/the-forge`, branch local `astra/1.63` | 02/10/2026 |

## Componentes do The Forge usados como vieram

| Componente | Caminho no submódulo | Observação |
|---|---|---|
| Vulkan headers + glslangValidator + validation layers (SDK 1.3.275) | `Common_3/Graphics/ThirdParty/OpenSource/VulkanSDK` | `glslangValidator.exe` compila os shaders FSL no host |
| volk, VulkanMemoryAllocator | `Common_3/Graphics/ThirdParty/OpenSource/` | Compilados dentro do renderer |
| AGDK (swappy, paddleboat, memory_advice) estáticos | `Common_3/OS/ThirdParty/OpenSource/agdk` | Binários pré-compilados pelo upstream |
