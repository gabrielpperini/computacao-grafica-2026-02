# Decisões do coordenador ("Rulings")

Linhas copiadas **sem alteração** do registro do coordenador
(`.superpowers/sdd/PLANO-IMPLEMENTACAO/progress.md`, que não é versionado), na ordem em que
foram tomadas. Cada decisão foi repassada ao subagente implementador (ou revisor) no despacho
da tarefa indicada e, por isso, faz parte do prompt dos commits daquela tarefa (veja
`PROMPTS.md`, entrada "Execução do plano por subagentes").

- `Ruling Rn:` decisão global tomada antes de começar (varredura prévia do plano), válida para
  todas as tarefas a partir da indicada.
- `Task X.Y: Ruling:` decisão tomada durante a tarefa X.Y (normalmente sobre um achado da revisão).
- Sem prefixo de tarefa e depois de "TODAS AS TAREFAS COMPLETAS": decisões da revisão final
  (onda de correção final, `final-fixlist.md`).

- Ruling R1: IA obtém assets CC0 com link direto ou gera malhas procedurais por script versionado — usuário pediu execução sem parar; plano dizia "tarefa humana" — se errado: dupla troca assets depois (custo baixo).
- Ruling R2: sem entrada nova em PROMPTS.md por tarefa; uma entrada única ("Execução do plano por subagentes") cobre os commits e cada commit cita "Tarefa X.Y" — evita 20 entradas redundantes; se professor quiser por commit, custo: adicionar entradas retroativas.
- Ruling R3: remover X/Y/Z na 0.1 conforme plano — controle do usuário volta na 1.2 (antes da avaliação de 04/11) — se errado: nenhum, demo é após 1.3.
- Ruling R4: implementador da 0.2 baixa 1 textura CC0 do Poly Haven (link direto dl.polyhaven.org) e registra em data/FONTES.txt; fallback: textura procedural no shader — custo se errado: troca de imagem.
- Ruling R5: Game ganha `bool spawn_enabled` (default true) na 2.2; testes de física do jogador (1.2) desligam spawn a partir da 2.2 — implementador da 2.2 ajusta os testes antigos — custo: nenhum.
- Ruling R6: construtor/Reset do Game deixa mode=PLAYING; o main.cpp é quem coloca MENU na inicialização (6.2) — mantém testes unitários simples — custo baixo.
- Ruling R7: autotest usa seed fixa (12345) e RNG próprio do Game (std::mt19937, nunca rand()); cenários que testam controle podem desligar spawn — custo: nenhum.
- Ruling R8: linkar libgcc/libstdc++ estaticamente no MinGW (CMakeLists) — main.exe crashava sem o PATH do WinLibs (DLL errada do Git Bash); risco real na demo — custo se errado: binário maior
- Task 0.2: Ruling: revisor marcou ❌ por usar glTexParameteri em vez de sampler object — é a leitura correta (sampler por unidade seria compartilhado) — custo se errado: nenhum
- Task 1.1: Ruling: revisor apontou PROMPTS.md ausente no commit — já decidido em R2 (entrada única cobre commits de tarefa; corpo cita Tarefa X.Y) — custo se errado: entradas retroativas
- Task 1.3: Ruling: callbacks de mouse removidos — mouse volta na 5.1 (câmera livre) e 6.2 (GUI); avaliação parcial pede "alguns requisitos" — custo se errado: professor nota ausência de mouse em 04/11
- Task 3.1: Ruling: PROMPTS.md por commit — R2 vale
- Task 3.2: minor (deferred→7.1): assets sem uso (bunny/sphere/plane/red_brick ~3.3 MB) — Ruling: remover em 7.1 se continuarem sem uso (brick pode servir ao túnel 4.2)
- Task 3.2: Ruling: commit de assets com prefixo IA: (IA baixou/gerou) — plano dizia "humano" — custo se errado: nenhum (é mais conservador)
- Task 4.1: Ruling: trem que cairia em curva vira SIGN na mesma faixa — preserva garantia e RNG — custo se errado: mais placas em curvas
- Ruling R9: túnel com altura interna ≥ 5.5 m; trens permitidos dentro; rampas (4.4) não podem terminar a < 20 m da entrada de um túnel (jogador sobre trem não entra no túnel) — custo se errado: menos rampas perto de túneis
- Task 4.3: Ruling: trem móvel para no início da sequência contínua de retas (reta/túnel/reta), não do segmento — intenção é nunca entrar em curva — custo se errado: nenhum visível
- Task 4.4: Ruling: desvio (a) altura de suporte unificada aceito; (b) trens 7% mais altos aceito; (c) rampa no seam aceito
- Task 4.4: Ruling: entrada lateral na rampa acima de 0.4 m de desnível = parede lateral (troca de faixa revertida, sem morte) — gênero faz assim, evita teleporte visível — custo se errado: jogador não sobe pela lateral
- Task 5.2: Ruling: PROMPTS.md — R2 vale
- Task 5.2: Ruling: tênis permite pular placas de 3 m durante 10 s — consequência aceita do ×1.5; citar no manual
- Task 5.3: Ruling: pontos de controle do plano (altos/longe; porco fora do quadro) substituídos por P0=(-5,5,12) P1=(3,8,20) P2=(-3,8,28) P3=(5,5,36) (S lateral, dentro do quadro) — requisito avaliado é movimento visível — custo se errado: só ajustar constantes
- Task 5.3: Ruling: cofrinho não nasce se houver túnel em [s, s+160] (voo inteiro fora de túnel; porco/moedas não atravessam abóbada) — custo se errado: menos cofrinhos perto de túneis
- Ruling: Tarefa 7.1 dividida em 7.1a (lógica/colisão/testes) e 7.1b (visual) por tamanho — listas em task-7.1a-fixlist.md / task-7.1b-fixlist.md
- Ruling: parqueados (aceitos, sem correção): ímã atravessa trens; moeda coberta por trem móvel; faíscas sem dimming no túnel; seam mais difícil c/ rampas; lookup string por instância; AutotestStep sem mods; UV_BBOX_XY NaN em bbox plana — custo se errado: pequenos artefatos visuais raros
- Task 7.1a: Ruling: PROMPTS.md — R2 vale; reforço: 7.2 adiciona ao PROMPTS.md um mapa Tarefa X.Y → commits (o texto de cada tarefa está no plano versionado) — custo se errado: professor pedir prompt por commit
- Task 7.1b: Ruling: rastreabilidade de prompt — fixlists 7.1a/7.1b copiadas para o plano (7.2 integra) + mapa Tarefa→commits no PROMPTS.md — custo: nenhum
- Ruling: onda final de correção única com final-fixlist.md (itens 1–10)
- Ruling: partículas sem iluminação parqueadas (emissivas; justificar no README) — custo se errado: desconto em "iluminação em todos os objetos"
- Ruling: FONTE em headers do código base do professor parqueado (CLAUDE.md proíbe editar) — custo se errado: nenhum (código base da disciplina)
- Ruling: primeiro power-up de cada corrida é cofrinho (150–300 m) para a Bézier aparecer na demo — custo se errado: menos variedade no início

Linhas da varredura prévia (tabela do registro) que apontam para as decisões acima:

| Par/tarefa | Produz → consome | Achado |
|---|---|---|
| 0.1 | Passo 6 pede entrada nova em PROMPTS.md | conflita com env.md → Ruling R2 |
| 0.1 | remove teclas X/Y/Z | transformações do usuário somem até 1.2/1.3 — aceitável (Ruling R3) |
| 0.2 | exige 3 texturas; repo só tem 2 imagens | Ruling R4 |
| 1.2 → 2.2 | testes longos (Test_SpeedCap 5000 s) quebram quando obstáculos existirem | Ruling R5 |
| 1.2 → 6.2 | modo inicial do Game | Ruling R6 |
| 0.3 → todas | cenários E2E com spawn aleatório | Ruling R7 |
| 3.2 | assets | Ruling R1 (já gravado no plano) |
