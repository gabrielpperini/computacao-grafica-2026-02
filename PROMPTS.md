# Prompts utilizados

Registro dos prompts usados para gerar código/conteúdo com ferramentas de IA neste trabalho,
conforme exigido pelo enunciado (`ordem-trabalho.md`). Os commits correspondentes têm a mensagem
iniciada por `IA: `.

---

## 2026-10-07 — `CLAUDE.md` (arquitetura do projeto)

**Ferramenta:** Claude Code (Claude Opus 5)

**Prompt:**

> ja cria um claude.md analisando o repositorio

**Gerado:** primeira versão do `CLAUDE.md`, com comandos de build/execução nas três plataformas e
descrição da arquitetura do código base (pipeline de carga de OBJ, contrato C++ ↔ GLSL, câmera e
matrizes, estado global e callbacks de entrada).

---

## 2026-10-07 — `CLAUDE.md` (regras do enunciado)

**Ferramenta:** Claude Code (Claude Opus 5)

**Prompt:**

> avalia agora o ordem-trabalho e define todas as regras necessarias devida a ordem do trabalho
> feita pelo professor

**Gerado:** reescrita do `CLAUDE.md` acrescentando a seção de regras obrigatórias extraídas do
`ordem-trabalho.md` (convenção de commits `IA:`, registro de prompts, arquivos proibidos para IA,
comentários `FONTE`, APIs permitidas, funções de matriz proibidas, `collisions.cpp`, histórico do
Git), a tabela de prazos, o checklist de requisitos técnicos com o estado atual do código base e a
seção de entrega final.

---

## 2026-11-01 — `docs/PLANO-IMPLEMENTACAO.md` (plano de implementação)

**Ferramenta:** Claude Code (Claude Opus 5.5)

**Prompt:**

> le todo o repositorio e vamos comecar a executar, comeca criando um plano de implementacao e
> adicionando ele no repositorio, divide em fases bem definidas, coloca tudo numa pasta docs, faz
> referencia no claude.md

**Gerado:** `docs/PLANO-IMPLEMENTACAO.md` (plano em 8 fases, com arquitetura, interfaces, testes e
cronograma alinhado às avaliações parciais) e a seção "Plano de implementação" do `CLAUDE.md`.

---

## 2026-11-01 — Execução do plano por subagentes

**Ferramenta:** Claude Code (Claude Opus 5.5 como coordenador; subagentes Claude implementam e
revisam cada tarefa)

**Prompt:**

> por subagentes, vou te deixar a noite ligado, nao para por nada, vai comitando tudo na main, sem
> pr, se precisar adiciona testes unitarios e E2E pra garantir eficacia

**Como os commits seguintes foram gerados:** cada commit `IA: ...` a partir daqui implementa uma
tarefa de `docs/PLANO-IMPLEMENTACAO.md`. O coordenador extrai o texto da tarefa do plano e o
entrega a um subagente implementador junto com as restrições globais; um subagente revisor
confere o diff. O "prompt" de cada commit é, portanto:

1. este prompt do usuário (acima);
2. o texto **exato** entregue ao subagente, versionado em [`docs/prompts/`](docs/prompts/):
   `task-X.Y-brief.md` (contexto global do plano + texto da tarefa, gerado por `mkbrief.sh`),
   `env.md` (ambiente de build/teste/commit desta máquina), as listas `task-7.1a-fixlist.md`,
   `task-7.1b-fixlist.md`, `task-7.2-extra.md` e `final-fixlist.md` (+ `final-wave-dispatch.md`),
   e as instruções dos revisores (`reviewer-instructions.md`, `rereview-instructions.md`);
3. as decisões do coordenador repassadas nos despachos (`docs/prompts/rulings.md`);
4. nas rodadas de correção pós-revisão, os achados enviados ao implementador
   (`docs/prompts/rodadas-de-correcao.md`).

Cada commit cita a tarefa no corpo (`Tarefa X.Y`, ou `Revisão final, item N` na onda final).

**Primeiro ajuste (este commit):** plano ganhou a Tarefa 0.3 (harness E2E com `--autotest`) e a
regra de obtenção de assets CC0/procedurais na Tarefa 3.2.

**Tarefa → commits (Tarefa 7.2, rastreabilidade):** todos os commits `IA: ...` das tarefas
executadas até aqui, com o hash curto. **Se o histórico for reescrito (rebase, squash, amend),
regenere esta tabela** com `git log --format='%h %s%n%b'` (o corpo de cada commit cita a tarefa). Cada commit cita a tarefa no corpo da mensagem
(`git log --grep "Tarefa 4.4"` lista os de uma tarefa). O prompt de cada um é o prompt desta
entrada mais o texto da tarefa em `docs/PLANO-IMPLEMENTACAO.md`; as Tarefas 7.1a e 7.1b têm a
lista de correções entregue ao implementador copiada na íntegra no próprio plano (subseções da
Tarefa 7.1).

| Tarefa | Commits (hash curto) |
|---|---|
| Plano (antes das tarefas) | `3c39030`, `9b4128c` |
| 0.1 | `f2df87b` |
| 0.2 | `9cb312b` |
| 0.3 | `e84b581` |
| 1.1 | `1b2bddd` |
| 1.2 | `44dff4b` |
| 1.3 | `16cf775`, `8e56b07` |
| 2.1 | `8ccfca1` |
| 2.2 | `683b2de` |
| 3.1 | `5fe2ec3` |
| 3.2 | `95d62da`, `57ed74b` |
| 4.1 | `d39be0e` |
| 4.2 | `5d181c7` |
| 4.3 | `cf248b6` |
| 4.4 | `009e36f`, `9eebfb6`, `1596faa` |
| 5.1 | `a20ab65`, `1e6efda` |
| 5.2 | `91d5de2` |
| 5.3 | `dc626a0`, `24763d1` |
| 6.1 | `7f491a3` |
| 6.2 | `96e110c`, `f4b6158` |
| 7.1a | `b62aeb7`, `041df36`, `8c5d68c`, `99504ef`, `7548316`, `a917cea`, `ad46e51`, `f97c1b6`, `c03ed66`, `e139af5` |
| 7.1b | `7321849`, `046ce1e`, `cc37c90`, `c4da3e6`, `8d6cc85`, `7576b9f`, `af91574`, `47fdc05`, `3e6c56b`, `1cb2e11`, `dbbba11`, `31e4706`, `582bfc4` |
| 7.2 | `c3d7efb` e os demais commits `Tarefa 7.2` (`git log --grep "Tarefa 7.2"`) |
| Revisão final (onda de correção, `docs/prompts/final-fixlist.md`) | `ccbfb4d`, `9ccc85e`, `1b128a5`, `42861b8`, `f326a9b`, `f0ada16`, `7faa9b6`, `2bda2c9`, `a44a4eb`, `92c6297` e os seguintes com `Revisão final` no corpo (`git log --grep "Revisão final"`: docs/prompts, CLAUDE.md) |

---

## 2026-11-04 — Build e execução no macOS

**Ferramenta:** Claude Code (Claude Opus 5.5)

**Prompt:**

> le todo o repo, valida oq foi feito, baixa todas ferramentas necessarias pra gente rodar e me da o
> comando pra rodar o jogo

**Gerado:** primeira compilação real no macOS (Apple Silicon). Correções: caminho da GLFW do
Homebrew no `Makefile.macOS` (`/opt/homebrew/lib`); viewport inicial pelo tamanho do framebuffer
(`glfwGetFramebufferSize`, em `main.cpp`), que no Retina é o dobro da janela; `CLAUDE.md` e
`COMPILACAO.md` passam a registrar o macOS como testado.

---

## 2026-11-04 — Tela "Controles" separada no menu

**Ferramenta:** Claude Code (Claude Opus 5.5)

**Prompt:**

> vamos fazer um ajuste no menu, nao quero que seja um accordion a parte dos controles, toda hora eu
> clico sair achando que vou sair dos controles e saio do jogo, ajusta isso pra mim

**Gerado:** "Controles" troca o menu por uma tela própria (`Gui_MenuControls()` em `main.cpp`) com
a lista e só o botão "Voltar" (`Esc` também volta); cenários E2E `menu_pause` e `gui_small_window`
ajustados; `CLAUDE.md` atualizado.
