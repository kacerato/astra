# 22 — Roadmap, fases e gates

Sem datas: o plano fixa **ordem, dependências, prazos máximos dos spikes e aceite observável**. Tamanho relativo: P (dias), M (1–3 semanas), G (1–2 meses), GG (> 2 meses), para uma pessoa com agentes.

---

## 1. Spikes (decidem ou derrubam hipóteses)

Um spike tem prazo máximo, pergunta binária e saída registrada como ADR. Estourar o prazo sem resposta conta como **reprovação**: aciona o plano B ou uma decisão do usuário (protocolo contra loops do AGENTS.md).

| ID | Pergunta | Prazo máx. | Aprovação | Se reprovar |
|---|---|---|---|---|
| **S-01** | O TF 1.63 (Vulkan) compila por **CMake + NDK r28+** com 16 KB e roda no aparelho e no host? | 10 dias úteis | Triângulo + textura pelo Resource Loader + shader FSL no aparelho e no host; `.so` alinhadas a 16 KB (`check_elf_alignment`); sem erro de validação | Plano B gráfico ([23](23-RISCOS-LIMITES-E-PLANO-B.md) §3) |
| **S-02** | Dá para usar o renderer do TF com **GameActivity** e loop próprio (sem o `IApp`)? | 5 dias | 100 ciclos de surface perdida/recriada; IME funcionando; nenhuma dependência do app glue do TF | Adaptar o OS do TF à GameActivity (custo maior) |
| **S-03** | O fork aceita **Vulkan no Windows** (a 1.60 removeu a troca DX12/Vulkan no PC)? | 3 dias | Mesmo triângulo/textura no Windows via Vulkan | Host em Linux/Vulkan, ou DX12 só no host com diferença registrada |
| **S-04** | O TF consome **SPIR-V gerado fora do FSL** (glslang) com layout de descriptors correto? | 3 dias | Shader GLSL compilado em runtime desenha corretamente | Patch no fork; sem isso, Shader Graph só no host (L-05) |
| **S-05** | Controles TBDR: load/store discard, memória *lazily allocated*, pré-rotação da surface | 5 dias | Cada item confirmado ou patch identificado; captura AGI com banda medida | Patches no fork, listados em `ASTRA_PATCHES.md` |
| **S-06** | flecs 4.1 aguenta o editor: reparent/reordenação de 10 mil, `Multi<T>`, mudanças estruturais adiadas | 4 dias | Tempos medidos dentro do orçamento; `OrderedChildren` estável | EnTT + hierarquia própria |
| **S-07** | RmlUi 6.3 sobre o TF serve ao editor? | 10 dias | `RenderInterface` completo (clip masks, filtros), toque inercial, IME, variáveis RCSS, **Inspector de 300 campos ≤ 16 ms (T2)**, árvore virtual de 10 mil fluida, ícone MSDF por shader, mini prova da fachada Canvas | Plano B de UI ([03](03-PILHA-E-BIBLIOTECAS.md) §6) |
| **S-08** | Luau atende? Bindings genérico + thunk, `vector`, `interrupt`, limite de memória, **ponto de parada com UI responsiva**, codegen arm64 no aparelho | 7 dias | Todos os itens demonstrados; custo de chamada medido | Lua 5.4 ou C# (D-12) |
| **S-09** | Custo do astcenc e da transcodificação BasisU no aparelho | 2 dias | Tempos por 2K/4K e presets definidos | Compressão no PC ou UASTC por padrão |
| **S-10** | Jolt com jobs Astra no aparelho: 1.000 corpos, determinismo no mesmo build | 2 dias | Tempo medido; replays iguais | Ajustar workers/camadas |
| **S-11** | Montar APK no aparelho: manifesto/recursos, zipalign, apksig, instalação manual | 5 dias | APK de teste instala e roda em outro aparelho | Template com placeholders; só export pelo PC |
| **S-12** | miniaudio no Android: latência AAudio, troca de rota Bluetooth, foco | 2 dias | Sem silêncio permanente nem crash | Oboe como saída |
| **S-13** | C# (CoreCLR, como na Astra atual) como segunda linguagem vale o custo no aparelho? (ADR-D12b) | 5 dias | Tamanho, memória, inicialização, compilação no aparelho e velocidade medidos lado a lado com o Luau; ganho justifica o custo | C# só em jogos exportados (AOT) ou só no PC |

S-01 a S-08 fazem parte da **F0**. S-09 roda antes da F3, S-10 antes da F5, S-12 antes da F9 e S-11 durante a F6, para reduzir o risco da F12 cedo.

## 2. Fases

### F0 — Viabilidade e fundação do repositório · G

| Entregável | Conteúdo |
|---|---|
| E-0.1 | Repositório `astra` com `AGENTS.md` novo (regras atuais + mapa novo), `docs/plano-mestre/` (estes documentos), CMakePresets, `third_party/VERSIONS.md` |
| E-0.2 | Fork `astra-forge` (v1.63) com build CMake, NDK r28+, Vulkan Windows/Android, módulos excluídos ([03](03-PILHA-E-BIBLIOTECAS.md) §3.3) |
| E-0.3 | Spikes S-01 a S-08 com ADR cada |
| E-0.4 | CI mínimo (host + Android + `check-layering` + `licenses`) |
| E-0.5 | Astra UI Preview inicial (RmlUi com backend de exemplo) para começar o design em paralelo |
| E-0.6 | Arquivo Figma criado com Fundamentos e Marca vetorizada (D-20) |

**Aceite:** todos os spikes aprovados ou com plano B decidido pelo usuário; APK "hello Astra" com RmlUi desenhando sobre o TF no aparelho.

### F1 — Núcleo · G (depende de F0)

`foundation` (memória com tags, jobs, VFS, log, Tracy), `object` (TypeRegistry, Variant, PropertyInfo, Resource, UID), `world` (flecs, Entity, hierarquia, Transform, ProcessMode, ciclo de vida, `Multi<T>`), `scene` (texto, binário, migração, dados desconhecidos), `api-dump` inicial. **Aceite:** [05](05-MODELO-DE-OBJETOS-E-REFLEXAO.md) §9 e [06](06-CENA-PREFABS-SERIALIZACAO.md) §10 (round-trip, migração, desconhecidos; prefabs entram na F4).

### F2 — Primeiro frame integrado · M (depende de F1)

`render::World` + `ForgeRenderer` (frame graph, Lit básico, direcional + CSM, câmera, céu/IBL simples, tonemap), `platform/android` (GameActivity, ciclo de vida, toque, IME), host Windows, `Input` núcleo, player mínimo que carrega cena binária. **Aceite:** [08](08-RENDERIZACAO-FORGE.md) §14 F2 + gates de aparelho ([21](21-QUALIDADE-TESTES-PERFORMANCE.md) §4) + primeira medição de orçamento.

### F3 — Assets e importação · G (depende de F2; S-09)

AssetDatabase, `.meta`, importadores glTF/FBX/textura/áudio/fonte, cozimento por plataforma, ResourceLoader assíncrono, hot reload, thumbnails, cenas multi/aditivas. **Aceite:** [07](07-RECURSOS-E-PIPELINE-DE-ASSETS.md) §11.

### F4 — Editor v1 · GG (depende de F3; S-06, S-07)

Núcleo do editor (documentos, comandos, transações, Undo, WAL, sessão, plugins), Astra UI (dock, árvore e grade virtuais, campos), Hub, Cena (gizmos, picking por ID buffer, snapping, menu radial), Hierarquia, Inspector gerado, Assets, Console, Histórico, paleta de comandos, prefabs completos, Play no mesmo processo, Profiler básico, design system implementado (tokens, ícones lote F4). **Aceite:** [16](16-EDITOR-ARQUITETURA.md) §14 + fluxos F-01, F-02, F-04, F-05, F-07, F-10 dentro da meta de toques + capturas aprovadas.

### F5 — Física · G (depende de F4; S-10)

[09](09-FISICA-JOLT.md) completo (veículo no fim da fase). **Aceite:** [09](09-FISICA-JOLT.md) §10.

### F6 — Scripting · G (depende de F4; S-08; S-11 em paralelo)

Luau completo, IDE no editor, depurador, Input Actions assets, tweens/timers/curvas, sinais. **Aceite:** [14](14-SCRIPTING-LUAU.md) §13 + fluxo F-06.

### F7 — Render v2 · G (depende de F2; aparelhos Mali e T0 obrigatórios)

Luzes clusterizadas, sombras locais, probes de reflexão, Volume de pós (bloom, exposição automática, grading, GTAO), TAA/upscalers, LOD, pilha de câmeras, `RenderTexture`, tiers por `gpu.data`, painel de gráficos do aparelho, Forward+ × VB2 medido. **Aceite:** [08](08-RENDERIZACAO-FORGE.md) §14 F7.

### F8 — Animação · G (depende de F5 e F6)

[10](10-ANIMACAO-OZZ.md) completo. **Aceite:** [10](10-ANIMACAO-OZZ.md) §10 + fluxo F-08.

### F9 — Áudio · M (depende de F4; S-12)

[11](11-AUDIO-MINIAUDIO.md) completo. **Aceite:** [11](11-AUDIO-MINIAUDIO.md) §10.

### F10 — Navegação · M (depende de F5)

[12](12-NAVEGACAO-RECAST.md) completo. **Aceite:** [12](12-NAVEGACAO-RECAST.md) §9.

### F11 — UI de runtime · G (depende de F6)

[13](13-UI-RUNTIME-RMLUI.md) completo, workspace UI. **Aceite:** [13](13-UI-RUNTIME-RMLUI.md) §9.

### F12 — Exportação · M (depende de F6; idealmente depois de F9–F11)

[20](20-BUILD-EXPORTACAO-E-DISTRIBUICAO.md) §5 completo. **Aceite:** [20](20-BUILD-EXPORTACAO-E-DISTRIBUICAO.md) §7 + fluxo F-09.

### F13 — Render v3, laboratório · GG (depende de F7)

Técnicas de [08](08-RENDERIZACAO-FORGE.md) §12 em ordem de prioridade, partículas GPU com editor ([18](18-EDITOR-PAINEIS-E-FLUXOS.md) §9), decals, atmosfera, água, terreno, light probes. Cada técnica é um bloco com aceite próprio (captura + custo por tier + controles).

### F14 — Ferramentas avançadas · GG

Shader Graph (compilação no aparelho), Timeline, visual scripting (AST → Luau), retargeting humanoide, extensões de editor em Luau, terreno com escultura.

### F15 — Plataformas extras · sob decisão

iOS/Metal (backend da 1.63 congelado), desktop como alvo de jogos, editor oficial no PC.

## 3. Marcos de produto

| Marco | Fecha com | O usuário consegue |
|---|---|---|
| M1 "Primeiro frame" | F2 | Ver uma cena autorada em código rodando no aparelho com a nova base |
| M2 "Editor utilizável" | F4 | Montar e salvar cenas com prefabs no celular, sem perder trabalho |
| M3 "Jogo simples" | F5 + F6 + F9 | Fazer um jogo pequeno com física, scripts e som, testando em Play |
| M4 "Jogo exportado" | F12 (+F10, F11) | Gerar APK e jogar em outro aparelho |
| M5 "Astra 2.0" | Escopo de [01](01-VISAO-PRINCIPIOS-ESCOPO.md) §5 completo | Tudo da tabela 2.0, com inventários verificados item a item |

## 4. Paralelismo com agentes

- Trilhas separáveis depois da F4: **Render (F7/F13)**, **Gameplay (F5/F8/F10)**, **Ferramentas (F6/F11/F12)**, **Design (contínuo)**.
- Cada tarefa delegada tem escopo de pastas, saída e aceite. Duas tarefas nunca editam o mesmo módulo ao mesmo tempo (AGENTS §9).
- O design anda **na frente** da implementação de cada painel: frames aprovados antes de codar a UI correspondente.

## 5. Governança

- Decisões estruturais viram **ADR** (`docs/adr/ADR-xxx-*.md`), começando por ADR-001 (D-01) a ADR-022 (D-22).
- **Um único arquivo de estado** (`docs/plano-mestre/ESTADO.md`): por família, tipos **implementados / parciais / pesquisados**, com contagem real e link para a evidência. Nenhum outro arquivo de auditoria.
- Este plano é atualizado quando um spike muda uma decisão. Os parágrafos antigos não são reescritos: entram como "substituído por ADR-xxx".

## 6. Primeiros passos concretos (início da F0)

1. Criar o repositório `astra` (pasta irmã de `atchengine`) e copiar estes documentos para `docs/plano-mestre/`.
2. Escrever o `AGENTS.md` novo: regras atuais de evidência e entrega + mapa de módulos de [04](04-ARQUITETURA-CAMADAS-E-REPOSITORIO.md) + regra visual + este roadmap.
3. Trazer `ConfettiFX/The-Forge` na tag `v1.63` para `third_party/the-forge` (commit registrado) e o art necessário das amostras usadas no spike.
4. Esqueleto CMake com `cmake/AstraModules.cmake`, presets host/android e `check-layering`.
5. S-01 → S-03 → S-02 → S-04 → S-05 (gráfico e plataforma), com S-06, S-07 e S-08 em paralelo, porque não dependem do aparelho no início.
6. Em paralelo ao S-07: arquivo Figma com Fundamentos, Marca vetorizada e o primeiro frame do layout em celular paisagem, para validar a direção visual com o usuário.
