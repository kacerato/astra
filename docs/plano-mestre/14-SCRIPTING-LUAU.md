# 14 — Scripting com Luau

Backend: **Luau 0.740** (MIT): VM, compilador, analisador de tipos e autocompletar. Referências: Unity 6.0 *MonoBehaviour* e [ordem de execução](https://docs.unity3d.com/6000.0/Documentation/Manual/execution-order.html); Stride `ScriptComponent` (`StartupScript`, `SyncScript`, `AsyncScript`); Godot 4.7 `Script`/GDScript (`@export`, sinais, `await`).

---

## 1. Decisão D-12: por que Luau

| Critério | Luau | Lua 5.4 | C# (CoreCLR + Roslyn, Astra atual) | C# (Mono, interpretador) |
|---|---|---|---|---|
| Compilar no aparelho | Milissegundos | Milissegundos | Segundos (Roslyn em JIT frio) | Segundos |
| Memória e tamanho | Poucos MB | Poucos MB | Dezenas a centenas de MB (runtime + Roslyn) | Dezenas de MB |
| iOS | Interpretador permitido | Idem | Exige AOT (sem edição no aparelho) | Interpretador permitido |
| Tipos e autocompletar | Gradual, com analisador oficial | Não | Fortes | Fortes |
| Sandbox e limite de execução | Nativos (`sandbox`, `interrupt`) | Manual | Difícil (código nativo e reflexão) | Difícil |
| Hot reload | Natural | Natural | Difícil (descarregar assembly, estado) | Difícil |
| Desempenho | Interpretador muito otimizado + codegen opcional x64/arm64 | Bom | JIT superior em cálculo pesado | Inferior |
| Familiaridade para quem vem da Unity | Média | Média | Alta | Alta |
| Prova em mobile em larga escala | Roblox | Defold e outros | Unity (com IL2CPP, sem edição no aparelho) | — |

**Luau ganha nos critérios que definem o produto** (editar e rodar no celular com segurança e rapidez). A familiaridade vem de uma API de nomes e semântica Unity. Já a Astra atual provou que C# no aparelho funciona, mas a um custo alto demais.

**Reavaliar C#** se o S-08 reprovar Luau (desempenho, depuração ou bindings), ou se houver demanda comprovada, usando o mesmo `api.json` para gerar bindings.

## 2. Modelo: comportamento como componente

```lua
--!strict
local Astra = require("@astra")

local Porta = Astra.Behavior("Porta", {
  anguloAbertura = Astra.number(90, { min = 0, max = 180, unit = "deg", tooltip = "Ângulo ao abrir" }),
  velocidade     = Astra.number(120, { unit = "deg/s" }),
  alvo           = Astra.entity({ requires = "Transform" }),
  somAbrir       = Astra.asset("AudioClip"),
  aberta         = Astra.bool(false, { readonly = true }),
})

function Porta:Start()
  self.fechada = self.transform.localRotation
end

function Porta:OnTriggerEnter(outro: Collider)
  if outro.entity:HasTag("Jogador") then
    self:StartCoroutine(self.Abrir)
  end
end

function Porta:Abrir()
  self.entity:GetComponent(AudioSource):PlayOneShot(self.somAbrir)
  local alvo = self.fechada * Quaternion.Euler(0, self.anguloAbertura, 0)
  while Quaternion.Angle(self.transform.localRotation, alvo) > 0.5 do
    self.transform.localRotation = Quaternion.RotateTowards(
      self.transform.localRotation, alvo, self.velocidade * Time.deltaTime)
    Astra.wait()          -- próximo frame
  end
  self.aberta = true
end

return Porta
```

- A declaração de propriedades vira um **tipo dinâmico** no TypeRegistry (`script:Porta`), com `PropertyInfo` completos. Por isso Inspector, serialização, overrides de prefab, Undo, multi-edição e animação de propriedades funcionam **sem código extra**.
- Tipos aceitos: número (com faixa e unidade), inteiro, bool, string, enum (lista), cor, vetor 2/3/4, quaternion (Euler no Inspector), curva, gradiente, layer mask, `entity`, `component(T)`, `asset(T)`, arrays e structs simples.
- Mudar a declaração (renomear campo) usa `aliases = {"anguloMax"}` para migrar dados salvos.

## 3. Ciclo de vida

| Callback | Quando | Unity |
|---|---|---|
| `Awake` | Primeira ativação na hierarquia | ✓ |
| `OnEnable` / `OnDisable` | Habilitado e ativo / inverso | ✓ |
| `Start` | Antes do primeiro `Update` com componente habilitado | ✓ |
| `FixedUpdate` | Cada passo de física, antes do step | ✓ |
| `Update` / `LateUpdate` | Fases `Update` / `LateUpdate` | ✓ |
| `OnDestroy` | Fase `Cleanup` | ✓ |
| `OnTriggerEnter/Stay/Exit`, `OnCollisionEnter/Stay/Exit`, `OnControllerColliderHit`, `OnJointBreak` | Depois do passo de física | ✓ |
| `OnAnimatorMove`, `OnAnimatorIK`, eventos de animação | Fase `Animation` | ✓ (IK genérico) |
| `OnBecameVisible` / `OnBecameInvisible` | Resultado do culling do frame anterior (F7) | ✓ |
| `OnApplicationPause` / `OnApplicationFocus` / `OnApplicationQuit` | Ciclo de vida da plataforma | ✓ |
| `OnValidate` | Editor: propriedade alterada no Inspector | ✓ |
| `Reset` | Editor: componente adicionado ou resetado | ✓ |
| `OnDrawGizmos` / `OnDrawGizmosSelected` | Editor: desenho via `Gizmos`/`DebugDraw` | ✓ |

- `executeInEditMode = true` na declaração permite rodar no mundo de edição (o `[ExecuteAlways]` da Unity), com sandbox mais restrita.
- O ScriptHost só chama callbacks que o script **implementa** (registro de interesse por classe), evitando custo de `Update` vazio.
- Ordem entre objetos: determinística ([05](05-MODELO-DE-OBJETOS-E-REFLEXAO.md) §5.4), com `executionOrder` por classe. Na Unity a ordem entre objetos é indefinida; aqui é adaptação explícita.

## 4. Corrotinas e tempo

- `self:StartCoroutine(fn, ...)` → coroutine Luau ligada ao componente. Para sozinha em `OnDisable`/`OnDestroy`, como na Unity.
- Esperas: `Astra.wait()` (próximo frame), `Astra.waitSeconds(s)` (tempo escalado), `Astra.waitRealtime(s)`, `Astra.waitFixed()`, `Astra.waitUntil(pred)`, `Astra.waitWhile(pred)`, `Astra.waitAll({...})`.
- `self:StopCoroutine(handle)`, `self:StopAllCoroutines()`.
- Tweens e timers (herança da Astra atual) como API de alto nível sobre o mesmo agendador: `Tween.to(obj, "transform.localPosition", alvo, 0.5, Ease.OutCubic)`.

## 5. API e geração de bindings

### 5.1 Superfície

`Entity`, `Transform`, todos os componentes registrados, `World` (Instantiate, Destroy, Find), `SceneManager`, `Time`, `Input` (actions), `Physics` (consultas), `Animator`, `Audio`/`AudioMixer`, `NavMesh`, `UI` (modelos de dados), `Assets` (carregar por `AssetRef`), `Debug` (log, desenho), `Gizmos`, `Mathf`, `Random`, `Vector2/3/4`, `Quaternion`, `Color`, `Matrix4x4`, `Curve`, `Gradient`, `Save` (JSON em `user://`), `Tween`, `Timer`.

### 5.2 Geração

```
TypeRegistry (C++) ──api-dump──► api.json ──┬─► astra.d.luau     (tipos para analisador, IDE e luau-lsp no PC)
                                             ├─► thunks C++ rápidos (Transform, matemática, Rigidbody, Input)
                                             └─► referência de API (docs) com exemplos
```

- Tipos fora da lista quente usam o **caminho genérico por reflexão** (Variant), mais lento, porém correto e automático.
- **Vetores sem alocação:** `Vector3` usa o tipo nativo `vector` do Luau (sem alocar no GC). `Quaternion`, `Color` e `Matrix4x4` usam userdata com pool ou representação compacta a definir no S-08, com medição de pressão de GC.
- Resultados de consulta (raycast) reaproveitam tabelas do chamador quando ele passa um buffer (variantes `NonAlloc`).

### 5.3 Segurança de referências

- Userdata de objeto guarda **handle** (World + entidade com geração + tipo + instância), nunca ponteiro.
- Acesso a objeto destruído → erro claro ("Porta.alvo foi destruído no frame 812"), equivalente ao `MissingReferenceException`. `obj:IsValid()` para checar.
- Handles de um mundo de Play não funcionam depois do Stop.

## 6. Execução e isolamento

| Mecanismo | Regra |
|---|---|
| VMs | Uma por mundo (edição, Play). Pré-visualizações sem scripts, salvo `executeInEditMode` |
| Sandbox | `luaL_sandbox` (globais só leitura) + ambiente por script; bibliotecas permitidas: `math`, `string`, `table`, `coroutine`, `buffer`, `bit32`, `utf8`, `vector`; sem `io`/`os`/`debug` |
| Memória | Contabilizada no `lua_Alloc` por VM; limite suave (aviso) e duro (erro e parada do Play) por tier |
| Tempo | Callback `interrupt` verifica o tempo do callback atual; acima do limite (padrão 100 ms, configurável) o script é interrompido com erro "tempo excedido" e o componente é desabilitado. Protege o editor de laços infinitos (P-07) |
| Erros | Cada callback roda protegido; o erro vai ao console com pilha e `arquivo:linha` clicável; o componente recebe selo de erro na hierarquia e no Inspector; opção "pausar no erro" |

## 7. Compilação e hot reload

- Ao salvar, com debounce: compilação + análise (tipos e lint) num job; diagnósticos na IDE e no console.
- Erro de compilação **não derruba nada**: o código anterior continua rodando, e o Play não inicia enquanto houver erro bloqueante (com mensagem clara).
- **Hot reload em Play:** a tabela de métodos da classe é trocada; instâncias mantêm valores dos campos; campos novos recebem o padrão; campos removidos são descartados com aviso. A Unity não faz isso por padrão, então é uma melhoria.
- Player exportado: só bytecode (compilado na exportação); o compilador e o analisador não vão no APK do jogo.

## 8. Desempenho

- Interpretador por padrão. **Codegen nativo arm64** (`--!native` por módulo) habilitável no Android depois de validar no S-08 que a memória executável funciona no aparelho; no iOS, só interpretador.
- GC incremental com parâmetros por tier; Profiler mostra tempo por classe de script e por callback, alocações por frame e pausas de GC.
- Diretrizes no guia de scripting: evitar `GetComponent` em `Update` (cachear em `Start`), usar `NonAlloc`, preferir vetores.

## 9. Depurador

- Pontos de parada na IDE (calha), passo a passo (entrar, pular, sair), variáveis locais, upvalues, pilha, *watch*, avaliação de expressão.
- Implementado com a API de depuração do Luau (`lua_breakpoint`, `lua_singlestep`, `lua_getinfo`, locais). Cada callback de script roda como coroutine, e um ponto de parada **suspende a coroutine** (`lua_break`) e **pausa o mundo de Play**, enquanto a UI do editor continua respondendo. Viabilidade verificada no S-08.
- Servidor **DAP** para usar o VS Code no PC (host ou aparelho via `adb forward`).

## 10. IDE no editor (workspace Script)

- Editor de código como **elemento nativo** (estrutura *piece table*, desfazer próprio por arquivo, destaque de sintaxe pelo lexer do Luau, dobra de blocos, busca e substituição, ir para definição, tipos ao tocar e segurar, **autocompletar do analisador Luau**, diagnósticos inline).
- Ergonomia de toque: fileira de teclas extras (Tab, `( ) { } [ ] " = . : ,`, setas, desfazer), snippets (`Behavior`, `OnTriggerEnter`…), seleção por arrasto com alças, zoom por pinça. Retrato por padrão no celular (comportamento herdado da Astra atual).
- Arquivos externos: `.luau` são texto puro; no PC, `luau-lsp` com `astra.d.luau` dá a mesma experiência no VS Code.

## 11. Mensagens entre objetos

- Sinais tipados por script (`Astra.signal()`) com `Connect/Disconnect`, ligáveis no Inspector (como os *UnityEvent* e os sinais da Godot).
- `SendMessage` por nome **não** é oferecido (lento e frágil); a alternativa documentada são sinais ou `GetComponent`.

## 12. Visual scripting (F14)

Herança da ADR-06: **uma AST**, três vistas (blocos, grafo, código). A AST gera **Luau legível** e é a fonte de verdade; editar o Luau gerado converte o script em código (com aviso). O mesmo runtime, a mesma depuração e os mesmos bindings servem às três vistas, sem implementação paralela de execução.

## 13. Aceite (F6)

- Porta, plataforma móvel, coletável com contador na UI e inimigo que persegue (F10) escritos em Luau **no aparelho**, com Inspector, prefabs e Undo funcionando nos campos de script.
- Laço infinito num `Update` é interrompido em ≤ limite configurado; o editor segue responsivo e o erro aponta a linha.
- Hot reload em Play muda o comportamento sem reiniciar e sem perder estado.
- Ponto de parada pausa o jogo com a UI do editor responsiva; inspeção de locais funciona.
- Medição: custo de `Update` vazio vs. não registrado; 1.000 scripts com `Update` simples em T1, tempo registrado.
