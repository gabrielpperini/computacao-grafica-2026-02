#!/usr/bin/env bash
# uso: mkbrief.sh X.Y  -> escreve task-X.Y-brief.md com contexto global + texto da tarefa
set -euo pipefail
cd "/computacao-grafica-2026-02"
P=docs/PLANO-IMPLEMENTACAO.md; W=.superpowers/sdd/PLANO-IMPLEMENTACAO; n="$1"; out="$W/task-$n-brief.md"
{
  echo "# Brief — Tarefa $n (Rail Rush)"; echo
  echo "Leia também: $W/env.md (como compilar/testar/commitar nesta máquina) e CLAUDE.md (regras do repositório)."; echo
  echo "## Contexto global do plano (vale para todas as tarefas)"; echo
  awk '/^## Restrições globais/{p=1} /^## Cronograma/{p=0} p' "$P"
  echo; echo "## SUA TAREFA"; echo
  awk -v n="$n" '/^```/{f=!f} !f && /^#+ +(Tarefa|Fase) /{p=($0 ~ ("^### Tarefa " n ":"))} p' "$P"
} > "$out"
grep -q "^### Tarefa $n:" "$out" || { echo "tarefa $n não encontrada" >&2; exit 3; }
echo "$out ($(wc -l < "$out") linhas)"
