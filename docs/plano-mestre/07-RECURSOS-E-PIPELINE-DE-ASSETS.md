# 07 — Recursos e pipeline de assets

Referências: Unity 6.0 *Asset Database*, *Asset Metadata*, *Model Import Settings*, *Texture Import Settings*, *Audio Clip*; Godot 4.7 `Resource`, `ResourceLoader`, `ResourceSaver`, *Import process*; Stride `Asset`/`AssetReference`; ezEngine *Asset Curator* (processamento de assets em processo/fila separada).

---

## 1. Recursos em runtime

### 1.1 Tipos

| Recurso | Fonte | Cozido (Android) | Consumidor |
|---|---|---|---|
| `Texture2D`, `TextureCube`, `Texture2DArray` | PNG/JPG/TGA/HDR/EXR/KTX2 | KTX2 (ASTC; HDR em RGBA16F, ou ASTC HDR quando suportado) | `render::World` |
| `RenderTexture` | Criado no editor | Descritor | Câmera, UI |
| `Mesh` | glTF/FBX/OBJ, primitivas | Blob quantizado + submeshes + bounds + LODs (+ meshlets em T3) | Render, física (colisor de malha), navegação |
| `Material` | `.amat` / sub-recurso de import | Binário: shader + parâmetros tipados | Render |
| `Shader` | FSL interno / Shader Graph | SPIR-V + reflexão | Render |
| `Skeleton`, `AnimationClip`, `AvatarMask` | glTF/FBX, `.aanim` | Formatos runtime do Ozz + trilhas de propriedade | Animação |
| `AnimatorController` | `.acontroller` | Binário | Animação |
| `AudioClip`, `AudioMixer` | WAV/MP3/FLAC/OGG, `.amixer` | Original comprimido ou PCM, conforme o *load type* | Áudio |
| `Font` | TTF/OTF | Arquivo original | UI |
| `UIDocument`, `StyleSheet` | `.rml`, `.rcss` | Original (RmlUi faz o parse) | UI |
| `Script` | `.luau` | Bytecode Luau | ScriptHost |
| `Prefab`, `Scene` | `.aprefab`, `.ascene` | Binário de cena | World |
| `PhysicsMaterial` | `.amat`-like (`.aphys`) ou sub-recurso | Binário | Física |
| `NavMeshData` | Bake | Binário Detour | Navegação |
| `InputActions` | `.ainput` | Binário | Input |
| `TerrainData` | Editor (F13) | Heightmap + camadas | Render, física |

### 1.2 Contrato do `Resource`

- `RefCounted`, com `AssetGuid` + `SubId`, caminho de origem (só editor), versão de conteúdo e sinal `changed`.
- `Ref<T>`: referência forte, carregada. `AssetRef<T>`: referência persistente e preguiçosa (GUID + tipo), resolvida pelo loader.
- **Sub-recursos** pertencem a um asset (mesh 3 de um GLB). **Recursos locais** vivem dentro de uma cena e são duplicados por instância quando marcados `localToScene` (semântica Godot).
- `duplicate(deep)` com regra explícita para sub-recursos.

### 1.3 ResourceLoader e cache

- Carregamento síncrono (só ferramentas e testes) e assíncrono com prioridade, progresso, cancelamento e resolução de dependências (material → texturas).
- Cache fraco `AssetGuid#SubId → Resource`. Pedidos simultâneos do mesmo recurso compartilham o mesmo carregamento.
- Modos de cache: reaproveitar (padrão), substituir (hot reload), ignorar (pré-visualização isolada).
- Upload para a GPU pelo Resource Loader do The Forge (buffers e texturas assíncronos); a Astra decide **o que** e **quando**, o TF decide **como**.
- Falha de carga: recurso de fallback do tipo + erro no console com GUID, caminho e motivo (nunca silencioso; ver [06](06-CENA-PREFABS-SERIALIZACAO.md) §5).

## 2. AssetDatabase (editor)

| Função | Comportamento |
|---|---|
| Varredura | Percorre `Assets/` na abertura e ao voltar do segundo plano; cria `.meta` para arquivos novos |
| `.meta` | `{ guid, importer, importerVersion, settings{}, subAssets{nome→SubId}, labels[] }` |
| Índices | GUID → caminho, caminho → GUID, tipo, rótulos, nome (busca) |
| Importação | Grafo de jobs em segundo plano; progresso por asset no indicador de tarefas da barra superior |
| Artefatos | `Library/artifacts/<guid>/<hash(importerVersion, settings, plataforma, deps)>` |
| Dependências | Fonte → fontes (glTF → imagens externas), artefato → artefatos; reimport em cascata |
| Gatilhos de reimport | Hash do arquivo mudou, configurações mudaram, versão do importador subiu, dependência mudou |
| Renomear/mover | O `.meta` acompanha; GUID preservado; referências continuam válidas |
| Excluir | Vai para a lixeira do projeto ([06](06-CENA-PREFABS-SERIALIZACAO.md) §8.3) |
| "Usado por" | Índice reverso de referências (cenas, prefabs, materiais) |
| Thumbnails | Render offscreen num mundo de pré-visualização; cache em `Library/thumbnails`; visíveis primeiro |

**Android:** não há observador de arquivos confiável para pastas arbitrárias. A varredura acontece na abertura, no retorno ao primeiro plano e no botão "Atualizar". Arquivos que chegam pelo seletor do sistema (SAF) já entram pelo editor e são importados na hora.

## 3. Configurações de importação

Exibidas no Inspector quando o asset é selecionado, com barra **Aplicar / Reverter** (modelo Unity).

### 3.1 Modelo (glTF/FBX/OBJ)

| Grupo | Configurações |
|---|---|
| Cena | Fator de escala, converter unidades, eixos (FBX), importar câmeras, importar luzes, preservar hierarquia (sempre; não achata) |
| Malhas | Quantização (posição/normal/UV), otimizar (cache/overdraw/fetch), ler/gravar em CPU, gerar LODs (níveis e erro), gerar colisor |
| Geometria | Normais (importar/calcular, ângulo de suavização), tangentes (importar/MikkTSpace), blend shapes (importar, normais) |
| Materiais | Importar como sub-recursos / extrair para `.amat` / remapear para materiais existentes / nenhum |
| Animação | Importar clipes; faixas de frames e nomes; loop; root motion (nó raiz, travar Y/rotação); taxa de amostragem; compressão (tolerâncias do Ozz) |
| Rig | Esqueleto (raiz, ossos), máscara padrão |

### 3.2 Textura

Tipo (`Default, NormalMap, Cubemap, Lightmap, UI, SingleChannel`), sRGB, fonte de alfa, alfa é transparência, mipmaps (filtro, preservar cobertura de alfa), wrap, filtro, anisotropia, tamanho máximo **por plataforma**, formato e qualidade de compressão por plataforma (bloco ASTC 4×4/6×6/8×8; UASTC), NPOT (manter/escalar).

### 3.3 Áudio

*Load type* (`DecompressOnLoad, CompressedInMemory, Streaming`), forçar mono, taxa de amostragem (manter/otimizar/forçar), pré-carregar, carregar em segundo plano.

### 3.4 Script

Diagnósticos do analisador Luau no import (erros bloqueiam o Play com mensagem e linha); bytecode em `Library/` para o player.

## 4. Importadores

### 4.1 glTF 2.0 / GLB (fastgltf)

| Item | Estado planejado |
|---|---|
| Hierarquia, TRS, matrizes, múltiplas cenas | F3 |
| Malhas com várias primitivas → submeshes + materiais correspondentes | F3 |
| Materiais metallic-roughness, alpha (opaque/mask/blend), double-sided | F3 |
| `KHR_texture_transform`, `KHR_materials_emissive_strength`, `KHR_materials_unlit` | F3 |
| `KHR_materials_clearcoat`, `sheen`, `transmission`, `volume`, `ior`, `specular` | F7 (com suporte no shader Lit) |
| `KHR_mesh_quantization`, `EXT_meshopt_compression` (decodificação meshoptimizer) | F3 |
| `KHR_texture_basisu` (transcodificação BasisU) | F3 |
| `KHR_lights_punctual` → componente `Light` (unidades convertidas) | F3 |
| Câmeras → componente `Camera` | F3 |
| Skins → `Skeleton` + `SkinnedMeshRenderer` | F8 (F3 importa e guarda) |
| Morph targets | F8 |
| Animações → `AnimationClip` (Ozz) | F8 (F3 importa e guarda) |
| `KHR_materials_variants` | F7 |
| `EXT_texture_webp`, `KHR_draco_mesh_compression` | Pendente (exigem decodificador extra; decidir por demanda) |
| `KHR_animation_pointer` | Pendente (depende de animação de propriedades da F8) |

Mapeamento: o arquivo importado vira **asset de modelo que se comporta como prefab** (como na Unity). Instanciar o modelo cria uma instância com overrides possíveis; meshes, materiais, texturas embutidas e clipes são sub-recursos.

### 4.2 FBX / OBJ (ufbx)

Mesmo mapeamento do glTF. Conversão de eixos e unidades pela ufbx; materiais legados convertidos para PBR (modelo unificado da ufbx); curvas de animação amostradas na taxa configurada e convertidas para o formato do Ozz; blend shapes → morph targets.

### 4.3 Texturas

1. Decodificar (stb_image, tinyexr, libktx).
2. Gerar mips em espaço linear (sRGB convertido antes); normais renormalizadas; preservação de cobertura de alfa opcional.
3. Comprimir por plataforma: **Android → ASTC** (astcenc), **host → BC** (BC7/BC5/BC6H). UASTC/BasisU como opção "universal" para jogos que miram GPUs desconhecidas.
4. Enquanto a compressão não termina, a textura aparece descomprimida com o selo "compressão pendente". O S-09 mede o custo do astcenc no aparelho e define os presets.

### 4.4 Áudio, fontes e scripts

- Áudio: decodificadores do miniaudio (WAV, MP3, FLAC, Vorbis). O cozido mantém o comprimido original ou PCM, conforme o *load type*. Opus fica pendente (exige libopus).
- Fontes: TTF/OTF copiadas; a RmlUi carrega o arquivo.
- Scripts: compilador e analisador Luau ([14](14-SCRIPTING-LUAU.md)).

## 5. Estabilidade de sub-recursos no reimport

Problema clássico: reimportar um GLB alterado não pode quebrar referências a "mesh 3".

- O `SubId` vem do **nome + caminho no grafo do arquivo** (`/Body/Mesh_Hull#prim0`), hasheado e registrado no `.meta` (`subAssets`).
- No reimport, nomes iguais mantêm o `SubId`. Nomes novos ganham `SubId` novo. Nomes sumidos ficam no `.meta` como "removidos" (referências viram "ausente", não "outro objeto").
- Renomear no DCC quebra a correspondência. O Inspector do asset mostra o relatório de reconciliação ("2 removidos, 2 novos — mapear?") com remapeamento manual. Esse conhecimento vem do plano `M08-2-IDENTIDADE-RECONCILIACAO` da Astra atual.

## 6. Processamento de malhas

| Etapa | Ferramenta | Observação |
|---|---|---|
| Solda de vértices e índices | meshoptimizer (`generateVertexRemap`) | |
| Normais e tangentes | Próprio + MikkTSpace | Tangentes MikkTSpace são o padrão do glTF |
| Ordem de vértices/índices | `optimizeVertexCache`, `optimizeOverdraw`, `optimizeVertexFetch` | |
| Quantização | Posições em `int16`/`half` com transformação de bounds, normais/tangentes octaédricas, UV `half` | Corta banda de vértice, crítico em mobile |
| LODs | `meshopt_simplify` (com e sem *sloppy*) por erro alvo | Integra com `LODGroup` (F7) |
| Meshlets | `meshopt_buildMeshlets` | Só para o experimento de Visibility Buffer/culling em GPU (T3) |
| Bounds | AABB + esfera por submesh e por LOD | Culling e picking |
| Colisor | Malha de triângulos e convexo cozidos para o Jolt | [09](09-FISICA-JOLT.md) |

## 7. Carga em runtime

```
pak (res://) → VFS → leitura assíncrona → validação de cabeçalho/CRC → decodificação mínima
   → Resource Loader do TF (upload GPU) → recurso pronto → consumidores notificados
```

- Prioridades: cena inicial > visível > próximo > restante.
- Orçamento de memória por tier; *streaming* de mips de textura como evolução da F13.

## 8. Hot reload no editor

Artefato novo → recurso recarregado **no mesmo objeto** (`Ref` existentes continuam válidos) → sinal `changed` → consumidores invalidam (material → instâncias de render; mesh → bounds, colisores, NavMesh marcada como desatualizada).

## 9. Segurança e robustez

- Limites por import: vértices, índices, dimensões de textura (16384), memória (arena com teto) e tempo (cancelável).
- Fuzzing dos caminhos de leitura (fastgltf, ufbx, stb, tinyexr, libktx, decodificadores de áudio) com libFuzzer no host, rodado no CI noturno.
- Import roda em job: um arquivo malformado gera relatório de erro, nunca crash do editor (P-07).

## 10. Corpus de validação

| Corpus | Uso | Atenção |
|---|---|---|
| Khronos glTF-Sample-Assets | Extensões e casos de borda | Licença varia por asset; usar só para teste local/CI |
| Projetos da Astra atual (`games/`, `samples/`) | Cenas reais do usuário | Conversão única |
| GLBs reais do usuário (plano M08 da Astra atual) | Compatibilidade prática | — |
| Arquivos malformados gerados pelo fuzzer | Robustez | — |

## 11. Aceite (F3)

- Importar o corpus no aparelho: hierarquia, pivôs e materiais corretos, conferidos contra o Khronos glTF Sample Viewer (captura lado a lado).
- Renomear/mover asset e reabrir: nenhuma referência quebrada.
- Reimport com malha renomeada: relatório de reconciliação correto, sem referência trocada silenciosamente.
- Matar o app no meio de uma importação: sem `.meta` corrompido; a importação recomeça na abertura.
- Import de arquivo malformado: erro legível, editor intacto.
