# 23 — Riscos, limites e plano B

## 1. Registro de riscos

Probabilidade (P) e impacto (I): A = alto, M = médio, B = baixo.

| ID | Risco | P | I | Mitigação | Gatilho de ação |
|---|---|---|---|---|---|
| **R-01** | A linha pública mobile do The Forge acabou na **1.63**; a 1.64 pública é só PC/DX12. Não haverá correções upstream para Android/Vulkan | A (fato) | A | Fork próprio com patches documentados; camadas isolam o backend; plano B (§3) | Bug de driver/plataforma sem correção viável em 5 dias |
| **R-02** | Portar o build do TF (VS2019 + AGDE + NDK r21e) para CMake + NDK r28+ revela muito código incompatível | M | A | S-01 com prazo; patches mínimos; avisos de terceiros silenciados | S-01 estourar o prazo |
| **R-03** | Estado global e `IApp` do TF conflitam com GameActivity e loop próprio | M | M | S-02; usar só o renderer + OS utilitário | S-02 reprovado |
| **R-04** | RmlUi lenta para o editor (Inspector, árvore, texto) | M | A | Elementos virtuais em C++, medição no S-07, orçamento explícito | Orçamento do S-07 não atingido |
| **R-05** | Editor de código próprio é um projeto grande (seleção, IME, desfazer, desempenho) | A | M | Escopo da IDE da F6 limitado ao essencial; VS Code + luau-lsp no PC como alternativa forte | IDE atrasando a F6 |
| **R-06** | Luau frustra a expectativa de quem quer C#; bindings ou depurador insuficientes | M | M | API com nomes Unity; tipos gerados; S-08; reavaliação de C# prevista em D-12 | Reprovação no S-08 ou demanda comprovada |
| **R-07** | Escopo "paridade Unity" explode | A | A | Famílias com inventário e classificação; marcos M1–M5; nada no menu antes de implementado | Fase com mais de 2× o tamanho previsto |
| **R-08** | Memória no celular (editor + mundo de Play + assets) | M | A | Tags e orçamentos; Play clonado com custo medido; opção de suspender o render de edição; `onTrimMemory` | RSS acima do orçamento do tier |
| **R-09** | Calor e bateria no uso prolongado do editor | M | A | Render sob demanda; ADPF; modo Economia; tiers | *Thermal headroom* crítico em 30 min de edição em T2 |
| **R-10** | Fragmentação de GPU/driver (Adreno, Mali, Xclipse, PowerVR) | A | A | Capabilities e caminhos alternativos testados; laboratório de aparelhos (L-12); validação Vulkan no debug | Erro em um fornecedor sem contorno |
| **R-11** | Licenças: NOTICE da Apache, atribuição de código portado (Godot, Wicked), fontes OFL | B | A | `tools/licenses` no CI; cabeçalho de origem em código portado; tela de licenças | Pasta de terceiros sem licença registrada |
| **R-12** | Perda de projetos do usuário (desinstalar apaga `Android/data`) | M | A | Export/backup em zip, pasta de backup vinculada, aviso no hub, lixeira, WAL | Projeto sem backup há mais de N dias |
| **R-13** | Políticas do Google Play: `targetSdk 36` obrigatório desde 31/08/2026, 16 KB desde 01/11/2025, permissões restritas | A (fato) | M | NDK r28+, AGP 8.5.1+, `targetSdk 36` desde o F0; sem `MANAGE_EXTERNAL_STORAGE` nem `REQUEST_INSTALL_PACKAGES` | Nova política anual (revisar todo ano) |
| **R-14** | Ferramentas externas de design dependem de contas (Figma, geração de imagem) | M | B | Fonte de tokens em JSON no repositório; Astra UI Preview como plano B de frames | Conta indisponível |
| **R-15** | **Disco cheio** na máquina de desenvolvimento (aconteceu durante a escrita deste plano: C: com 0 bytes livres) | A (fato) | M | Pastas de build fora do repositório versionado; limpeza de caches (`.cxx`, Gradle) documentada; checagem de espaço nos scripts de build | Espaço livre < 20 GB antes de builds grandes |
| **R-16** | Desenvolvimento por uma pessoa com agentes: perda de contexto entre sessões | A | M | ADRs, `ESTADO.md` único, AGENTS.md com mapa atualizado, commits pequenos | Decisão reaberta sem evidência nova |
| **R-17** | APIs do flecs mudam entre versões 4.x | M | M | Versão fixada; flecs só em `engine/world`; atualização por tarefa explícita | Atualização necessária por bug |
| **R-18** | Engasgos de compilação de pipeline (PSO) no Android | A | M | Cache de pipeline persistido, coleta de variantes, aquecimento na carga | Engasgo > 50 ms em cena de teste |
| **R-19** | Montar APK no aparelho (AXML/ARSC, assinatura) é frágil | M | M | S-11 cedo; plano B só pelo PC | S-11 reprovado |

## 2. Limites conhecidos (aceitos e comunicados)

| ID | Limite | Consequência para o usuário |
|---|---|---|
| L-01 | O TF não oferece cena, streaming, física, áudio nem rede | Tudo isso é implementado pela Astra 2 (por isso o plano tem famílias próprias) |
| L-02 | O toolchain FSL é Python e não roda no aparelho | Shaders internos são compilados no build; shaders do usuário seguem o caminho glslang |
| L-03 | RCSS sem CSS Grid; seletores e `calc()` a confirmar | Layouts com flexbox; protótipos limitados ao subconjunto |
| L-04 | Sem leitor de tela no editor nem nos jogos na 2.0 | Registrado como pendência de acessibilidade |
| L-05 | Se o S-04 falhar, o Shader Graph só compila no host | Shaders customizados só a partir do PC |
| L-06 | Jolt tem um único coeficiente de atrito | Atrito estático é aproximado (documentado) |
| L-07 | Determinismo entre plataformas desligado | Replays e rede determinística exigem reavaliação |
| L-08 | miniaudio sem reverb e ducking prontos | Reverb próprio na F9; ducking pendente |
| L-09 | `dtCrowd::update` é single-thread | Limite de agentes por tier |
| L-10 | Projetos abertos direto de pastas SAF não são suportados | Projetos vivem no app; import/export por zip |
| L-11 | Instalar o APK exportado é manual (sem `REQUEST_INSTALL_PACKAGES`) | Um passo a mais para testar no próprio aparelho |
| L-12 | Hoje só há um aparelho de teste (T3, Adreno) | Fechar F7 exige Mali e T0; até lá, afirmações de desempenho valem só para o aparelho testado |
| L-13 | iOS fora da 2.0 (Metal congelado na 1.63) | — |
| L-14 | Lightmaps fora da 2.0 | Iluminação indireta por probes/IBL (e DDGI em T3) |
| L-15 | Codegen nativo do Luau proibido no iOS | Só interpretador lá |
| L-16 | Editor em processo único: crash nativo derruba o editor | WAL + sessão garantem retomada sem perda |

## 3. Plano B gráfico (se S-01/S-02/S-03/S-04 reprovarem)

| Opção | Licença | A favor | Contra |
|---|---|---|---|
| **Diligent Engine** | Apache-2.0 | CMake nativo; Android Vulkan **e** GLES; glslang em runtime (HLSL/GLSL → SPIR-V); loader glTF e PBR de referência; desenvolvimento ativo | Abstração mais pesada; outro modelo de recursos para aprender |
| **RHI Vulkan próprio** (conhecimento da Astra 1) | — | Controle total; o time já conhece Vulkan no Android (bindless com fallback, budget de memória, validação) | Mais trabalho: memória, sincronização, pipelines, ferramentas |
| bgfx | BSD-2 | Maduro, multiplataforma | Shaders offline (shaderc), recursos modernos limitados para o laboratório gráfico |

**Ordem de preferência:** Diligent → RHI próprio. A troca atinge só `backends/forge` (vira `backends/diligent` ou `backends/vk`), o toolchain de shaders e o S-05. Servidores, componentes, editor, formatos e scripts não mudam. É o motivo da regra D-04.

## 4. Planos B das demais bibliotecas

Ver [03](03-PILHA-E-BIBLIOTECAS.md) §6.

## 5. Revisão

Este registro é revisado ao fim de cada fase e sempre que um spike reprova. Risco materializado vira tarefa com dono ou limite aceito pelo usuário. Nunca fica apenas como texto.
