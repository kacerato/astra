# 06 — Cena, prefabs e serialização

## 1. Arquivos do projeto

```
MeuJogo/
├── project.astra            descritor: nome, guid, versão do projeto, versão da engine, cena inicial
├── Assets/                  fontes do usuário + .meta ao lado de cada arquivo
├── ProjectSettings/         tags_layers.json, physics.json, quality.json, audio.json, input.json, player.json, build.json
├── Library/                 cache regenerável (cozidos, thumbnails, índice do AssetDatabase) — não versionar
├── .astra/                  sessão do editor (layout, cena aberta, câmera), WAL, lixeira do projeto
└── .gitignore               gerado (ignora Library/ e .astra/)
```

| Extensão | Conteúdo | Formato fonte |
|---|---|---|
| `.ascene` | Cena | Texto (JSON determinístico) |
| `.aprefab` | Prefab ou variante | Texto |
| `.amat` | Material | Texto |
| `.aanim` | Clipe de animação autorado no editor | Texto (curvas) |
| `.acontroller` | Animator Controller | Texto |
| `.ainput` | Input Actions | Texto |
| `.amixer` | Mixer de áudio | Texto |
| `.anav` | Configuração de NavMesh (os dados assados ficam em `Library/` ou como sub-recurso) | Texto |
| `.rml` / `.rcss` | Documento e estilo de UI | Texto (RmlUi) |
| `.luau` | Script | Texto |
| `.meta` | GUID, importador, configurações de importação, versão | Texto |
| Fontes externas (`.glb`, `.fbx`, `.png`, `.wav`…) | Importadas | Binário do usuário |

## 2. Identidade

| Identificador | Tamanho | Gerado quando | Nunca muda em |
|---|---|---|---|
| `AssetGuid` | 128 bits aleatórios | Primeiro import ou criação do asset | Renomear, mover, reimportar |
| `SubId` | 64 bits | Sub-recurso de um import (mesh 3 de um GLB) | Reimport, desde que o nome/caminho interno estável não mude; política de correspondência em [07](07-RECURSOS-E-PIPELINE-DE-ASSETS.md) §5 |
| `LocalId` | 64 bits aleatórios | Criação de objeto num arquivo de cena/prefab | Salvar, renomear, mover na hierarquia |
| `LocalId` de objeto de instância de prefab | Derivado: `mix64(instanceLocalId, sourceLocalId)` | Determinístico, sem precisar gravar | Reabrir a cena |

- Duplicar entidades gera `LocalId` novos e **remapeia referências internas** à seleção duplicada (o comportamento do Ctrl+D da Unity).
- Referências de uma cena para objetos de **outra** cena não são persistidas (mesma regra da Unity). Em runtime, isso é resolvido por busca ou por serviço.

## 3. Formato texto (verdade versionável)

Regras de determinismo:

- JSON estrito (RFC 8259), UTF-8, LF, indentação de 2 espaços, uma propriedade por linha em objetos de componente.
- Números com a representação mais curta que faz *round-trip* (`std::to_chars`); `-0` normalizado; NaN e infinito proibidos (rejeitados na validação da propriedade).
- Ordem estável: entidades em ordem de hierarquia (profundidade primeiro, ordem dos filhos); componentes na ordem do usuário (a ordem do Inspector é semântica); propriedades na ordem do registro.
- Valores iguais ao padrão **são omitidos** (arquivo menor, diffs mais limpos). O padrão faz parte da versão do tipo, por isso mudar um padrão exige migração.
- Sem carimbo de tempo, caminho absoluto ou dado da máquina.

```json
{
  "astra": "scene",
  "format": 1,
  "guid": "6f1c0b0e4a2d4f7b9c1e2d3f4a5b6c7d",
  "settings": { "environment": "asset:3c9e51aa0d6b4c3e8f7a1b2c3d4e5f60" },
  "entities": [
    {
      "id": "8a41f2c09b1e7d33",
      "name": "Porta",
      "tags": ["Interativo"],
      "components": [
        { "type": "astra.Transform", "v": 1, "position": [0, 0, -4] },
        { "type": "astra.MeshRenderer", "v": 1,
          "mesh": "asset:9f2a0c1d2e3f40516273849a0b1c2d3e#1",
          "materials": ["asset:51bd7e8f9a0b4c1d2e3f405162738495"] },
        { "type": "astra.BoxCollider", "v": 1, "size": [1, 2.2, 0.1] },
        { "type": "script:Porta", "v": 1, "instance": 1, "anguloAbertura": 95,
          "alvo": "entity:3b77c1d0e2f4a5b6" }
      ]
    }
  ],
  "prefabInstances": []
}
```

## 4. Formato binário cozido (runtime)

- Cabeçalho: magia `ASTB`, versão do formato, tipo do conteúdo, hash do texto de origem e do registro de tipos usado.
- Tabelas: strings, tipos (TypeId + versão + layout), entidades (LocalId, pai, flags), blocos de componentes **por tipo** (contíguos, prontos para inserção em lote no flecs), referências a assets.
- *Little-endian*, alinhado, pronto para `mmap`; CRC32C por bloco.
- **Derivado e descartável**: sempre reconstruível a partir do texto. O player só lê binário; o editor lê texto e mantém cache binário em `Library/`.

## 5. Dados desconhecidos e referências ausentes

| Situação | Comportamento |
|---|---|
| Componente de tipo desconhecido (script apagado, plugin ausente) | Guardado como `UnknownComponent` com o JSON original; exibido no Inspector como "Script ausente: Porta" com ação "Reatribuir"; regravado sem alteração |
| Propriedade desconhecida num tipo conhecido | Guardada num saco `extra` e regravada; aviso no console |
| `AssetRef` para GUID inexistente | Mantém o GUID; Inspector mostra "Ausente (9f2a…)" em vermelho; o runtime usa o recurso de fallback do tipo (mesh vazia, material de erro magenta) **e registra erro**, nunca silenciosamente |
| `EntityRef` para objeto removido | Mantém o `LocalId`; resolve para "nenhum"; aviso no carregamento |

## 6. Prefabs

Semântica de referência: Unity 6.0 [Prefabs](https://docs.unity3d.com/6000.0/Documentation/Manual/Prefabs.html), Nested Prefabs, Prefab Variants, Instance overrides. Estudo complementar: Godot 4.7 `PackedScene`, cenas herdadas e *editable children*.

### 6.1 Modelo

| Elemento | Armazenado como |
|---|---|
| Asset de prefab | Subárvore de entidades com raiz única (`.aprefab`) |
| Variante | `.aprefab` com `base: <AssetGuid>` + modificações sobre a base |
| Instância numa cena | Registro `PrefabInstance { source, instanceId, parent, modifications[], addedComponents[], removedComponents[], addedChildren[], removedChildren[] }` |
| Modificação | `PropertyPath` (relativo ao prefab) → `Variant` |
| Em memória | Instância **expandida** no World; cada entidade tem `PrefabLink { instance, sourceLocalId }` |

A cena grava **só as diferenças**. Mudanças no prefab propagam para todas as instâncias, exceto nas propriedades sobrescritas.

### 6.2 Operações

| Operação | Escopo | Undo | Observação |
|---|---|---|---|
| Criar prefab a partir de seleção | Entidade e filhos | Transação cena + novo asset | Substitui a seleção por instância |
| Instanciar (arrastar do Assets) | — | Sim | Com pose do ponto de soltura (raycast na cena) |
| Detectar overrides | Propriedade, componente, entidade | — | Compara com o valor resolvido pela cadeia de bases |
| Aplicar | Propriedade / componente / entidade / tudo | Transação **multi-documento** (cena + prefab) | Com prefabs aninhados, escolhe o alvo da cadeia ("Aplicar em 'Inimigo'" ou "em 'InimigoVermelho'") |
| Reverter | Mesmos escopos | Sim | — |
| Desempacotar | Externo ou completo | Sim | Quebra o vínculo e gera `LocalId` próprios |
| Modo prefab | Isolado ou em contexto | Histórico próprio | Mundo de pré-visualização; cena ao fundo esmaecida no modo contexto |
| Comparar com a fonte | Lista de diferenças | — | Herança dos planos `P-OVERRIDES-*` da Astra atual |

### 6.3 Casos de borda (todos com teste)

- Override de propriedade removida da fonte → mantido como "órfão", listado no painel de overrides com ação "Descartar".
- Filho adicionado cujo pai deixou de existir na fonte → reanexado à raiz da instância, com aviso.
- Componente da fonte trocado de tipo → override vira órfão; nada é aplicado silenciosamente a outro tipo.
- Prefab que referencia a si mesmo, direta ou indiretamente → rejeitado na criação e na carga.
- Instância com prefab ausente → expansão substituída por um marcador "Prefab ausente" que preserva modificações e filhos adicionados.

### 6.4 Runtime

`World:Instantiate(prefab)` copia a partir do blob cozido (instâncias independentes, semântica Unity). Os prefabs nativos do flecs (`IsA`, componentes compartilhados) podem servir como **gabarito interno de cópia** se o S-06 mostrar ganho; não mudam a semântica.

## 7. Versionamento e migração

- Cada tipo tem `v` (inteiro) e uma cadeia de migrações `v(n) → v(n+1)` registrada junto do tipo. Funções de migração operam sobre o JSON (ou sobre `Variant`), nunca sobre o objeto vivo.
- Aliases para tipos e propriedades renomeados; o salvamento usa sempre o nome novo.
- Abrir arquivo de versão antiga: migra em memória, marca o documento como alterado e, ao salvar pela primeira vez, guarda cópia `.bak` em `.astra/backups/`.
- Arquivo de versão **mais nova** que a engine: abre só leitura, com aviso.
- Cada versão de formato tem fixture em `tests/fixtures/formats/` e teste de migração.

## 8. Gravação segura

### 8.1 Salvar

`escrever temporário → fsync → rename atômico → fsync do diretório`. Falha em qualquer passo mantém o arquivo antigo intacto e mostra erro com caminho e motivo.

### 8.2 WAL de edição (herança ADR-12)

| Aspecto | Regra |
|---|---|
| Unidade | Uma **transação** de Undo (um ou mais comandos) |
| Momento | Gravada com `fsync` **antes** de ser aplicada ao documento |
| Registro | `u32 tamanho · u32 crc32c · u64 sequência · payload binário` (comandos com `PropertyPath` e `Variant` antes/depois) |
| Recuperação | Ao abrir, se o WAL tem registros posteriores ao último salvamento, o editor oferece "Recuperar N alterações". A leitura para no primeiro registro inválido |
| Truncamento | Após salvar com sucesso |
| Custo | Transações de arrasto contínuo (mover gizmo) são agrupadas: só o estado final de cada gesto vai ao WAL |
| Escopo | Um WAL por documento aberto (cena, prefab em edição, settings) |

### 8.3 Lixeira do projeto

Excluir asset no editor move para `.astra/trash/` com o `.meta`, o que permite restaurar com GUID intacto. Esvaziar a lixeira é ação explícita do usuário.

## 9. Cenas em runtime

| Capacidade | Referência | Fase |
|---|---|---|
| Carregar cena única / aditiva, síncrona e assíncrona com progresso | Unity `SceneManager.LoadSceneAsync` | F3 |
| Descarregar cena | `SceneManager.UnloadSceneAsync` | F3 |
| Cena ativa (onde `Instantiate` coloca objetos por padrão) | `SceneManager.SetActiveScene` | F3 |
| Objetos persistentes entre cenas | `DontDestroyOnLoad` | F3 |
| Configurações por cena (ambiente, névoa, céu) | Lighting settings por cena | F7 |
| Streaming por regiões (subcenas) | — | Após F13 |

## 10. Aceite

- Round-trip texto ↔ mundo ↔ texto idêntico byte a byte; binário reconstruído é idêntico entre execuções.
- Matar o processo (`adb shell am kill` / `kill -9`) no meio de edições → reabrir recupera todas as transações gravadas.
- Prefab: aplicar e reverter em cada escopo, com prefabs aninhados de 3 níveis e uma variante; propagação para duas cenas abertas; cada caso de borda de §6.3.
- Migração: fixture v1 abre, migra e salva na versão atual sem perda (diff semântico vazio).
- Dados desconhecidos sobrevivem a abrir → salvar sem alteração.
