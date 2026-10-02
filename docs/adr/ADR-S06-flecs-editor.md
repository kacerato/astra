# ADR-S06 — flecs 4.1.6 para o mundo do editor

- **Status:** aprovado (02/10/2026)
- **Pergunta (doc 22 §1):** o flecs aguenta o editor (reparent e reordenação de 10 mil, `Multi<T>`, mudanças estruturais adiadas, ordem de filhos estável)?
- **Prazo:** 4 dias. Usado: 1 sessão. **Plano B (EnTT + hierarquia própria) descartado.**

## Método

`spikes/s06_flecs` (console, mesmo código no PC e no aparelho). Cena: raiz → 100 grupos × 1000 folhas (100.101 entidades), todos os pais com `OrderedChildren`. Medido para as duas formas de hierarquia do flecs 4.1: `ChildOf` (par; fragmenta tabelas por pai) e `Parent` (componente; não fragmenta). **Orçamentos fixados no código antes da primeira medição**, para o aparelho T3.

## Resultado no aparelho (Xiaomi 25053PC47G, SM8735, Android 16; Release; `adb shell`; 3ª repetição)

| Medição | ChildOf | Parent | Orçamento |
|---|---|---|---|
| Criar 100.101 entidades | 12,4 ms | 17,8 ms | — |
| Reparent interativo (p99, 1000 amostras) | 5,9 µs | 2,5 µs | 200 µs |
| Reordenação interativa, pai com 1000 filhos (p99) | 0,16 µs | 0,16 µs | 500 µs |
| Reparent em lote (10 mil) | 1,3 ms | 1,4 ms | 100 ms |
| Percorrer 100 mil na ordem (painel Hierarquia completo) | 0,68 ms | 0,75 ms | 20 ms |
| Aplicar 10 mil operações adiadas (`defer_end`) | 1,8 ms | 0,65 ms | 50 ms |
| Destruir subárvore de 10 mil | 5,3 ms | 0,22 ms | 50 ms |
| `Multi<T>`: adicionar 10 mil colisores em 5 mil entidades | 1,9 ms | | 20 ms |
| `Multi<T>`: iterar 10 mil colisores | 0,018 ms | | 1 ms |

Saídas completas: `docs/validacao/2026-10-02-s06/` (aparelho e host). Condições não controladas (modo de jogo do fabricante, temperatura); a folga de 10× a 3000× torna a conclusão insensível a isso.

## Corretude verificada (as duas formas)

- Ordem dos filhos **estável** depois de mudança estrutural nos filhos (troca de tabela).
- Reparent leva o filho para o **fim** da lista ordenada do novo pai; lote de 10 mil chega na ordem enviada.
- `ecs_set_child_order` aplica exatamente a ordem pedida.
- Operações adiadas são aplicadas no `defer_end`; destruir o pai destrói os filhos.
- `Multi<T>`: `instanceId` das demais instâncias não muda ao remover uma.

## Decisões

- flecs 4.1.6 fica como armazenamento do `World` (doc 05), build enxuto (`cmake/Flecs.cmake`: C++ API, sistemas, pipeline e log; sem REST/HTTP/script).
- `Multi<T>` = um componente flecs com vetor pequeno e `instanceId` estável (doc 05).
- **Forma da hierarquia:** as duas passam com folga. A escolha fica para a F1, medida no que esta bancada não cobre: custo de iteração de sistemas (`Transform`) com a fragmentação do `ChildOf` em cena real. Ponto de partida: `ChildOf` + `OrderedChildren` para cenas, a recomendação do flecs para hierarquias grandes e dinâmicas.
- Atenção: `ecs_set_child_order` **não é adiado**, segundo a documentação, e falha dentro de sistema multithread. Reordenação vem só de comandos do editor, na thread principal.
