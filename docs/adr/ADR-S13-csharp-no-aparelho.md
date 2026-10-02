# ADR-S13 — C# como segunda linguagem no aparelho

- **Status:** aprovado com regras de uso (02/10/2026). Confirma a ADR-D12b (Luau + C#).
- **Pergunta:** o C# vale o custo no aparelho, lado a lado com o Luau?
- **Prazo:** 5 dias. Usado: 1 sessão.

## Método

`spikes/s13_dotnet/Bench`: console C# autocontido para `linux-bionic-arm64` (.NET 8.0.27, mesma versão vendorizada na Astra atual), rodado por `adb shell` no Xiaomi 25053PC47G. Mesmos laços do S-08, mais o custo de chamada e a compilação Roslyn de um comportamento (`Porta`). A hospedagem **dentro do app** (hostfxr, OpenSSL privado, Roslyn, reabertura no mesmo PID) já foi provada neste aparelho pela Astra atual (`atchengine/native/third_party/dotnet-runtime/README.md`), por isso não foi repetida aqui.

**Fato importante:** no Android, o runtime oficial do .NET 8 para bionic é **Mono** (JIT do Mono + GC SGen), ainda que o arquivo se chame `libcoreclr.so`. O desempenho medido é o do Mono, não o do CoreCLR de PC.

## Resultado (3 rodadas; saída em `docs/validacao/2026-10-02-s13/`)

| Medida | C# (.NET 8 / Mono JIT) | Luau 0.740 (S-08, Release, shell) |
|---|---|---|
| Laços (20M aritmética + 20M acessos a array) | **193–210 ms** (1 rodada ruidosa: 460 ms) | nativo 306 ms; interpretado 503 ms |
| Chamada vazia | 1,5–1,8 ns (delegate) | 13–22 ns (C++↔Luau) |
| Partida até o `Main` | 96–152 ms | — |
| Memória | 27 MB (runtime); **95 MB** com o Roslyn carregado | poucos MB |
| Compilar um script no aparelho (Roslyn) | **1,2–2,3 s** na 1ª compilação; 3–9 ms nas seguintes | milissegundos |
| Tamanho implantado | 38 MB (6,5 MB nativo + 31 MB gerenciado, dos quais 9 MB de Roslyn só no editor) + 6,6 MB de OpenSSL (só para o Roslyn) | ~2 MB |

**Ganho:** cerca de **1,6×** sobre o Luau nativo em cálculo e ~10× mais barato em chamadas. **Custo:** dezenas de MB, memória alta com o compilador e primeira compilação de segundos.

## Regras que valem a partir daqui

1. **Luau é a linguagem padrão** de comportamentos (compilação instantânea, hot reload, sandbox, depurador). C# é opcional, por projeto, para módulos de cálculo pesado.
2. O runtime .NET e o Roslyn **só carregam se o projeto tiver código C#**; projetos só-Luau não pagam o custo.
3. O editor aquece o Roslyn **em segundo plano** ao abrir um projeto com C#, para a 1ª compilação não travar o "Salvar".
4. Jogos exportados recebem o C# **pré-compilado** (sem Roslyn nem OpenSSL no APK do jogo); no iOS, só AOT.
5. A mesma API gerada (`api.json`) serve às duas linguagens; os bindings C# usam `UnmanagedCallersOnly` e ponteiros de função, como na Astra atual.
6. **Reavaliar** com o .NET 10/11 quando o CoreCLR para Android estiver estável: o JIT do CoreCLR deve ganhar bem mais que o do Mono.

## Pendente

- Hospedagem dentro do app da Astra 2 (portar `dotnet_host.cpp` para a camada de plataforma) e custo de chamada C++↔C# medido no app (F6).
- Medir AOT do Mono (jogo exportado) no mesmo benchmark.
