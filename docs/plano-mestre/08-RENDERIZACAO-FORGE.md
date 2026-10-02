# 08 — Renderização sobre The Forge

## 1. Princípios

1. **Um pipeline escalável** (herança da ADR-10): nada de URP/HDRP. Os tiers mudam **parâmetros e passes opcionais**, não o formato dos materiais.
2. **Mobile primeiro (TBDR):** banda de memória é o recurso mais caro. Load/store corretos, poucos render targets, FP16, ASTC e resolução dinâmica valem mais que efeitos.
3. **Sem degradação silenciosa** (AGENTS §11): o que não roda no aparelho aparece **indisponível com motivo** no editor; nunca some sem aviso.
4. **O editor vê o que o jogo vê:** o viewport usa o mesmo caminho do player; overlays do editor são passes extras.
5. **Medir antes de adotar:** cada técnica do laboratório (§12) entra com captura AGI/RenderDoc e custo por tier.

## 2. Arquitetura

```
Componentes (Camera, Light, MeshRenderer, SkinnedMeshRenderer, Decal, ParticleSystem, Volume, ReflectionProbe…)
        │ RenderSyncSystem (só o que mudou: versões de Transform e de componente)
        ▼
render::World  — servidor: handles, descritores, sem flecs, sem TF
        │ Extract (fim do frame do jogo) → FramePacket imutável (views, instâncias visíveis, luzes, parâmetros)
        ▼
ForgeRenderer  — backends/forge: frame graph, passes, pipelines, descriptors, upload
        ▼
The Forge 1.63 (Vulkan) → Swappy → present
```

### 2.1 API do servidor (esboço)

```cpp
namespace astra::render {
  MeshId      createMesh(const MeshDesc&);              // geometria cozida (submeshes, LODs, bounds)
  TextureId   createTexture(const TextureDesc&);
  MaterialId  createMaterial(ShaderId, const MaterialParams&);
  InstanceId  createInstance(const InstanceDesc&);      // mesh + materiais[] + flags + layer + skinning opcional
  void        setInstanceTransform(InstanceId, const Affine3x4&, u32 version);
  void        setInstanceOverrides(InstanceId, const PropertyBlock&);   // como MaterialPropertyBlock
  LightId     createLight(const LightDesc&);
  ViewId      createView(const ViewDesc&);              // câmera: projeção, viewport, alvo, máscara, pós
  ProbeId     createReflectionProbe(const ProbeDesc&);
  DecalId     createDecal(const DecalDesc&);
  EnvironmentId createEnvironment(const EnvironmentDesc&);  // céu, ambiente, névoa
  VolumeId    createVolume(const VolumeDesc&);          // pós com peso por volume
  DebugDraw&  debugDraw();                              // linhas, formas, texto; editor e gizmos de jogo
  UiLayerId   submitUi(const UiDrawList&);              // listas da RmlUi
  PickResult  pick(ViewId, Vec2 pixel);                 // ID buffer, resultado assíncrono
}
```

Toda função é chamada na thread principal; o servidor bufferiza e entrega o `FramePacket` à thread de render.

### 2.2 Frame graph próprio

O TF não fornece render graph nem cena ("Renderer / Scene (not provided)"). O `ForgeRenderer` tem um frame graph enxuto:

- Passes declaram recursos lidos/escritos (texturas transitórias, importadas, buffers).
- O compilador do grafo calcula tempo de vida, **aliasing** de memória transitória, barreiras (`cmdResourceBarrier` do TF) e **ações de load/store** (essencial em TBDR: `DONTCARE` sempre que possível).
- Passes sem consumidor são removidos (culling do grafo).
- Compute assíncrono opcional por tier.
- Depuração: visualização do grafo no Profiler (passes, recursos, tempo GPU por pass via timestamps do TF).

## 3. Passes do pipeline base (F2 → F7)

| # | Pass | Tier | Notas |
|---|---|---|---|
| 1 | **Shadow** | T0+ | CSM para a direcional (1–4 cascatas por tier), atlas para spot/point; PCF por tier |
| 2 | **Depth prepass** | Configurável | Liga ou desliga por GPU após medição (LRZ no Adreno, HSR em PowerVR/Apple) |
| 3 | **Light culling** (compute) | T0+ | Clusters (froxels) com listas de luzes e decals |
| 4 | **Opaco Forward+** | T0+ | PBR Lit, luzes clusterizadas, sombras, IBL, névoa; MSAA 4× opcional (resolvido no tile) |
| 5 | **Céu** | T0+ | HDRI ou atmosfera procedural (F13) |
| 6 | **Transparentes** | T0+ | Ordenados de trás para frente; refração por cópia da cor opaca (T2+) |
| 7 | **Partículas GPU** | F13 | — |
| 8 | **GTAO** | T2+ | Meia resolução; desligado em T0/T1 com aviso |
| 9 | **TAA / upscaler** | T1+ | §7 |
| 10 | **Bloom** | T0+ | Dual filter em resoluções decrescentes |
| 11 | **Exposição** | T0+ | Manual (EV100) ou automática (histograma compute) |
| 12 | **Tonemap + grading** | T0+ | Khronos PBR Neutral / ACES / AgX; LUT 3D 32³; vinheta, grão, nitidez (CAS) |
| 13 | **UI** | — | Listas da RmlUi (jogo) e, no editor, painéis |
| 14 | **Overlays do editor** | Editor | Grade, gizmos, contornos, ícones, depuração física/nav, ID buffer |
| 15 | **Present** | — | Swapchain com pré-rotação; Swappy para frame pacing |

**Decisão D-15:** Forward+ clusterizado é o caminho principal porque lida bem com MSAA no tile, transparência e banda limitada. O **Visibility Buffer 2** do TF (que roda em Android S22+) fica como experimento de T3, comparado por medição no S-05/F13.

## 4. Shaders e materiais

### 4.1 Shaders internos (D-16)

- Escritos em **FSL** e compilados **offline** pelo toolchain do TF (Python + compiladores) para SPIR-V, no build.
- Biblioteca: `Lit`, `Unlit`, `ParticleLit`, `ParticleUnlit`, `Sky`, `UI`, `Decal`, `Terrain` (F13), `Water` (F13), passes de pós e utilitários.
- Variantes por *keywords* com orçamento de permutações por shader. As variantes realmente usadas são coletadas (como as *Shader Variant Collections* da Unity) para pré-aquecer pipelines.
- `VkPipelineCache` persistido por aparelho/driver; aquecimento de PSO no carregamento da cena para evitar engasgos.
- Hot reload de FSL no host de desenvolvimento.

### 4.2 Shaders do usuário (Shader Graph, F14)

O toolchain FSL é Python e **não roda no aparelho**. Por isso:

- O Shader Graph gera **GLSL** a partir do grafo; o **glslang** compila para SPIR-V no aparelho e no host Vulkan.
- O S-04 (F0) confirma que o TF 1.63 consome SPIR-V produzido fora do FSL com reflexão correta dos descriptors. Se não consumir, o plano é um patch no fork para aceitar SPIR-V + layout declarado. Sem isso, o Shader Graph fica restrito ao host (limite L-05).

### 4.3 Modelo de material

| Elemento | Conteúdo |
|---|---|
| Material | Shader + bloco de parâmetros **tipados** (float, int, vec2–4, cor linear/sRGB, textura + sampler, keyword, enum) + estado de render (superfície, blend, cull, depth write/test, fila/prioridade) |
| Instâncias | `MaterialPropertyBlock` por renderer: overrides sem duplicar o material (cor de inimigo, por exemplo) |
| Compartilhamento | O material é recurso compartilhado; editar no Inspector afeta todos os usuários (com indicação "Usado por N") |
| Sem slots fixos | O número de parâmetros vem do shader, não de uma estrutura fixa (requisito do AGENTS) |

### 4.4 Shader `Lit` (referência: URP Lit 17 + glTF PBR)

| Grupo | Propriedades |
|---|---|
| Superfície | Tipo (opaco/transparente), blend (alfa/premultiplicado/aditivo/multiplicativo), face (frente/trás/ambas), alpha clip + limiar |
| Base | Mapa base + cor, tiling/offset por textura (`KHR_texture_transform`) |
| Metálico/rugosidade | Mapa ORM ou mapas separados, valores escalares. **Rugosidade**, como no glTF, em vez de *smoothness* (adaptação explícita, com rótulo claro) |
| Normal | Mapa + escala; detalhe (mapa + máscara + escala) |
| Oclusão | Mapa + força |
| Altura | Parallax (T2+) |
| Emissão | Cor + intensidade (nits), afeta bloom |
| Extensões | Clearcoat, sheen, transmissão (T2+), specular, IOR |
| Outros | Cor de vértice, receber sombras, prioridade de ordenação |

## 5. Luzes

Unidades físicas, alinhadas ao `KHR_lights_punctual`: **lux** para direcional e **candela** para point/spot (com conversão para lúmens no Inspector), mais a exposição da câmera (EV100). É uma adaptação explícita em relação à intensidade adimensional da URP.

| Propriedade | Direcional | Point | Spot | Notas |
|---|---|---|---|---|
| Cor / temperatura (K) | ✓ | ✓ | ✓ | Temperatura opcional |
| Intensidade | lux | cd / lm | cd / lm | |
| Alcance | — | ✓ | ✓ | Atenuação física com janela suave |
| Ângulo interno/externo | — | — | ✓ | |
| Sombras | nenhuma/dura/suave | idem | idem | Só com suporte efetivo; senão desabilitado com motivo |
| Força, bias, normal bias, plano próximo | ✓ | ✓ | ✓ | |
| Máscara de culling (layers) | ✓ | ✓ | ✓ | |
| Cookie | Pendente | Pendente | Pendente | Sem consumidor na 2.0 = não aparece |
| Luz de área (retângulo, LTC) | — | — | — | T3, pendente |

- Ambiente: céu (cubemap/HDRI ou procedural), gradiente ou cor; IBL com especular pré-filtrado + irradiância em SH L2.
- **ReflectionProbe:** assada no editor (cubemap + pré-filtro em compute) ou em tempo real (T2+, com orçamento), projeção em caixa, mistura entre probes.
- **LightProbeGroup** (SH L2) assado por captura de cubemaps no aparelho: F13.
- Lightmaps: fora da 2.0 ([01](01-VISAO-PRINCIPIOS-ESCOPO.md) §6).

## 6. Sombras

| Técnica | Tier | Detalhe |
|---|---|---|
| CSM estável (snapping de texel) | T0+ | 1/2/3/4 cascatas; distância e resolução por tier; mistura entre cascatas |
| Atlas de sombras locais | T1+ | Alocação por tamanho em tela e prioridade; faces de cubo para point |
| PCF 3×3 → Poisson 5×5 | T0 → T2 | |
| Sombras de contato (screen-space) | T2+ | Referência: TF 1.56 *Screen-space Shadows* |
| Cache de sombras estáticas | F13 | Só objetos com `staticFlags.Render` |
| PCSS | T3, F13 | |

## 7. Anti-aliasing e upscaling

| Opção | Tier | Licença | Observação |
|---|---|---|---|
| MSAA 2×/4× | T0+ | — | Barato em TBDR se resolvido no tile |
| FXAA / SMAA 1× | T0+ | Domínio público / MIT | Pós barato |
| TAA próprio | T1+ | — | Jitter + reprojeção + clamp de vizinhança; referência: Wicked/Unreal |
| Upscaler espacial (FSR 1 / Snapdragon GSR) | T0+ | MIT / a confirmar | Com render scale |
| **Arm ASR** (temporal) | T2+ | A confirmar na adoção (F7) | Já testado na Astra atual (memória `validacao-no-aparelho-astra`) |

A escolha por tier vem de medição (F7). O modo temporal ativo aparece como estado explícito no editor.

## 8. Câmera

| Propriedade | Referência U5 | Fase |
|---|---|---|
| Projeção perspectiva/ortográfica, FOV vertical / tamanho ortográfico | ✓ | F2 |
| Near/far, viewport rect, prioridade (depth) | ✓ | F2 |
| Máscara de culling (layers) | ✓ | F2 |
| Limpeza: céu, cor, só profundidade, nada | ✓ | F2 |
| Alvo: tela ou `RenderTexture` | ✓ | F7 |
| HDR, MSAA, pós ligado/desligado, render scale | ✓ | F7 |
| Pilha de câmeras (overlay para UI/arma) | URP Camera Stacking | F7 (limites documentados) |
| Câmera física (sensor, distância focal, abertura → DoF) | ✓ | F13 |
| Seleção da câmera de jogo | "Main camera" por tag ou prioridade | F2 |

A câmera do editor é **outra** (estado de sessão do editor). Ela nunca substitui a câmera autorada.

## 9. Overlays e vistas do editor

- **Picking por ID buffer:** pass que escreve `R32_UINT` (id de instância) com leitura assíncrona de 1 pixel (latência de 1 frame). Funciona para malhas com skin e partículas; substitui o picking por esferas da Astra atual.
- Contorno de seleção (silhueta por stencil/borda), grade infinita com fade, ícones de luz/câmera/áudio em billboard, gizmos sempre por cima com fade por profundidade.
- Depuração: colisores (física), NavMesh, bounds, frustums, probes, clusters de luz.
- **Modos de vista:** Lit, Unlit, Wireframe, Shaded+Wire, Overdraw, Normais, Albedo, Rugosidade, Metálico, AO, Só iluminação, Complexidade de luz (heatmap dos clusters), Cascatas de sombra, Nível de LOD, Nível de mip, Checker de UV.

## 10. Tiers e capabilities

- **Sondagem na inicialização:** versão Vulkan, features (descriptor indexing, FP16/storage 16-bit, subgroup, timeline semaphore, VRS, ray query), limites e formatos (ASTC HDR, R11G11B10 como render target). Registrada no log e no relatório de aparelho.
- **Classificação:** `gpu.data`/`gpu.cfg` do TF (fornecedor, modelo, driver → preset), com dados versionados pela Astra.
- **Asset `QualitySettings`** por tier: render scale, MSAA, cascatas/resolução/distância de sombra, máximo de luzes por cluster, pós ligados, tamanho máximo de textura, LOD bias, orçamento de partículas.
- **Resolução dinâmica** guiada por tempo de GPU e pelo *thermal headroom* (ADPF).
- No editor: painel "Gráficos do aparelho" com tier, features e **o que está desligado e por quê**.

## 11. Especificidades mobile a validar no S-05

| Item | Pergunta |
|---|---|
| Load/store | O TF 1.63 expõe `LOAD_ACTION_DONTCARE`/store discard em todos os alvos usados? |
| Memória *lazily allocated* | Depth e MSAA transitórios podem usar `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT`? Se não, patch no fork |
| Pré-rotação da surface | O swapchain trata `preTransform` sem pagar rotação no compositor? |
| Subpasses | Existem no TF? Se não, o Forward+ não depende deles (decisão consciente) |
| FP16 | Tipos `half` do FSL geram `mediump`/`float16` efetivos no Adreno/Mali? |
| Comparação Forward+ × VB2 | Custo GPU e banda (AGI) numa cena de referência em T2 e T3 |

## 12. Laboratório gráfico (F13): Wicked, TF e outros

Cada técnica entra com: referência (arquivo/artigo), tier, custo medido e controles no Volume/QualitySettings. Wicked Engine (MIT) permite portar código com atribuição. As amostras do TF (Apache-2.0) também.

| Técnica | Referência principal | Tier alvo | Prioridade |
|---|---|---|---|
| Light culling em tiles/clusters | Wicked (`wiRenderer`, shaders de culling); Doom 2016 | T0+ (F7) | 1 |
| GTAO | Wicked (GTAO); Jimenez et al. 2016 | T2+ | 2 |
| Bloom físico | Wicked; CoD Advanced Warfare (dual filter) | T0+ (F7) | 3 |
| TAA | Wicked; Karis 2014 | T1+ (F7) | 4 |
| SSR estocástico (meia/quarto de resolução) | Wicked; TF UT 10 (PPR / AMD FFX SSSR) | T2+ | 5 |
| Névoa volumétrica (froxels) | Wicked (volumetric fog/light shafts); Hillaire 2015 | T2+ | 6 |
| Partículas GPU (emitir/simular/ordenar/desenhar) | Wicked (emitted particles); TF UT 39 (Mega Particle System) | T1+ | 7 |
| Decals clusterizados | Wicked (decals no Forward+) | T1+ | 8 |
| Atmosfera e céu (LUTs) | Hillaire 2020; Wicked (sky) | T0+ | 9 |
| Nuvens volumétricas | Wicked; Schneider (Horizon) | T3 | 10 |
| Oceano FFT | Conhecimento da Astra atual (cascatas) + Wicked (ocean) | T1+ | 11 |
| Terreno (heightmap, camadas, CDLOD/clipmap) | Wicked (terrain); Godot terrain plugins como estudo | T1+ | 12 |
| Impostores para vegetação | Wicked (impostors) | T1+ | 13 |
| DDGI | Wicked (DDGI); Majercik 2019 | T3 | 14 |
| Cabelo/pelo | Wicked (hair particles); TF TressFX | T3 | 15 |
| Visibility Buffer 2 + culling em GPU com meshlets | TF Visibility_Buffer2 | T3 | Experimento |

Referência adicional para PBR mobile e color grading: **Filament** (Apache-2.0), documentação *Physically Based Rendering in Filament*.

## 13. Inventário da família Renderização

| Componente | Unity 6.0 | Godot 4.7 | Fase | Classificação planejada |
|---|---|---|---|---|
| Camera | [Camera](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Camera.html) | Camera3D | F2/F7 | Equivalente (pilha com limites) |
| Light | [Light](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Light.html) | DirectionalLight3D/OmniLight3D/SpotLight3D | F2/F7 | Adaptação (unidades físicas) |
| MeshRenderer + MeshFilter | [Mesh Renderer](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html) | MeshInstance3D | F2 | Adaptação (MeshFilter fundido no MeshRenderer) |
| SkinnedMeshRenderer | Skinned Mesh Renderer | MeshInstance3D + Skeleton3D | F8 | Equivalente |
| LODGroup | LOD Group | `visibility_range` | F7 | Equivalente |
| ReflectionProbe | Reflection Probe | ReflectionProbe | F7 | Equivalente |
| LightProbeGroup | Light Probes | (LightmapProbe) | F13 | Equivalente |
| Volume (pós) | URP Volume | WorldEnvironment | F7 | Adaptação |
| Skybox / ambiente / névoa | Lighting window | Environment | F7 | Equivalente |
| DecalProjector | URP Decal Projector | Decal | F13 | Equivalente |
| ParticleSystem (GPU) | Particle System / VFX Graph | GPUParticles3D | F13 | Adaptação (subconjunto de módulos, ver doc 18 §9) |
| LineRenderer, TrailRenderer | ✓ | — | F13 | Equivalente |
| Terrain | Terrain | — | F13/F14 | Parcial planejado |

## 14. Aceite

- **F2:** cubo texturizado e malha importada iluminados por direcional com sombra CSM, câmera autorada, no aparelho e no host; 100 retomadas + rotação sem erro de validação Vulkan.
- **F7:** cena de referência com 64 luzes point/spot, sombras locais, IBL, bloom, tonemap, TAA/upscaler; medição AGI por tier registrada; nenhum controle de luz/câmera sem efeito.
- **F13:** cada técnica do laboratório com captura antes/depois, custo GPU por tier e controle no editor.
