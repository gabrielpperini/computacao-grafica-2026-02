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
confere o diff. O "prompt" de cada commit é, portanto, este prompt acima mais o texto da tarefa
correspondente no plano (citada no corpo do commit como `Tarefa X.Y`).

**Primeiro ajuste (este commit):** plano ganhou a Tarefa 0.3 (harness E2E com `--autotest`) e a
regra de obtenção de assets CC0/procedurais na Tarefa 3.2.
