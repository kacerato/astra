# 12 — Navegação com Recast/Detour

Backend: **Recast/Detour v1.6.0** + commit fixado do `main` (Zlib): Recast (voxelização e geração), Detour (malha e consultas), DetourCrowd (agentes e desvio), DetourTileCache (obstáculos dinâmicos). Referências de comportamento: Unity **AI Navigation 2.0** (NavMeshSurface, NavMeshAgent, NavMeshObstacle, NavMeshLink, NavMeshModifier); Godot 4.7 `NavigationRegion3D`, `NavigationAgent3D`, `NavigationObstacle3D`, `NavigationLink3D`, `NavigationServer3D`.

---

## 1. Arquitetura

```
NavMeshSurface / NavMeshModifier(+Volume) / NavMeshLink / NavMeshAgent / NavMeshObstacle
        │ NavSyncSystem: coleta de geometria para bake, agentes, obstáculos, links
        ▼
nav::World (servidor: malhas por tipo de agente, crowd, obstáculos, consultas)
        ▼
RecastBackend: rcConfig/rcBuild*, dtNavMesh em tiles, dtTileCache, dtCrowd, dtNavMeshQuery
```

## 2. Configurações do projeto

| Item | Conteúdo |
|---|---|
| Tipos de agente | Nome, raio, altura, altura de degrau, inclinação máxima (cada tipo tem sua própria NavMesh) |
| Áreas | Até 32 áreas nomeadas com custo (o Detour aceita 64; limitamos ao mesmo número da Unity) |
| Crowd | Máximo de agentes por mundo (por tier), parâmetros de desvio por qualidade |

## 3. Componentes

### 3.1 NavMeshSurface

Tipo de agente; coleta (`All`, `Volume`, `Children`); layers incluídas; geometria (`RenderMeshes` ou `PhysicsColliders`); área padrão; overrides de tamanho de voxel e de tile; área mínima de região; gerar *height mesh*. Ação **Bake** (job em segundo plano por tiles, com progresso e cancelamento) e **Clear**.

Mapeamento para `rcConfig`: `cs`/`ch` (voxel), `walkableSlopeAngle`, `walkableHeight`, `walkableClimb`, `walkableRadius` (derivados do tipo de agente), `maxEdgeLen`, `maxSimplificationError`, `minRegionArea`, `mergeRegionArea`, `maxVertsPerPoly`, `detailSampleDist`, `detailSampleMaxError`.

### 3.2 NavMeshModifier e NavMeshModifierVolume

Sobrescrever área, ignorar no bake, aplicar aos filhos, filtrar por tipo de agente; o volume aplica numa caixa.

### 3.3 NavMeshLink

Pontos inicial/final, largura, bidirecional, área, modificador de custo, atualização automática ao mover. Implementado como *off-mesh connection* do Detour (por tile, no tile cache).

### 3.4 NavMeshAgent

| Propriedade | Consumidor |
|---|---|
| Tipo de agente, offset base, raio, altura | `dtCrowdAgentParams` |
| `speed`, `angularSpeed`, `acceleration`, `stoppingDistance`, `autoBraking` | Crowd + integração Astra (rotação) |
| `obstacleAvoidanceType` (qualidade), `avoidancePriority` | Parâmetros de desvio do crowd |
| `autoTraverseOffMeshLink`, `autoRepath`, `areaMask` | Filtro de consulta e máquina de links |
| `updatePosition`, `updateRotation` | Permite root motion: o agente sugere, o Animator move |

API: `SetDestination`, `CalculatePath`, `SetPath`, `ResetPath`, `Warp`, `Move`, `isStopped`, `remainingDistance`, `pathStatus`, `velocity`, `desiredVelocity`, `nextPosition`, `isOnOffMeshLink`, `CompleteOffMeshLink`.

### 3.5 NavMeshObstacle

Forma (caixa/cápsula), centro, tamanho; `carve` (recorta a malha via tile cache) com `moveThreshold`, `timeToStationary`, `carveOnlyStationary`. Sem carve, só participa do desvio do crowd.

## 4. Consultas estáticas (`NavMesh.*`)

| Consulta | Detour |
|---|---|
| `CalculatePath` | `findPath` + `findStraightPath` |
| `Raycast` | `dtNavMeshQuery::raycast` |
| `SamplePosition` | `findNearestPoly` com raio |
| `FindClosestEdge` | `findDistanceToWall` |
| Custos por área | `dtQueryFilter` |

`dtNavMeshQuery` não é thread-safe: uma instância por thread que consulta.

## 5. Regras de integração

- O agente move o `Transform` na fase `Navigation`. Agente com Rigidbody deve ser cinemático; caso contrário, aviso (mesma recomendação da Unity).
- Com root motion: `updatePosition = false`, o script ou o Animator usam `desiredVelocity`, e `nextPosition` é sincronizado.
- Geometria alterada (malha reimportada, colisor editado) marca a NavMesh como **desatualizada** no editor, com aviso visível. Nunca há rebake silencioso.
- Rebake de tiles em runtime para regiões alteradas: depois da F10, com orçamento de tempo por frame.

## 6. Editor

- Overlay da NavMesh com polígonos coloridos por área; filtro por tipo de agente.
- Gizmos: cilindro do agente, caminho atual, destino, alças de links, volume do modificador.
- Painel de bake no Inspector da NavMeshSurface (parâmetros, botão, progresso, estatísticas de tiles/polígonos/memória).
- Depuração em Play: vizinhos do crowd, corredor, velocidade desejada × real.

## 7. Limites conhecidos

| Limite | Consequência |
|---|---|
| `dtCrowd::update` é single-thread | Um crowd por mundo, rodando num job; máximo de agentes por tier medido |
| Bake no aparelho é caro em cenas grandes | Bake por tiles, em segundo plano, cancelável e com tamanho de tile ajustável |
| Último release tagueado é de 2023 | Fixar commit do `main` testado; patches próprios documentados |

## 8. Inventário

| Componente | Unity (AI Navigation 2.0) | Godot 4.7 | Fase | Classificação planejada |
|---|---|---|---|---|
| NavMeshSurface | ✓ | NavigationRegion3D | F10 | Equivalente |
| NavMeshModifier/Volume | ✓ | (grupos de fonte) | F10 | Equivalente |
| NavMeshLink | ✓ | NavigationLink3D | F10 | Equivalente |
| NavMeshAgent | ✓ | NavigationAgent3D | F10 | Equivalente |
| NavMeshObstacle | ✓ | NavigationObstacle3D | F10 | Equivalente |

## 9. Aceite (F10)

- Cena com dois andares ligados por escada e por link de salto: agente encontra o caminho e atravessa o link.
- Obstáculo com carve (porta fechada) bloqueia o caminho; aberta, o caminho volta.
- 100 agentes em T1 com desvio, tempo medido no Profiler.
- NavMesh desatualizada após mover uma parede aparece no editor; rebake corrige.
