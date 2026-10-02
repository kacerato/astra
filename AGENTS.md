# AGENTS.md — Astra 2

Este arquivo vale para todo o repositório `astra/` e **substitui** o `AGENTS.md` da pasta-mãe (`Downloads/AGENTS.md`), que descreve a engine anterior (`atchengine`). A engine anterior é material de estudo: não copie código dela sem passar pelas regras abaixo.

## 1. Missão e regra de entrega

Construir a Astra 2: engine e editor mobile-first (Android, Vulkan) sobre The Forge e bibliotecas maduras, com API própria e trocável. O plano mestre está em `docs/plano-mestre/` (00-INDICE a 24-REFERENCIAS). Decisões têm IDs (`D-xx`), spikes (`S-xx`), riscos (`R-xx`) e limites (`L-xx`); use esses IDs nas entregas.

**Entregue a menor mudança coesa que resolva o pedido inteiro no escopo acordado.** Menor não é capado; universal não é framework para hipóteses.

Prioridades: correção e dados → comportamento pedido → integração → simplicidade → desempenho medido.

## 2. Estado atual (revalide antes de trabalhar)

| Área | Situação |
|---|---|
| Fase | **F0** (viabilidade e fundação), ver `docs/plano-mestre/22-ROADMAP-FASES-E-GATES.md` |
| The Forge | Submódulo `third_party/the-forge` → fork privado `kacerato/astra-forge`, branch `astra/1.63` (base = árvore da tag `v1.63`; ver `third_party/VERSIONS.md`). Patches em `third_party/ASTRA_PATCHES.md`; envie o submódulo antes do repositório principal |
| Build | CMake ≥ 3.28 + presets (`CMakePresets.json`); The Forge compilado por `cmake/TheForge.cmake` |
| Spikes | `spikes/` (código descartável que responde uma pergunta; resultado vira ADR em `docs/adr/`). Aprovados: S-01, S-02, S-03, S-04, S-05 (medição de banda pendente), S-06, S-08. Falta S-07 (RmlUi) |
| Renderer | A Astra liga só `tf_core` + `tf_graphics`; `tf_os`/`tf_renderer` (IApp, ImGui, Lua) só em spikes. Ver ADR-S02 |
| Aparelho | `tools/device/surface_cycles.sh` (ciclos de surface). Xiaomi/MIUI: pacote novo por ADB exige "Instalar via USB" ligado; use `MSYS_NO_PATHCONV=1` no Git Bash para caminhos `/sdcard` |
| Engine (`engine/`, `backends/`, `editor/`…) | Ainda não existe. Estrutura alvo em `docs/plano-mestre/04-ARQUITETURA-CAMADAS-E-REPOSITORIO.md` §11 |

Antes da primeira alteração: `git status --short`, `git branch --show-current`, `git rev-parse HEAD`, e o mesmo dentro do submódulo quando mexer nele. Preserve alterações do usuário. Não troque branch, não faça reset, não limpe arquivos e não atualize dependências por iniciativa própria.

## 3. Evidência antes de afirmação

| Evidência | Afirmação permitida |
|---|---|
| Leitura de arquivos | Contrato ou implementação localizada |
| Compilação | Compilou naquele alvo e configuração |
| Teste executado | Passou nos casos e no ambiente executados |
| Execução no aparelho/app | Comportamento observado naquela execução |
| Captura ou medição | Resultado nas condições registradas |

Não invente APIs, versões, comandos, logs, capturas ou métricas. "Compilou" não é "funciona no Android"; "declarado" não é "integrado". Proibido sucesso falso: função vazia anunciada como pronta, erro engolido, fallback silencioso que muda a semântica.

Para APIs do The Forge e das bibliotecas: localize a declaração **no código fixado em `third_party/`**, não na memória nem na documentação de outra versão.

## 4. Fluxo

1. Defina o resultado observável e o aceite (use o aceite do documento do plano quando existir).
2. Reaproveite o que existe; não reimplemente com outro nome.
3. Consulte a referência pertinente (Unity 6.0, Godot 4.7, Stride, ezEngine, Wicked conforme o plano), registrando versão e link.
4. Implemente uma fatia funcional completa: dado → API/editor → validação → persistência → consumidor runtime.
5. Verifique proporcionalmente e pare no aceite.

## 5. Arquitetura (resumo obrigatório do doc 04)

- Camadas: aplicações → editor → Astra API → servidores → backends → núcleo. **Só `backends/<x>` inclui headers de terceiros** (`IGraphics.h`, `Jolt/`, `miniaudio.h`, `RmlUi/`, `lua.h`, `ozz/`, `Detour*`; `flecs.h` só em `engine/world`).
- Servidor nunca lê o ECS; sistemas de sincronização empurram dados.
- Handles com geração; referências persistentes usam `LocalId`/`AssetGuid`, nunca handles de runtime.
- Documento de edição ≠ mundo de Play ≠ estado da câmera do editor.
- Toda propriedade exposta tem consumidor real, persistência, Undo (autoral) e mutabilidade em Play definidas (doc 05).
- Runtime não depende do editor. Exportar jogo ≠ compilar o APK do editor.

## 6. The Forge e terceiros

- Mudança dentro de `third_party/the-forge`: commit no branch `astra/1.63` do submódulo com prefixo `forge:`, marcação de arquivo modificado (Apache-2.0) e linha em `third_party/ASTRA_PATCHES.md` (arquivo, motivo, risco).
- Dependência nova exige licença, plataforma e versão verificadas e registro em `third_party/VERSIONS.md`.
- Não atualize versão de biblioteca sem tarefa explícita.

## 7. Contra código e documentação desnecessários

Sem `Manager`/`Service`/`Factory`/wrappers de uma chamada/opções sem consumidor/backends fictícios. Interface nova precisa de motivo concreto. Sem logs por frame, dashboards, auditorias ou demos extras. Um único arquivo de estado (`docs/plano-mestre/ESTADO.md`, quando existir); decisões estruturais viram ADR.

## 8. Protocolo contra loops

Cada tentativa muda hipótese, entrada, código ou condição. Após duas tentativas sem evidência nova, pare a repetição, registre erro/hipótese/descartado/próxima verificação e mude a estratégia. Spike que estoura o prazo conta como reprovado (aciona plano B ou decisão do usuário). Não reabra decisões sem evidência nova.

## 9. Validação proporcional

| Mudança | Validação |
|---|---|
| Texto/doc | Revisão do diff e referências |
| Bug localizado | Teste que falha antes e passa depois |
| Identidade, composição, serialização, Undo | Round-trip, operação inversa, atomicidade |
| Fronteiras (JNI, Luau, threads) | Build dos dois lados + caso de tempo de vida |
| UI, entrada, física, Play | Caminho real no ambiente disponível |
| Renderer/shaders | Build + validação Vulkan sem erros + captura |
| Desempenho | Medição comparável antes/depois (bancada do doc 21 §6) |

Build incremental do alvo afetado; sem clean/rebuild total por ritual. Antes de commitar código nativo que roda no Android, compile o alvo `android-arm64-*` (o clang do NDK pega avisos que o compilador do host não pega).

## 10. Android e aparelho

Considere pausa/retomada, surface, toque, IME, permissões e descarte. Capture a tela antes de cada toque de navegação no ADB; não toque às cegas. Nunca desinstale o app num aparelho com projetos sem backup. Afirmações de desempenho valem só para o aparelho e as condições medidas.

## 11. Regra visual do editor

**Não serei simplista no design.** Toda função de editor nova ou revisada segue `docs/plano-mestre/17`, `18` e `19`: tokens únicos, ícones do pipeline, todos os estados desenhados, captura no aparelho comparada com o frame aprovado. Imagem gerada é conceito, nunca evidência; cada controle precisa de comportamento real. Relate separadamente **conceito**, **implementado** e **testado no aparelho**.

## 12. Pronto e resposta final

```text
Resultado: o que passou a ser possível.
Integração: arquivos/símbolos principais; diferença relevante da referência.
Validação: comandos executados, resultados, o que não foi verificado.
Pendências: limitações concretas ou bloqueio exato.
```

Sem "100%", "paridade Unity" ou "produção" sem delimitação e evidência. Terminou o objetivo, encerre.
