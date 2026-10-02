# 05 — Modelo de objetos e reflexão

Este é o contrato central da Astra 2. Inspector, scripts, serialização, prefabs, Undo, animação de propriedades e documentação de API **leem a mesma descrição de tipo**. Não existem tabelas paralelas.

---

## 1. Três tipos de coisa

| Tipo | Exemplo | Vive em | Identidade persistente | Identidade em runtime |
|---|---|---|---|---|
| **Entidade + componentes** | Porta, câmera, luz, personagem | `World` (flecs) | `LocalId` (64 bits, por arquivo de cena/prefab) | `astra::Entity` (id flecs com geração + World) |
| **Recurso** | Mesh, Material, Texture, AudioClip, AnimationClip, Prefab, InputActions | Cache de recursos, com contagem de referência | `AssetGuid` (128 bits) + `SubId` para sub-recursos | `Ref<T>` |
| **Objeto de serviço** | Plugin do editor, painel, configurações do projeto | Heap, com dono explícito | Nome do tipo (singletons) ou nenhuma | Ponteiro com dono, `ObjectId` para referências fracas |

Os três são descritos no `TypeRegistry`.

## 2. Referências estudadas e o que vem de cada uma

| Conceito | Referência | Astra 2 | Classificação |
|---|---|---|---|
| `Object` + `GDCLASS` + `cast_to` sem RTTI | Godot 4.7 *Object class* | `astra::Object` + `ASTRA_TYPE` + `objectCast<T>` | Adaptação explícita (entidades não são Objects) |
| `ClassDB` (`bind_method`, `ADD_PROPERTY`, `ADD_SIGNAL`) | Godot 4.7 `ClassDB` | `TypeRegistry` + `TypeBuilder<T>` | Equivalente, com metadados extras |
| `Variant` | Godot 4.7 `Variant` | `astra::Variant` | Equivalente (conjunto de tipos próprio) |
| `PropertyInfo(type, name, hint, hint_string, usage)` | Godot 4.7 | `PropertyInfo` estendido (§4) | Adaptação: id estável, unidade, mutabilidade em Play, invalidação, consumidor |
| `RefCounted`/`Ref`, `ObjectID` | Godot 4.7 | `RefCounted`/`Ref<T>`, `ObjectId` | Equivalente |
| `Resource` (`resource_local_to_scene`, `duplicate`) | Godot 4.7 `Resource` | `Resource` ([07](07-RECURSOS-E-PIPELINE-DE-ASSETS.md)) | Equivalente |
| `Node` + `SceneTree` (process mode, grupos, `queue_free`) | Godot 4.7 | Entity + World: `ProcessMode`, tags, destruição adiada | Adaptação: hierarquia de entidades com componentes, não nós especializados |
| `Entity` + `EntityComponent` + `TransformComponent` obrigatório | Stride `Entity`/`EntityComponent` | Entity sempre com `Transform` | Equivalente |
| `EntityProcessor` (processa componentes de um tipo) | Stride | Sistemas de sincronização em `components/` | Equivalente no papel |
| `GameObject` (`activeSelf`/`activeInHierarchy`, tag, layer, `GetComponent`) | Unity 6.0 [U1] | Fachada de script `Entity` | Equivalente na API de script |
| Atributos de reflexão que dirigem o editor (clamp, default, categoria) | ezEngine `ezRTTI` + atributos | Builders de `PropertyInfo` (`.range()`, `.group()`…) | Equivalente no papel |

## 3. World, Entity e hierarquia

### 3.1 World

- Encapsula um `flecs::world`. Instâncias: **edição** (documento aberto), **Play**, **pré-visualização** (edição de prefab isolada, thumbnails, preview de animação) e **validação** (importação).
- Cada World tem: pipeline de sistemas (fases de [04](04-ARQUITETURA-CAMADAS-E-REPOSITORIO.md) §6), servidores associados (instâncias de `render::World`, `physics::World`…), relógio próprio, fila de comandos adiados.
- Nenhum tipo `flecs::` aparece fora de `engine/world`. A API pública é `astra::World`, `astra::Entity`, `astra::Query<...>`.

### 3.2 Entidade

| Propriedade | Tipo | Semântica | Referência |
|---|---|---|---|
| `name` | `String` | Livre, não precisa ser único | U1 |
| `localId` | `u64` | Persistente no arquivo; gerado aleatoriamente; nunca reaproveitado | — |
| `activeSelf` | `bool` | Estado próprio | U1 |
| `activeInHierarchy` | `bool` (só leitura) | `activeSelf` de toda a cadeia; cacheado e propagado | U1 |
| `tags` | conjunto de `StringName` da lista do projeto | Consulta rápida; Unity tem uma tag por objeto, Godot tem grupos | Adaptação (múltiplas tags) |
| `layer` | `0..31` | Filtro de câmera, física e raycast | U1 |
| `processMode` | `Inherit, Pausable, WhenPaused, Always, Disabled` | Comportamento sob pausa | Godot `Node.process_mode` |
| `staticFlags` | bits (`Render`, `Navigation`, `Occluder`, `ReflectionProbe`) | Dicas para bake e batching; só existem bits com consumidor | U1 (Static) |
| `hideFlags` | bits do editor (`HideInHierarchy`, `NotEditable`, `DontSave`) | Objetos internos do editor | Unity `HideFlags` |
| `prefabLink` | referência à instância de prefab | Ver [06](06-CENA-PREFABS-SERIALIZACAO.md) | U10 |

### 3.3 Hierarquia

- Implementada com o relacionamento `ChildOf` do flecs e o traço `OrderedChildren` (ordem dos filhos estável e controlável).
- Operações: `setParent(parent, PosePolicy::KeepWorld | KeepLocal)`, `setSiblingIndex(i)`, `children()`, `parent()`, `root()`, `findChild(path)`.
- **Ciclos rejeitados** com erro explícito (pai não pode ser descendente).
- Destruir um pai destrói os filhos (depth-last, como o flecs), com `OnDisable`/`OnDestroy` na ordem documentada.
- Multi-cena: cada cena carregada tem uma entidade-raiz oculta; mover entidade entre cenas é um reparent entre raízes.
- Desempenho a validar no S-06: reparent e reordenação frequentes no editor (arrastar na hierarquia) e 100 mil entidades em hierarquias profundas.

### 3.4 Transform

| Campo | Tipo | Observação |
|---|---|---|
| `localPosition` | `Vec3` (m) | |
| `localRotation` | `Quat` | Fonte de verdade |
| `localScale` | `Vec3` | Escala não uniforme permitida |
| `eulerHint` | `Vec3` (graus), só editor | Evita "saltos" de Euler no Inspector, como o `m_LocalEulerAnglesHint` da Unity |
| `worldMatrix` | afim 3×4 em cache | Suporta cisalhamento vindo de escala não uniforme + rotação |
| `version` | `u32` | Incrementa quando a pose de mundo muda; consumidores (render, física, áudio) comparam versões |

- Conversões local ↔ mundo, `lossyScale` aproximada (documentado: com cisalhamento não existe decomposição exata).
- **Quem escreve a pose** (regra global): editor/scripts escrevem o `Transform`; física dinâmica escreve depois do passo; animação escreve na fase `Animation`; constraints depois da animação. Conflitos (ex.: script movendo corpo dinâmico) recebem aviso e seguem a política do componente (`Rigidbody.MovePosition` vs teleporte).

### 3.5 Coordenadas e unidades (D-11)

Sistema **destro, Y para cima, −Z à frente, +X à direita** (o mesmo de Godot e glTF), em metros, quilogramas e segundos. Ângulos em radianos na API interna e em graus no Inspector e na API de script (a API de script segue a convenção Unity de graus nos ângulos públicos). A importação de FBX converte eixos e unidades com `ufbx`; a de glTF não precisa converter.

> Diferença com a Unity, que é canhota: documentada como **adaptação explícita**. Scripts portados da Unity que usem `Vector3.forward` funcionam, porque `forward` na Astra é `(0, 0, −1)`; a constante tem o mesmo significado semântico.

## 4. TypeRegistry e PropertyInfo

### 4.1 TypeInfo

| Campo | Conteúdo |
|---|---|
| `id` | `TypeId` = hash estável do nome canônico (`astra.RigidBody`) + lista de aliases para renomeações |
| `kind` | `Component`, `Resource`, `Struct` (valor), `Enum`, `Flags`, `Object` |
| `parent` | Tipo base (componentes podem herdar contrato, ex.: `Collider` → `BoxCollider`) |
| Layout | Tamanho, alinhamento, construtor, destrutor, cópia, move (o flecs recebe esses hooks) |
| Editor | Nome de exibição (chave de localização), categoria, ícone, ordem no menu, ajuda (link de doc) |
| Composição | `requires<T...>`, `conflicts<T...>`, `allowMultiple`, `disallowRemoval` (`Transform`) |
| Mutabilidade | `structuralInPlay`: `Never` / `SafePoint` (herdado de `component_schema.h`) |
| Membros | Propriedades, métodos, eventos, constantes |
| Versão | `u32` + funções de migração entre versões ([06](06-CENA-PREFABS-SERIALIZACAO.md) §7) |

### 4.2 PropertyInfo

| Campo | Conteúdo | Exemplo (`Light.range`) |
|---|---|---|
| `id` | `PropertyId` estável (hash da chave persistente) + aliases | `range` |
| `type` | `VariantType` | `Float` |
| `unit` | `Meter, Degree, Radian, Kilogram, Second, Lux, Lumen, Candela, Kelvin, Percent, Decibel, Hertz…` | `Meter` |
| `default` | `Variant` | `10.0` |
| Faixa | `min`, `max`, `softMin`, `softMax`, `step` | `0.01 … ∞`, soft `0 … 100` |
| `hint` | `Range, Enum, Flags, LayerMask, Layer, Tag, AssetRef(tipo), EntityRef(componente exigido), Color(hdr), Angle, Multiline, Curve, Gradient, FilePath, Password, ExpEasing` | `Range` |
| `usage` | `Serialize, Editor, Script, ReadOnly, Advanced, EditorOnly, RuntimeOnly, Animatable, Overridable, NoUndo` | `Serialize|Editor|Script|Animatable|Overridable` |
| `group` | Seção e subseção no Inspector | `"Emission"` |
| `visibleIf` / `enabledIf` | Predicado simples sobre outra propriedade (igual / diferente / em conjunto) | visível se `type ∈ {Point, Spot}` |
| `playMutability` | `Never`, `SafePoint`, `Live` | `Live` |
| `invalidates` | Bits: `RenderInstance, RenderLight, Material, Bounds, PhysicsBody, PhysicsShape, NavMesh, AnimGraph, Audio, UILayout, Transform` | `RenderLight|Bounds` |
| `consumer` | Sistema que produz o efeito, nomeado e testado | `RenderSyncSystem → render::World::setLightRange` |
| Acesso | Campo direto ou par getter/setter | setter |
| `validate` | Função opcional que devolve `Error` com mensagem para o usuário | rejeita NaN |
| `docKey` | Âncora da documentação | `light.range` |

**Regra de integridade (P-10):** uma propriedade com `usage` de Editor ou Script **precisa** declarar `consumer`. O teste `reflection_consumers` percorre o registro e falha se algum consumidor estiver ausente ou não tiver caso de teste associado. Assim um campo sem efeito nunca chega ao Inspector.

### 4.3 Registro

```cpp
// engine/components/light_reflect.cpp
void reflect(TypeBuilder<Light>& t) {
  t.category("Rendering").icon("component-light").requires<Transform>()
   .structuralInPlay(PlayMutability::SafePoint);

  t.property("type", &Light::type, &Light::setType)
   .hint(Hint::Enum).defaultValue(LightType::Point)
   .invalidates(Inv::RenderLight | Inv::Bounds).consumer("render.light");

  t.property("color", &Light::color, &Light::setColor)
   .hint(Hint::Color).defaultValue(Color::White).animatable()
   .invalidates(Inv::RenderLight).consumer("render.light");

  t.property("range", &Light::range, &Light::setRange)
   .unit(Unit::Meter).range(0.01f, kInf).softRange(0.f, 100.f).defaultValue(10.f)
   .visibleIf("type", {LightType::Point, LightType::Spot}).animatable()
   .invalidates(Inv::RenderLight | Inv::Bounds).consumer("render.light");
}
```

- O registro é **explícito por módulo**: `registerRenderingTypes(registry)`. Não depende de inicialização estática, que tem ordem frágil no Android.
- `tools/api-dump` exporta o registro em `api.json`, que alimenta a geração de `.d.luau`, da referência de API e dos thunks de binding ([14](14-SCRIPTING-LUAU.md) §5).

### 4.4 Variant

| Grupo | Tipos |
|---|---|
| Escalares | `Nil, Bool, Int (i64), Float (f64 armazenado; f32 nas propriedades), Enum (TypeId + valor), Flags` |
| Matemática | `Vec2, Vec3, Vec4, Quat, Color (linear, f32×4), Mat3x4, Mat4, Rect, AABB` |
| Texto | `String` (UTF-8), `StringName` (internada) |
| Referências | `EntityRef` (LocalId + resolução no World), `ComponentRef`, `AssetRef` (AssetGuid + SubId + TypeId), `ObjectRef` (fraca) |
| Coleções | `Array` (tipado opcionalmente), `Dictionary` |
| Domínio | `Curve`, `Gradient`, `LayerMask` |

- Usado no editor, na serialização, no Undo e nos scripts dinâmicos. **Não** é usado no caminho quente do runtime: sistemas acessam componentes tipados.
- Codificação textual estável (para o formato de cena) e binária (para o cozido e o WAL).

## 5. Componentes

### 5.1 Armazenamento e fachada

- Dados no armazenamento do flecs (arquétipos). A classe do componente expõe getters e setters; o setter grava e **marca a mudança** (contador por componente lido pelos sistemas de sincronização).
- `enabled` por componente (o `Behaviour.enabled` da Unity) usa o recurso de habilitar/desabilitar componente do flecs quando o componente é único.

### 5.2 Multiplicidade

O flecs guarda **um** componente de cada tipo por entidade. A Unity permite vários (scripts, AudioSource, colisores). Decisão:

- Tipos com `allowMultiple` são guardados num contêiner por tipo (`Multi<T>`: pequeno vetor com `instanceId` estável por entidade).
- A fachada apresenta cada instância como componente independente. Referência persistente: `(entity LocalId, TypeId, instanceId)`.
- Consultas de sistemas iteram as instâncias internas.
- Comportamentos de script ficam num `Behaviors` com N instâncias de scripts diferentes ou repetidos.
- **Validar no S-06:** custo de adicionar e remover instâncias e iteração em cenas com milhares de colisores.

### 5.3 Composição atômica

`AddComponent<CharacterController>()` resolve `requires` recursivamente, verifica `conflicts` e monta um **plano de composição**. O plano é aplicado inteiro ou não é aplicado: em caso de conflito, nada fica pela metade. No editor, o plano vira **uma transação** de Undo.

### 5.4 Ciclo de vida

| Evento | Quando | Ordem |
|---|---|---|
| `Awake` | Primeira vez que a entidade fica ativa na hierarquia após criação/carga | Por cena: profundidade primeiro, na ordem dos filhos |
| `OnEnable` | Componente habilitado **e** entidade ativa na hierarquia | Idem |
| `Start` | Antes do primeiro `Update` em que o componente estiver habilitado | Fase `Initialization` |
| `OnDisable` | Inverso de `OnEnable` | Inverso da profundidade |
| `OnDestroy` | Destruição (fase `Cleanup`) | Filhos antes dos pais |

- Prioridade de execução por tipo (`executionOrder`, como o *Script Execution Order* da Unity) aplicada antes da ordem de hierarquia.
- Componentes nativos e scripts seguem o mesmo ciclo. A diferença com a Unity (que não garante ordem entre objetos) é uma **adaptação explícita**: a Astra 2 é determinística.

## 6. PropertyPath — um endereço para tudo

```
<entidade>/<Componente>[#instância]/<propriedade>[.<campo>][<índice>]
ex.:  Player/Light/color.r        Door/Behaviors#2/openAngle        Enemy/MeshRenderer/materials[1]
```

O mesmo endereço é usado por:

- **Undo** (comando `SetProperty(path, old, new)`);
- **overrides de prefab** (lista de `PropertyPath` modificados);
- **clipes de animação** (curvas sobre qualquer propriedade `Animatable`);
- **Inspector** (multi-edição, cópia/cola de valores);
- **busca global** (`t:Light range>20`);
- **Timeline** (F14).

O caminho da entidade é relativo (nome + índice entre irmãos homônimos) dentro de clipes e prefabs, e por `LocalId` no WAL e no Undo.

## 7. Fachada de script (estilo GameObject)

| Membro | Semântica |
|---|---|
| `entity.name`, `entity.activeSelf`, `entity.activeInHierarchy`, `entity:SetActive(b)` | U1 |
| `entity.tags`, `entity:HasTag(t)` · `entity.layer` | U1 (tags múltiplas) |
| `entity.transform`, `entity.parent`, `entity:GetChildren()` | U2 |
| `entity:GetComponent(T)`, `GetComponents(T)`, `GetComponentInChildren(T)`, `GetComponentInParent(T)` | U1 |
| `entity:AddComponent(T)` (aplica composição), `Destroy(component)` | U1 |
| `World:Instantiate(prefabOrEntity, parent?, pose?)` | U10 |
| `Destroy(entity, delay?)` (adiado para `Cleanup`) | U1 |
| `World:FindWithTag(t)`, `World:Find(path)` | Disponíveis, documentados como lentos |

Detalhes, ciclo de vida e corrotinas em [14](14-SCRIPTING-LUAU.md).

## 8. Inventário da família "Objetos e composição"

| Capacidade | Unity 6.0 | Godot 4.7 | Fase | Classificação planejada |
|---|---|---|---|---|
| Identidade estável e nome | [GameObject](https://docs.unity3d.com/6000.0/Documentation/Manual/class-GameObject.html) | [Node](https://docs.godotengine.org/en/stable/classes/class_node.html) | F1 | Equivalente |
| Ativo próprio/efetivo | GameObject.SetActive | `process_mode`, `visible` | F1 | Equivalente |
| Hierarquia ordenada, reparent com pose | [Transform](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Transform.html) | Node3D | F1 | Equivalente |
| Tags e layers | [Tags and Layers](https://docs.unity3d.com/6000.0/Documentation/Manual/class-TagManager.html) | Groups | F1 | Adaptação (tags múltiplas) |
| Componentes múltiplos do mesmo tipo | GameObject | (nós filhos) | F1 | Equivalente via `Multi<T>` |
| Requisitos e conflitos | `RequireComponent`, `DisallowMultipleComponent` | — | F1 | Equivalente |
| Process mode sob pausa | — | `Node.process_mode` | F1 | Equivalente (Godot) |
| Destruição adiada | `Destroy` | `queue_free` | F1 | Equivalente |
| Instanciação | `Object.Instantiate` | `PackedScene.instantiate` | F1/F4 | Equivalente |
| Duplicação com remapeamento de referências internas | Ctrl+D | `duplicate` | F4 | Equivalente |

## 9. Aceite desta camada (F1)

- Round-trip texto → mundo → texto **byte a byte idêntico** num corpus de cenas sintéticas (todos os tipos de `Variant`, hierarquias profundas, componentes múltiplos).
- Reparent com `KeepWorld` preserva a pose de mundo (tolerância documentada) com escala não uniforme e rotações arbitrárias.
- Ciclo rejeitado com erro; composição com conflito não altera o mundo.
- `reflection_consumers` verde: nenhuma propriedade exposta sem consumidor.
- Teste de vida: 10 mil criações/destruições com handles antigos sempre reportados como inválidos.
- Ciclo de vida: ordem de `Awake/OnEnable/Start/OnDisable/OnDestroy` verificada em árvore de teste com ativação e desativação no meio.
