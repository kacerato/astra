# 21 — Qualidade, testes e desempenho

## 1. Evidência (herança do AGENTS.md)

| Evidência | Afirmação permitida |
|---|---|
| Leitura de código | "Implementação localizada" |
| Compilou | "Compila no alvo X, configuração Y" |
| Teste passou | "Passou nos casos e no ambiente executados" |
| Visto no aparelho | "Comportamento observado na execução registrada" |
| Medido | "Resultado nas condições registradas" |

Todo relato de bloco usa o formato **Resultado / Integração / Validação / Pendências**, com comandos executados e o que não foi executado.

## 2. Pirâmide de testes

| Nível | Ferramenta | Onde roda | Exemplos |
|---|---|---|---|
| Unitário | doctest | Host, todo PR | Variant, PropertyPath, math, handles, parsers |
| **Contrato** | doctest + geradores de casos | Host, todo PR | `reflection_consumers`; round-trip texto↔mundo↔texto; **Undo invertível** (sequências aleatórias de comandos aplicadas e desfeitas → estado idêntico); **recuperação do WAL** (corte em pontos aleatórios do arquivo); migração de fixtures |
| Integração headless | doctest + backends reais sem janela (Jolt, Luau, flecs, miniaudio com device nulo) e `null` | Host, todo PR | Física + scripts + eventos; carga de cena cozida; prefab apply/revert |
| **Imagem de referência** | Render offscreen Vulkan no host; no CI Linux, **lavapipe** (Vulkan por software do Mesa) | Noturno | Cenas de referência por feature, comparadas com tolerância perceptual |
| Fuzzing | libFuzzer | Noturno | Importadores, formato de cena, WAL, pak |
| Sanitizers | ASan/UBSan (host), HWASan (build Android dedicado) | Noturno / sob demanda | — |
| Aparelho | Scripts ADB + cenários por intent | Local | Ciclo de vida, fluxos F-01 a F-10, capturas, medições |
| Soak | Aparelho | Antes de fechar fase | 30 min de edição/Play; 100× Play/Stop; 100 retomadas |

Regras: não remover asserts, não desligar testes e não relaxar expectativas para ficar verde. Falha nova é regressão; falhas preexistentes ficam registradas à parte, com evidência.

## 3. Validação proporcional

| Mudança | Validação |
|---|---|
| Texto/documentação | Revisão do diff e das referências |
| Bug localizado | Teste que falha antes e passa depois, de preferência no arquivo existente |
| Identidade, composição, hierarquia, serialização, Undo | Contrato do fluxo afetado (round-trip, inverso, atomicidade) |
| Fronteiras (JNI, Luau, threads) | Build dos lados envolvidos + caso de integração/tempo de vida |
| UI, entrada, física, Play | Caminho real no ambiente disponível, limitações declaradas |
| Renderer e shaders | Build + validação Vulkan sem erros + **captura** para afirmar correção visual |
| Desempenho | Medição comparável antes/depois, só quando houver hipótese de desempenho |

## 4. Gates permanentes de aparelho

Valem para toda fase com renderização ou plataforma:

- 100 ciclos pausa/retomada + rotação + multi-janela, sem erro da camada de validação (`logcat` filtrado) e sem mudança de PID (PID diferente = crash).
- 100× Play/Stop sem crescimento de RSS, memória Vulkan, corpos do Jolt, VMs Luau ou vozes de áudio.
- Editor parado: 0 frames de GPU por 60 s.

## 5. Automação no aparelho

- **Cenários por intent:** `am start ... --es astra.project <nome> --es astra.scenario <roteiro>`. O app executa o roteiro (abrir cena, criar objeto, dar Play, capturar) por **ids semânticos**, sem coordenadas cegas.
- Captura antes de cada toque quando houver interação manual; serial do ADB tratado por tabulação; transport listado antes de cada uso (lições da Astra atual).
- Coleta: capturas, `logcat` (erros de validação Vulkan, crashes, avisos Astra), relatório de frame do Profiler, traço Perfetto quando pedido.
- Saída em `build/device-validation/<data>/` com `report.json`.

## 6. Bancada de medição

Uma medição só vale com estado controlado (herança da memória `aether-bancada-de-medicao`):

| Condição | Regra |
|---|---|
| Modos do fabricante (GameTurbo, Game Mode, economia) | Desligados ou registrados; rodada com estado diferente é **inválida** |
| Bateria e temperatura | Bateria ≥ 50%; temperatura inicial registrada; esperar estabilizar entre rodadas |
| Tela | Brilho fixo, taxa de atualização fixa, mesma resolução e tier |
| Aquecimento | Descartar os primeiros N segundos |
| Amostras | Número finito, mesmo roteiro, p50/p95/p99 de frame, divisão CPU/GPU/apresentação |
| Comparação | Antes e depois nas mesmas condições; diferença dentro do ruído não conta como ganho |

## 7. Ferramentas de profiling

| Ferramenta | Uso |
|---|---|
| **Profiler do editor** | Linha do tempo por thread, GPU por pass, memória por tag, contadores de subsistema, *thermal headroom* |
| **Tracy 0.14** | Zonas CPU/GPU, locks, alocações, gráficos; conexão remota via `adb forward` (builds de desenvolvimento) |
| **Android GPU Inspector (AGI)** | Profiler de frame e de sistema (Adreno/Mali), contadores de banda |
| **RenderDoc** | Captura Vulkan no Android e no host |
| **Perfetto** | Traços de sistema, agendamento, térmico, energia |
| **Arm Performance Studio** (Streamline, Mali Offline Compiler) | Custo de shaders e contadores Mali |
| **Snapdragon Profiler** | Contadores Adreno |
| Android Studio Profiler | Memória nativa e Java |

## 8. Orçamentos

Os números abaixo são **hipóteses de partida** e serão substituídos pela medição da F2 (base) e da F7 (render completo) em cada tier. Nenhum vale como promessa antes disso.

| Item | T1 (60 Hz) | T3 (120 Hz, opcional) |
|---|---|---|
| Frame total | 16,6 ms | 8,3 ms |
| Thread principal: scripts / física / animação / UI | 2 / 2 / 1,5 / 1 ms | 1 / 1 / 0,8 / 0,5 ms |
| GPU: sombras / opaco / transparente+partículas / pós | 2 / 6 / 2 / 2 ms | 1 / 3 / 1 / 1 ms |
| Editor: Inspector com 300 propriedades (montagem) | ≤ 16 ms | ≤ 8 ms |
| Editor: toque → reação visível no viewport | A medir | A medir |
| RSS do editor com projeto médio | A medir | A medir |
| Entrar no Play (cena média) | A medir | A medir |

Os orçamentos ficam no asset `QualitySettings` e no relatório de aparelho. O Profiler pinta de amarelo/vermelho o que passa do orçamento.

## 9. Laboratório de aparelhos

| Aparelho | Tier | Situação |
|---|---|---|
| Xiaomi `25053PC47G` (2772×1280) | T3 | Disponível (usado na Astra atual) |
| Aparelho com GPU **Mali** (G7x/G7xx) | T1/T2 | **Necessário antes de fechar F7** |
| Aparelho de entrada (Adreno 6xx ou Mali-G57) | T0 | **Necessário antes de fechar F7** |
| Tablet Android | — | Necessário antes de fechar F4 (layout tablet) |
| Exynos/Xclipse, PowerVR | — | Desejável |

Registro em `tests/device/devices.json`: modelo, SoC, GPU, driver, versão do Android, tier atribuído.

## 10. Qualidade de código

- `-Werror` nos alvos Astra; clang-format; conjunto pequeno de checks do clang-tidy (bugprone, performance), ativado por pasta.
- `check-layering` e `licenses` no CI.
- Sem log por frame; instrumentação temporária removida antes do commit.
- Revisão de diff antes de cada commit: sem mudanças alheias, sem símbolos inventados, sem consumidor faltando, sem perda silenciosa de dados.

## 11. Definição de pronto (por bloco)

1. Comportamento implementado e **integrado** (dado → editor/API → validação → persistência → consumidor runtime).
2. Testes proporcionais passando; validação no aparelho quando o bloco é visível ou de plataforma.
3. Capturas do editor comparadas com o design aprovado (blocos de UI).
4. Documentação existente atualizada se mudou contrato ou uso.
5. Relato no formato de §1, com pendências explícitas.
