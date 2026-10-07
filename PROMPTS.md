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
