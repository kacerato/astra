# ADR-D12b — Luau + C# como linguagens de script

- **Status:** decidido pelo usuário (02/10/2026); substitui a parte "só Luau" da D-12. Execução depende do spike S-13.
- **Decisão:** Luau continua como linguagem principal no aparelho (compilação instantânea, hot reload, sandbox, depurador; ADR-S08). **C# entra como segunda linguagem** para módulos de cálculo pesado, com a mesma API gerada do `api.json` (doc 14 §5.2).
- **Motivo:** poder computacional. No S-08, o Luau nativo deu 1,64× sobre o interpretador; o JIT do .NET tende a ganhar bem mais em cálculo pesado.
- **Custos conhecidos:** runtime de dezenas de MB, compilação (Roslyn) de segundos no aparelho, iOS só com AOT (sem editar C# no aparelho).
- **Referência interna:** a Astra atual hospeda o .NET no Android via `hostfxr`/CoreCLR (`atchengine/native/platform/android/dotnet_host.cpp`, `managed/Astra.Scripting`, net8.0).

## S-13 (novo spike, antes da F6)

Mede no aparelho, dentro do app: tamanho do runtime no APK, memória, tempo de inicialização, tempo de compilação de um script no aparelho, custo de chamada C++↔C#, e velocidade em laços de cálculo lado a lado com o Luau (mesmos benchmarks do S-08). Aprova se o ganho de velocidade justificar o custo; senão, C# fica só para jogos exportados (AOT) ou só no PC.
