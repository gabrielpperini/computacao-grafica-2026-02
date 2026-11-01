# Rodadas de correção pós-revisão

Depois de cada tarefa um subagente revisor conferiu o diff. Quando ele achou problemas que não
podiam ficar para depois, o coordenador mandou ao implementador uma rodada de correção com os
achados (o revisor da rodada recebeu `rereview-instructions.md` com esses mesmos achados no lugar
de `[FINDINGS]`). Esta página reúne, por commit de correção, o texto dos achados enviados.

**Fontes (copiadas sem alteração, entre aspas):** o registro do coordenador
(`.superpowers/sdd/PLANO-IMPLEMENTACAO/progress.md`, não versionado; as decisões estão também em
`rulings.md`) e as seções de correção dos relatórios dos implementadores
(`task-X.Y-report.md`, que citam o achado que corrigiram). O despacho de cada rodada era: o brief
da tarefa (`task-X.Y-brief.md`, nesta pasta) + "corrija os achados abaixo" + os achados. Achados
"minor" que não pediram correção imediata ficaram registrados como "deferred" e foram tratados nas
listas das Tarefas 7.1a/7.1b (`task-7.1a-fixlist.md`, `task-7.1b-fixlist.md`) ou na onda final
(`final-fixlist.md`).

---

## Tarefa 4.4 — rodada 1: `009e36f` → `9eebfb6`

Commit: `9eebfb6 IA: lateral da rampa vira parede para quem entra de lado`.

Achados (registro do coordenador):

> Task 4.4: Ruling: entrada lateral na rampa acima de 0.4 m de desnível = parede lateral (troca de
> faixa revertida, sem morte) — gênero faz assim, evita teleporte visível — custo se errado:
> jogador não sobe pela lateral
>
> Task 4.4: fix round 1/5 dispatched (findings: side-entry snap; stale comments; frontal hit tests
> at max speed/dt)

O relatório da correção (`task-4.4-report.md`, seção "Correção da revisão (Important: entrada
lateral na rampa)") descreve o achado principal como o comportamento anterior, registrado no
próprio relatório da tarefa:

> **Entrada lateral na rampa pelo chão** (ex.: cruzando a faixa dela na emenda) põe o jogador
> na superfície (pode "teleportar" até ~3 m para cima, como a ruling pede: rampa nunca mata).

Resultado registrado:

> Task 4.4: fix round 1/5 (2 addressed, 1 open — junção rampa/trem: footprint varrido teleporta
> 0→3.2 quando o passo cruza o fim da rampa; commits 009e36f..9eebfb6)

## Tarefa 4.4 — rodada 2: `9eebfb6` → `1596faa`

Commit: `1596faa IA: bloqueio lateral da rampa com a pegada varrida no passo`.

Achado (o que ficou aberto na rodada 1, registro do coordenador):

> junção rampa/trem: footprint varrido teleporta 0→3.2 quando o passo cruza o fim da rampa

Como o relatório da correção o confirma (`task-4.4-report.md`, seção "Correção da revisão,
rodada 2 (emenda rampa/trem)", "Problema (confirmado)"):

> `BlockRampSides` testava a rampa só com a pegada do FIM do passo, mas `SupportHeight` mede rampas
> com a pegada VARRIDA (antes ∪ agora). Entrada lateral em que a traseira da pegada tocava a rampa
> no começo do passo (≤ 33.5) e já tinha passado do fim dela depois do passo: sem bloqueio, e o apoio
> achava a rampa (3.2 m) pela pegada varrida → 0 → 3.2 m num quadro, e a isenção "em pé na altura do
> teto" pulava o teste de morte. O teste anterior excluía justamente essa janela.

Resultado registrado:

> Task 4.4: fix round 2/5 (1 addressed, 0 open; commits 9eebfb6..1596faa)

## Tarefa 5.1 — rodada 1: `a20ab65` → `1e6efda`

Commit: `1e6efda IA: ponto-no-túnel da câmera livre em collisions.cpp com o volume correto`.

Achados (registro do coordenador):

> Task 5.1: fix round 1/5 dispatched (PointInsideTunnel volume errado + mover p/ collisions.cpp com
> testes; minors raw motion reset, check do mouse)

Detalhados no relatório da correção (`task-5.1-report.md`, "Correção da revisão (rodada 1)"):
`PointInsideTunnel` amostrava a pista a cada 2 m e testava contra a fachada de 9 m (erro de até
1 m), estava em `main.cpp` (teste de intersecção fora de `collisions.cpp`), o movimento bruto do
mouse não era desligado ao sair da câmera livre e o E2E `free_camera` não conferia que o mouse
girou o olhar.

Resultado registrado:

> Task 5.1: fix round 1/5 (4 addressed, 0 open; commits a20ab65..1e6efda)

## Tarefa 5.3 — rodada 1: `dc626a0` → `24763d1`

Commit: `24763d1 IA: ajusta voo do porco ao quadro da câmera e evita túneis`.

Achados (registro do coordenador; as duas decisões foram enviadas como os itens a corrigir):

> Task 5.3: implementer done dc626a0 (base 91d5de2); review dispatched. Porco no topo do quadro
> (pontos de controle do plano altos demais)
>
> Task 5.3: Ruling: pontos de controle do plano (altos/longe; porco fora do quadro) substituídos por
> P0=(-5,5,12) P1=(3,8,20) P2=(-3,8,28) P3=(5,5,36) (S lateral, dentro do quadro) — requisito
> avaliado é movimento visível — custo se errado: só ajustar constantes
>
> Task 5.3: Ruling: cofrinho não nasce se houver túnel em [s, s+160] (voo inteiro fora de túnel;
> porco/moedas não atravessam abóbada) — custo se errado: menos cofrinhos perto de túneis
>
> Task 5.3: fix round 1/5 dispatched

Itens tratados (relatório, "Correcao pos-revisao"): pontos de controle trocados; cofrinho não
nasce com túnel nos 160 m seguintes e moedas do porco nunca caem dentro de túnel; constantes
nomeadas para a raiz das asas (mesmos números de `tools/gen_models.py`).

Resultado registrado:

> Task 5.3: fix round 1/5 (3 addressed, 0 open; commits dc626a0..24763d1)

---

## Revisão final (todas as tarefas) — onda de correção final

Achados da revisão final (registro do coordenador):

> Final review: Needs fixes (code: restart mesma semente; prompts verbatim; pedra/tijolo do SPEC;
> inverse(view) por fragmento; + minors). Críticos fora do código: commits do Bruno; README.
>
> Ruling: onda final de correção única com final-fixlist.md (itens 1–10)

O texto exato entregue ao implementador está em `final-fixlist.md` (a lista de itens) e
`final-wave-dispatch.md` (a mensagem de despacho). Os commits citam o item no corpo
(`Revisão final, item N`); `git log --grep "Revisão final"` lista todos. Itens "parqueados" (sem
correção) estão no fim de `final-fixlist.md` e em `rulings.md`.
