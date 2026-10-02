# 10 — Animação com Ozz

Backend: **ozz-animation 0.17.0** (MIT). Referências de comportamento: Unity 6.0 *Animator*, *Animator Controller*, *Animation State Machines*, *Blend Trees*, *Avatar Mask*, *Root Motion*, *Animation Events*, *Animation window*; Unity Animation Rigging (constraints); Godot 4.7 `AnimationPlayer`, `AnimationTree`, `AnimationNodeStateMachine`, `SkeletonIK3D`.

---

## 1. Arquitetura

```
Animator (componente) + AnimatorController (asset) + parâmetros vindos de script
        │ AnimationSystem (fase Animation): avalia máquinas de estado, pesos, transições
        ▼
anim::World  (servidor: instâncias de esqueleto, jobs de amostragem/blend/IK, trilhas de propriedade)
        ▼
OzzBackend  (SamplingJob, BlendingJob, LocalToModelJob, TwoBoneIKJob, AimIKJob)
        │ matrizes de skin → render::World (paleta de ossos por SkinnedInstance)
        ▼
Skinning na GPU (vertex shader ou compute de pré-skin; decisão por medição)
```

Um único componente, `Animator`, cobre animação esquelética e de propriedades. Sem controller e com um clipe atribuído, ele toca o clipe direto. Assim não existem dois sistemas paralelos (`Animation` legado + `Animator`), que a Unity mantém por compatibilidade histórica.

## 2. Assets

| Asset | Conteúdo | Origem |
|---|---|---|
| `Skeleton` | Esqueleto runtime do Ozz (hierarquia, pose de bind, nomes) | Import glTF/FBX |
| `AnimationClip` | Animação runtime do Ozz (trilhas de osso comprimidas) + trilhas de propriedade (float/vec/quat/cor) + eventos + curva de root motion | Import ou `.aanim` autorado |
| `AnimatorController` | Parâmetros, camadas, máquinas de estado, blend trees, transições | `.acontroller` |
| `AvatarMask` | Peso por osso (e por trilha de propriedade) | `.amask` |

### Pipeline de importação

1. fastgltf/ufbx → `RawSkeleton` e `RawAnimation` (biblioteca offline do Ozz).
2. Otimização com tolerâncias configuráveis (posição/rotação/escala, distância de influência) → relatório de erro máximo por osso.
3. Extração de root motion (translação/rotação do nó raiz para uma curva separada, com travas por eixo), com o utilitário do Ozz offline quando existir na 0.17 ou com implementação própria.
4. Serialização no formato de arquivo do Ozz dentro do artefato em `Library/`.

## 3. Runtime por personagem

| Etapa | Job Ozz | Notas |
|---|---|---|
| Amostragem de cada clipe ativo | `SamplingJob` (com contexto em cache por clipe) | Paralelo entre personagens |
| Mistura de estados e camadas | `BlendingJob` (pesos por camada, pesos por osso das máscaras, camadas aditivas) | |
| Espaço local → modelo | `LocalToModelJob` | |
| IK | `TwoBoneIKJob`, `AimIKJob` | Constraints, §6 |
| Paleta de skin | Multiplicação pela inversa do bind | Escrita direto no buffer do render |

- **Skinning na GPU:** no vertex shader ou num compute de pré-skin quando a malha é desenhada várias vezes (cascatas de sombra + principal). A escolha sai de medição na F8.
- **Morph targets** (blend shapes): deltas num buffer, pesos animáveis por trilha de propriedade; limite de morphs ativos por malha e por tier.
- **Modos de atualização:** `Normal`, `AnimatePhysics` (no passo fixo), `UnscaledTime`.
- **Culling:** `AlwaysAnimate`, `CullUpdateTransforms`, `CullCompletely`, com base na visibilidade dos renderers.
- **LOD de animação:** taxa de atualização reduzida por distância/tamanho em tela (configurável), uma melhoria além da Unity.

## 4. Animator Controller

| Recurso | Detalhe | Referência | Fase |
|---|---|---|---|
| Parâmetros | `Float`, `Int`, `Bool`, `Trigger` | Unity Animator | F8 |
| Estados | Motion (clipe ou blend tree), velocidade + multiplicador por parâmetro, offset de ciclo, tag | ✓ | F8 |
| Transições | Condições, *has exit time*, *exit time*, duração fixa ou normalizada, offset, fonte de interrupção e interrupção ordenada | ✓ | F8 |
| Any State, Entry, Exit | ✓ | ✓ | F8 |
| Sub-máquinas de estado | ✓ | ✓ | F8 |
| Camadas | Peso, `Override`/`Additive`, máscara | ✓ | F8 |
| Camadas sincronizadas | — | ✓ | Pendente |
| Blend trees | 1D, 2D simples direcional, 2D freeform direcional, 2D freeform cartesiano, Direct | ✓ | F8 |
| Comportamentos de estado | Scripts Luau com `OnStateEnter/Update/Exit` | StateMachineBehaviour | F8 |
| Eventos de animação | Nome de função + parâmetro (float/int/string/objeto) → chamada no script da entidade | Animation Events | F8 |
| Root motion | `applyRootMotion`; destino: Transform, Rigidbody (`MovePosition`) ou CharacterController (`Move`); callback `OnAnimatorMove` | ✓ | F8 |
| API de script | `SetFloat/SetInteger/SetBool/SetTrigger/ResetTrigger`, `Play`, `CrossFade`, `GetCurrentStateInfo`, `IsInTransition`, `speed`, `GetLayerWeight/SetLayerWeight` | ✓ | F8 |

## 5. Animação de propriedades

- Curvas sobre **qualquer propriedade `Animatable`** do TypeRegistry, endereçadas por `PropertyPath` ([05](05-MODELO-DE-OBJETOS-E-REFLEXAO.md) §6): intensidade de luz, cor de material (via `MaterialPropertyBlock`), transform de objetos sem esqueleto, campos de script.
- Avaliação na fase `Animation`, depois da esquelética, com a mesma mistura de camadas.
- Curvas: chaves com tangentes (auto, linear, constante, livre, quebrada), igual ao asset `Curve` usado em gameplay (herança de `CURVAS-GRADIENTES-*`).
- Gravação no editor: com "gravar" ligado, alterações no Inspector e nos gizmos viram chaves no tempo atual.

## 6. Constraints e IK (estilo Animation Rigging)

| Constraint | Base | Herança da Astra atual |
|---|---|---|
| `TwoBoneIK` (braço/perna, alvo + dica) | `TwoBoneIKJob` | — |
| `AimConstraint` / `LookAt` (cabeça, olhos, torre) | `AimIKJob` | `CONSTRAINTS-*` |
| `ParentConstraint`, `PositionConstraint`, `RotationConstraint`, `ScaleConstraint` | Própria | `CONSTRAINTS-*` |
| Peso por constraint, animável | — | — |

Avaliadas depois da amostragem e antes da paleta de skin, na ordem da hierarquia.

## 7. Editor

### 7.1 Janela Animação

- Dopesheet (linhas por propriedade/osso, chaves como losangos, seleção por caixa, mover/escalar chaves) e modo curvas (editor de curvas com tangentes).
- Régua de tempo em frames/segundos, *scrub* com o dedo, play/loop, ajuste de taxa de amostragem.
- Trilha de eventos (marcador com nome da função).
- **Modo gravar** (indicador vermelho no topo do editor, como na Unity): mudanças viram chaves.
- Pré-visualização num **mundo de pré-visualização** ou no próprio mundo de edição com restauração garantida da pose original ao sair.

### 7.2 Janela Animator

- Editor de grafo (pan, pinch-zoom, arrastar estados, criar transição puxando da borda do estado).
- Painel de parâmetros e de camadas; Inspector mostra estado/transição/blend tree selecionados.
- Blend tree 2D com visualização dos pontos de movimento e do ponto atual.
- **Em Play:** estado atual destacado com barra de progresso, transição ativa animada, parâmetros editáveis ao vivo.

## 8. Fora da F8

| Item | Motivo | Quando |
|---|---|---|
| Retargeting humanoide (Avatar, músculos) | Grande; depende do Animator estável | F14 |
| Timeline (sequências com trilhas de animação, áudio, ativação, sinal) | Ferramenta própria | F14 |
| Camadas sincronizadas, espelhamento | Dependem do humanoide | F14 |
| `KHR_animation_pointer` (glTF) | Depende de animação de propriedades estável | Depois da F8 |

## 9. Inventário

| Capacidade | Unity 6.0 | Godot 4.7 | Fase | Classificação planejada |
|---|---|---|---|---|
| Animator + Controller | Animator, Animator Controller | AnimationTree | F8 | Equivalente |
| Clipe de propriedades | Animation Clip | AnimationPlayer | F8 | Equivalente (num único componente) |
| SkinnedMeshRenderer | ✓ | Skeleton3D + MeshInstance3D | F8 | Equivalente |
| Blend shapes | ✓ | Blend shapes | F8 | Equivalente |
| Avatar Mask | ✓ | Filtros do AnimationTree | F8 | Equivalente (genérico) |
| Root motion | ✓ | Root motion | F8 | Equivalente |
| IK genérico | Animation Rigging | SkeletonIK3D | F8 | Equivalente |
| Humanoide | Avatar | Retarget | F14 | Pendente |

## 10. Aceite (F8)

- Personagem importado (glTF e FBX) anda/corre/pula com blend tree 1D/2D e transições configuradas **só pela UI**; parâmetros vindos de script Luau ligado a Input Actions.
- Root motion move o CharacterController sem deslizar (pé parado no chão, verificado por captura quadro a quadro).
- Máscara de camada: braço acenando sobre a locomoção.
- Evento de animação dispara som de passo (integração com F9).
- 50 personagens animados em T2 com tempo de animação medido e registrado; LOD de animação reduz o custo de forma mensurável.
- Gravar keyframes de luz e cor de material no editor; tocar em Play; resultado idêntico.
