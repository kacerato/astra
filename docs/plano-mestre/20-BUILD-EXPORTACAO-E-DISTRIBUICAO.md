# 20 — Build, exportação e distribuição

**Duas entregas diferentes** (regra do AGENTS.md): compilar o **APK do editor** é uma coisa; **exportar um jogo** feito no editor é outra.

---

## 1. Toolchains (fixadas no F0, registradas em `docs/TOOLCHAIN.md`)

| Ferramenta | Versão | Uso |
|---|---|---|
| CMake + Ninja | ≥ 3.28 | Tudo |
| Android NDK | **r28 ou mais novo** (versão exata fixada no S-01) | Alinhamento de 16 KB por padrão |
| Android Gradle Plugin | ≥ 8.5.1 (versão exata fixada no S-01) | Empacotamento, alinhamento 16 KB |
| JDK | 17 (ou a exigida pelo AGP escolhido) | Gradle |
| Android SDK | Plataforma 36, build-tools correspondentes | `targetSdk 36` |
| Python | 3.11+ | Toolchain FSL do The Forge |
| Compilador SPIR-V usado pelo FSL | A confirmar no S-01 (glslangValidator ou DXC) | Shaders internos |
| MSVC 2022 / clang-cl | Atual | Host Windows |
| Vulkan SDK | Atual | Camadas de validação e ferramentas no host |

Regra: **build offline e reproduzível**. Nenhuma etapa baixa coisas da rede durante o build (lição da Astra atual com `--offline`).

## 2. Alvos

| Alvo | Conteúdo |
|---|---|
| `astra_*` (bibliotecas estáticas) | Um por módulo de [04](04-ARQUITETURA-CAMADAS-E-REPOSITORIO.md) §2 |
| `astra_editor` | Núcleo do editor, UI, painéis, plugins, importadores |
| `host-editor` | Executável Windows/Linux |
| `android-editor` | APK do editor (debug, profile, release) |
| `astra_player` (`libastra_player.so`) | Runtime sem editor, importadores, compilador/analisador Luau nem Tracy |
| `tools/*` | `astra-cook` (CLI de cozimento), `astra-build` (CLI de exportação no PC), `api-dump`, `shader-build`, `tokens`, `licenses`, `check-layering` |

## 3. Integração contínua

| Gatilho | Jobs |
|---|---|
| Todo PR | Build host + testes unitários e de integração headless; `check-layering`; `licenses`; tokens/ícones gerados em dia |
| PR que toca `engine/`, `backends/`, `platform/` | + build Android (`assembleDebug`) com clang do NDK e `-Werror` nos alvos Astra |
| Noturno | ASan/UBSan no host, fuzzing dos importadores, suíte completa, golden images no host, relatório de tamanho do APK e do player |
| Manual (máquina do usuário) | Testes no aparelho via ADB (`tests/device/`), medições, capturas |

O CI publica o APK debug como artefato. Os testes no aparelho são locais, porque o aparelho é do usuário.

## 4. APK do editor

- `applicationId` próprio do produto novo (não reutilizar `dev.aether.editor`, para conviver com a Astra atual no mesmo aparelho durante a transição).
- Assinatura: debug para desenvolvimento; release com a chave do usuário (o usuário faz a assinatura de release). Debug e release com a mesma chave no aparelho de teste permitem `install -r` sem apagar dados (lição da Astra atual). **Nunca desinstalar o editor num aparelho com projetos sem backup.**
- Conteúdo empacotado: shaders SPIR-V, UI do editor (RML/RCSS/ícones/fontes), recursos padrão (materiais de fallback, HDRI de pré-visualização), **template do player** (§5.3).
- Orçamento de tamanho acompanhado no CI noturno.

## 5. Exportação de jogos

### 5.1 Seleção e cozimento de conteúdo

1. Cenas da lista de build + pastas "sempre incluir" + dependências transitivas (pelo grafo do AssetDatabase).
2. Cozimento por plataforma: texturas ASTC, shaders SPIR-V (+ variantes usadas), malhas quantizadas, cenas e prefabs binários, **bytecode Luau**, áudio conforme o *load type*, dados de NavMesh e física cozidos.
3. Empacotamento em **pak** (formato zip, lido pelo FileSystem do TF, com índice), compressão por entrada (LZ4/zstd, já presentes no TF) e hash de conteúdo por arquivo.
4. Relatório de build: tamanho por tipo de asset, maiores assets, variantes de shader, avisos (assets não usados, texturas sem compressão).

### 5.2 Configurações do player

Nome, `applicationId`, versão (`versionName`/`versionCode`), ícone adaptativo (gerado de uma imagem), splash (API de splash do Android 12+), orientação, tier mínimo, recursos declarados (Vulkan 1.1 obrigatório), permissões **derivadas dos recursos usados** (vibração, internet) e nada além disso.

### 5.3 APK de teste montado no aparelho

```
template do player (APK não assinado, versionado com o editor)
   → cópia → trocar assets/ pelo pak → reescrever manifesto e recursos (nome, id, versão, ícones, orientação)
   → zipalign (alinhamento 4 B; .so sem compressão e alinhadas a 16 KB)
   → assinatura v2/v3 (apksig) com a keystore do usuário
   → salvar via SAF
```

- Reescrever manifesto e recursos binários (AXML/ARSC) no aparelho exige uma biblioteca de edição de APK (ex.: ARSCLib, Apache-2.0). Isso é o **S-11**. Plano B: template com placeholders de tamanho fixo e só os campos essenciais.
- **Instalação:** o editor salva o APK (por exemplo em Downloads) e o usuário instala pelo gerenciador de arquivos. Pedir `REQUEST_INSTALL_PACKAGES` tem restrição de política no Google Play, por isso fica fora do padrão (limite L-11).
- Keystore: importada pelo usuário; senha pedida a cada exportação ou guardada protegida pelo Android Keystore, por escolha explícita do usuário.

### 5.4 AAB e loja (no PC)

`astra-build` (CLI) usa o mesmo cozimento + módulo Gradle do player + bundletool para gerar **AAB** assinado com a chave de upload. Play Asset Delivery (pacotes de assets *install-time*/*on-demand*) fica para depois da F12, quando o conteúdo passar do limite do módulo base.

### 5.5 Símbolos e falhas

As `.so` vão sem símbolos; os arquivos de símbolos ficam guardados por versão para simbolizar *tombstones*. Relatório de falhas embutido nos jogos fica pendente (opcional e com consentimento).

## 6. Versões e migração de projetos

- `project.astra` guarda a versão da engine. Abrir com engine mais nova → migração com backup ([06](06-CENA-PREFABS-SERIALIZACAO.md) §7) e aviso. Mais antiga → só leitura.
- Versionamento semântico da engine; notas de versão listam migrações de formato.

## 7. Aceite (F12)

- Exportar o projeto de exemplo da F6/F10 **no aparelho** → APK assinado → instalar em **outro** aparelho → roda sem editor, com física, scripts, áudio, UI e navegação.
- Mesmo projeto, dois cozimentos seguidos → paks com o mesmo hash (reprodutível).
- Player sem símbolos de editor (verificação por `nm` no CI).
- AAB gerado pela CLI no PC aceito pelo `bundletool validate`.
- Relatório de build correto (soma dos tamanhos confere com o pak).
