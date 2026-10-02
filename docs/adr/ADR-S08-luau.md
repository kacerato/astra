# ADR-S08 — Luau 0.740 como linguagem de scripts

- **Status:** aprovado (02/10/2026). **D-12 confirmada** (Luau no lugar de C#).
- **Pergunta (doc 22 §1):** o Luau atende? Bindings genérico + thunk, `vector`, `interrupt`, limite de memória, ponto de parada com UI responsiva, codegen arm64 no aparelho.
- **Prazo:** 7 dias. Usado: 1 sessão.

## Método

`spikes/s08_luau/luau_suite.cpp`: uma suíte, três execuções. Console no PC (Release) e no celular por `adb shell` (Release), e **dentro do app** S-02 (`debug.astra.s02.luau=1`, build Debug), porque a política de memória executável de um app difere da do shell.

## Resultado (Xiaomi 25053PC47G, SM8735, Android 16)

| Item | Aparelho, shell (Release) | Dentro do app (Debug) |
|---|---|---|
| Binding genérico (busca por nome + Variant): leitura + escrita de `vector` | 179 ns/iter | ok |
| Binding rápido (átomo de string → `switch`, o que o gerador de thunks vai emitir) | **87 ns/iter** (2,1× mais rápido) | ok |
| Chamada C++ → Luau (função vazia) / Luau → C++ | 22 ns / 13 ns | ok |
| `vector` nativo: 100 mil operações | **0 alocações** | 0 alocações |
| `interrupt`: laço infinito com prazo de 100 ms | parou em 100,0 ms, erro `tempo excedido em Porta.luau:2`, VM reutilizável | 100,2 ms |
| Limite duro de memória (8 MB) | `LUA_ERRMEM`; após coleta 0,29 MB e VM reutilizável | ok |
| Ponto de parada | Parou em `Contador.luau:5`, local `total=15`; 120 quadros do "editor" rodaram com o script pausado; retomada terminou com o resultado certo (30) | ok |
| Sandbox (`luaL_sandbox`) | Biblioteca padrão somente leitura | ok |
| Codegen nativo arm64 | 3 de 3 funções compiladas; **1,64×** em laços de aritmética e array | Funciona no contexto de app (2,29× em Debug); nenhuma negação do SELinux |

Saídas completas em `docs/validacao/2026-10-02-s08/`. Condições de bancada não controladas; os números servem de ordem de grandeza, não de orçamento.

## Regras que o S-08 fixou para o ScriptHost (F6)

1. **Depurador:** o *hook* `debugbreak` chama `lua_break` só na primeira passagem. Ao retomar, a VM reexecuta a instrução `BREAK`, e o depurador a deixa seguir (mesmo padrão do teste de conformidade do Luau). Inspeção de pilha e locais enquanto pausado é feita com **`lua_callhook`** na thread parada: `lua_getinfo`/`lua_getlocal` chamados direto de fora devolvem a linha de entrada e nenhum local.
2. Scripts em depuração são compilados com **`debugLevel = 2`** (nomes de locais e upvalues); o player usa 1.
3. O `interrupt` só lança erro fora do GC (`gc < 0`) e informa `arquivo:linha` via `lua_getinfo(L, 0, "sl")`.
4. Propriedades quentes usam **átomos** (`useratom` + `lua_tostringatom`/`lua_namecallatom`); o caminho genérico por nome continua como fallback automático.
5. Codegen pela API C++ (`Luau::CodeGen::compile` com `CompilationStats`) para registrar quantas funções viraram nativas; ganho esperado em laços de VM, não em chamadas a builtins (`math.*` já são *fastcalls*).

## Pendente

- Medir pressão de GC de `Quaternion`/`Color` como userdata com pool (doc 14 §5.2) quando os tipos existirem (F6).
- Analisador de tipos (`Luau.Analysis`) e autocompletar no aparelho: tamanho e tempo ainda não medidos (F6, IDE).
- Tamanho do VM + compilador + codegen no APK de release.
