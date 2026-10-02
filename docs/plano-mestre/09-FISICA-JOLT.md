# 09 — Física com Jolt

Backend: **Jolt Physics v5.6.0** (MIT). Referências de comportamento: Unity 6.0 [Rigidbody](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Rigidbody.html), [Box Collider](https://docs.unity3d.com/6000.0/Documentation/Manual/class-BoxCollider.html), Character Controller, Joints, *Layer-based collision*; Godot 4.7 `RigidBody3D`, `CharacterBody3D`, `PhysicsServer3D`.

---

## 1. Arquitetura

```
Rigidbody / Colliders / Joints / CharacterController / Vehicle      (components/)
        │ PhysicsSyncSystem (fase PrePhysics) — cria/atualiza corpos, formas, juntas; aplica filas
        ▼
physics::World                                                      (servers/physics)
        │ createBody, createShape, createJoint, step, raycast, shapeCast, overlap, events
        ▼
JoltPhysicsBackend                                                  (backends/jolt)
        │ BodyInterface, PhysicsSystem, camadas, listeners, CharacterVirtual, VehicleConstraint
        ▼
Jolt
```

- **Um `physics::World` por `astra::World`.** O mundo de Play tem física própria; o de edição só tem consultas (picking físico, colocação sobre superfícies) com corpos estáticos e cinemáticos.
- Os callbacks de contato do Jolt rodam em threads de worker. Eles **só gravam numa fila** sem lock; o despacho para scripts acontece na thread principal, depois do passo.

## 2. Passo e tempo

| Item | Regra |
|---|---|
| Passo fixo | `fixedDeltaTime` (padrão 1/60 s, configurável), acumulador com máximo de subpassos por frame (`maxSubSteps`) |
| Modos de simulação | `FixedUpdate` (padrão), `Update`, `Script` (o usuário chama `Physics.Simulate(dt)`), como o `Physics.simulationMode` da Unity |
| Interpolação | Por corpo: `None`, `Interpolate`, `Extrapolate`; a pose de render é calculada no `Transform` visual sem alterar a pose física |
| Escrita de volta | Só corpos ativos (`GetActiveBodies`) escrevem `Transform` depois do passo |
| Determinismo | Determinístico para o mesmo build e a mesma ordem de entrada. Determinismo entre plataformas (`JPH_CROSS_PLATFORM_DETERMINISTIC`) fica desligado por custo, a reavaliar se surgir rede |

## 3. Camadas e filtros

- **32 layers do usuário** (as mesmas de [05](05-MODELO-DE-OBJETOS-E-REFLEXAO.md) §3.2) + **matriz de colisão 32×32** em `ProjectSettings/physics.json` (Layer Collision Matrix da Unity).
- `ObjectLayer` do Jolt = (layer do usuário, tipo de movimento). `BroadPhaseLayer`: `Static`, `Dynamic`, `Sensor`.
- Por corpo: `includeLayers`/`excludeLayers` (overrides do Rigidbody da Unity 6).
- Consultas recebem `LayerMask` e `QueryTriggerInteraction` (`UseGlobal`, `Ignore`, `Collide`).

## 4. Componentes

### 4.1 Rigidbody

| Propriedade | Tipo/unidade | Consumidor | Observação |
|---|---|---|---|
| `mass` | kg | Jolt `MassProperties` | `automaticCenterOfMass`/`automaticTensor` como na Unity 6 |
| `linearDamping`, `angularDamping` | 1/s | `MotionProperties` | Nomes da Unity 6 (antigos `drag`/`angularDrag` como alias de script) |
| `useGravity`, `gravityScale` | bool, fator | `GravityFactor` | `gravityScale` é extra (Godot) |
| `isKinematic` | bool | `EMotionType::Kinematic` | Movimento por `MovePosition`/`MoveRotation` → `MoveKinematic` (velocidade derivada, empurra corretamente) |
| `interpolation` | enum | §2 | |
| `collisionDetection` | `Discrete`, `Continuous` (`LinearCast`), `ContinuousSpeculative` | `EMotionQuality` | Mapeamento documentado: o Jolt tem `Discrete` e `LinearCast` |
| `constraints` | congelar posição/rotação por eixo | `EAllowedDOFs` | |
| `linearVelocity`, `angularVelocity` | m/s, rad/s | `BodyInterface` | Leitura e escrita |
| `maxLinearVelocity`, `maxAngularVelocity` | | `MotionProperties` | |
| `sleepThreshold`, `isSleeping`, `WakeUp()`, `Sleep()` | | Ativação do Jolt | |
| `AddForce/AddTorque/AddForceAtPosition` com `ForceMode` (`Force`, `Acceleration`, `Impulse`, `VelocityChange`) | | Fila aplicada no PrePhysics | |
| `centerOfMass`, `inertiaTensor` (manual) | | `OffsetCenterOfMassShape`/massa manual | |

### 4.2 Colisores

| Colisor | Forma Jolt | Unity | Godot | Notas |
|---|---|---|---|---|
| `BoxCollider` (`center`, `size`) | `BoxShape` (+ `RotatedTranslatedShape`) | ✓ | BoxShape3D | Raio de convexo configurável |
| `SphereCollider` (`center`, `radius`) | `SphereShape` | ✓ | SphereShape3D | |
| `CapsuleCollider` (`center`, `radius`, `height`, `direction`) | `CapsuleShape` | ✓ | CapsuleShape3D | |
| `CylinderCollider` | `CylinderShape` | — | CylinderShape3D | Extra (Godot) |
| `MeshCollider` (`mesh`, `convex`) | `MeshShape` / `ConvexHullShape` | ✓ | ConcavePolygon/ConvexPolygon | Malha estática só em corpo estático/cinemático, como na Unity; cozido em `Library/` |
| `HeightfieldCollider` | `HeightFieldShape` | TerrainCollider | HeightMapShape3D | F13 com terreno |
| Comuns | `isTrigger`, `material`, `layerOverrides`, `contactOffset` (mapeado para o raio de convexo quando aplicável) | | | |

**Composição:** colisores na entidade do Rigidbody **e nos filhos sem Rigidbody próprio** formam um composto (`StaticCompoundShape`, ou `MutableCompoundShape` se mudam em Play), com a mesma semântica da Unity. Escala não uniforme em filho rotacionado nem sempre é representável: gera aviso com o colisor afetado, nunca uma forma silenciosamente errada.

### 4.3 PhysicsMaterial

`dynamicFriction`, `staticFriction`, `bounciness`, `frictionCombine`, `bounceCombine` (`Average`, `Minimum`, `Multiply`, `Maximum`). O Jolt usa um atrito só; o atrito estático é aproximado com documentação (adaptação explícita). A combinação vem do callback de combinação de material do Jolt.

### 4.4 Juntas

| Junta Astra | Constraint Jolt | Unity | Godot |
|---|---|---|---|
| `FixedJoint` | `FixedConstraint` | FixedJoint | — (Generic6DOF travado) |
| `HingeJoint` (limites, motor, mola) | `HingeConstraint` | HingeJoint | HingeJoint3D |
| `SliderJoint` (limites, motor) | `SliderConstraint` | (ConfigurableJoint) | SliderJoint3D |
| `SpringJoint` / `DistanceJoint` | `DistanceConstraint` (com mola) | SpringJoint | — |
| `ConeTwistJoint` / `CharacterJoint` | `SwingTwistConstraint` | CharacterJoint | ConeTwistJoint3D |
| `ConfigurableJoint` (6 DOF, limites, motores, molas) | `SixDOFConstraint` | ConfigurableJoint | Generic6DOFJoint3D |
| `PointJoint` | `PointConstraint` | — | PinJoint3D |

Comuns: `connectedBody` (ou mundo), âncoras local/conectada com `autoConfigureConnectedAnchor`, `breakForce`/`breakTorque` (evento `OnJointBreak`), `enableCollision` entre os corpos ligados.

### 4.5 CharacterController

Sobre `JPH::CharacterVirtual` (movimento por varredura, sem corpo dinâmico):

| Membro | Semântica Unity |
|---|---|
| `slopeLimit` (°), `stepOffset` (m), `skinWidth`, `minMoveDistance`, `center`, `radius`, `height` | ✓ |
| `Move(motion)` → `CollisionFlags` (`Sides`, `Above`, `Below`) | ✓ |
| `SimpleMove(speed)` (com gravidade) | ✓ |
| `isGrounded`, `velocity` | ✓ |
| `OnControllerColliderHit(hit)` | ✓ |
| Empurrar corpos dinâmicos (força configurável) | Adaptação (Jolt `CharacterContactListener`) |
| Plataformas móveis (herdar velocidade do chão) | Extra, herança dos planos `CHARACTER-PLATFORMS` |

### 4.6 Veículos (F5, depois do núcleo)

`VehicleBody` + `Wheel` sobre `VehicleConstraint`/`WheeledVehicleController` do Jolt: motor (curva de torque), transmissão, diferenciais, suspensão (curso, rigidez, amortecimento), atrito longitudinal/lateral (curvas), freio e freio de mão. Classificado como **adaptação** do `WheelCollider` da Unity, que é um componente por roda sem modelo de motor.

### 4.7 Fora da F5

Ragdoll (F8, sobre esqueleto do Ozz + `Ragdoll` do Jolt), soft body/cloth (pendente), física 2D (fora da 2.0).

## 5. Eventos

| Evento de script | Origem | Dados |
|---|---|---|
| `OnTriggerEnter/Stay/Exit(other)` | Sensor ↔ corpo | Collider do outro |
| `OnCollisionEnter/Stay/Exit(collision)` | Contato entre corpos | Outro collider/rigidbody, pontos de contato (posição, normal, separação), impulso, velocidade relativa |
| `OnJointBreak(force)` | Junta | Força no momento da quebra |

- `Stay` tem custo: entregue só a quem implementa o callback (o ScriptHost registra interesse por tipo de script).
- Gatilho cinemático contra estático exige configuração no Jolt (`mCollideKinematicVsNonDynamic`); a regra exata da Unity (quais pares geram trigger) entra numa tabela de teste.
- Os eventos são despachados depois do passo, na ordem dos contatos, numa estrutura reaproveitada (sem alocação por evento).

## 6. Consultas

| Consulta | Variantes | Resultado |
|---|---|---|
| `Raycast` | Primeiro, todos (`RaycastAll`), sem alocação (buffer do chamador) | `RaycastHit { point, normal, distance, collider, rigidbody, triangleIndex, barycentric }` |
| `SphereCast`, `BoxCast`, `CapsuleCast` | Idem | Idem |
| `OverlapSphere/Box/Capsule`, `CheckSphere/Box/Capsule` | Com buffer | Lista de colliders |
| `ClosestPoint`, `ComputePenetration` | — | Ponto / direção e distância |

Regras: `LayerMask`, `QueryTriggerInteraction`, distância máxima. Com buffer do chamador, o retorno informa **quantos acertos existiam** e se houve **truncamento** (nunca truncamento silencioso; requisito do AGENTS). Consultas valem no editor (mundo de edição) e em Play.

## 7. Configurações do projeto (`physics.json`)

Gravidade, material padrão, matriz de colisão, `fixedDeltaTime`, `maxSubSteps`, iterações de velocidade/posição do solver (Jolt `mNumVelocitySteps`/`mNumPositionSteps`), limiar de sono, limiar de quique, consultas acertam triggers por padrão, consultas acertam faces de trás, modo de simulação.

## 8. Editor

- Gizmos de colisores com **edição por alças** (tamanho, raio, altura, centro), como o *Edit Collider* da Unity.
- "Ajustar à malha" (bounds da malha → caixa/cápsula/esfera), herança de `editor_collider_fit.h` da Astra atual.
- Gizmos de juntas: eixos, âncoras, arcos de limite.
- Editor visual da matriz de colisão (grade com rótulos girados, toque alterna).
- **Depurador de física** (Play): corpos ativos/dormindo por cor, contatos, AABBs, consultas recentes desenhadas com o resultado.
- Colocação sobre superfície no editor ("soltar no chão"), via raycast no mundo de edição.

## 9. Desempenho

- Inserção em lote de corpos ao carregar cena (`AddBodiesPrepare/Finalize`).
- Formas de malha e convexas **cozidas** e salvas em binário no `Library/` (estado binário do Jolt), sem recozinhar no carregamento.
- Jobs do Jolt no sistema de jobs Astra; número de workers por tier.
- Orçamento por tier: número de corpos ativos e de contatos; o Profiler mostra o tempo do passo, número de corpos ativos e de pares.

## 10. Aceite (F5)

| Cena de teste | Critério |
|---|---|
| Pilha de 10 caixas | Estável por 60 s, sem tremor visível nem afundamento acima do limite documentado |
| Rampa, escada e plataforma móvel com CharacterController | `slopeLimit` e `stepOffset` respeitados; personagem acompanha a plataforma |
| Porta com HingeJoint e motor, aberta por trigger + script | Funciona criada **só pela UI** |
| Raycast de picking em Play e no editor | Acerta o colisor certo com layer mask e trigger interaction |
| 1.000 corpos dinâmicos (T2) | Tempo de passo medido e registrado; sem queda abaixo do orçamento do tier |
| Play/Stop 100× | Sem vazamento de corpos ou formas (contadores do Jolt voltam a zero) |
| Consulta com buffer pequeno | Retorna contagem total e sinaliza truncamento |
