#!/usr/bin/env bash
# Roda os cenários E2E (main --autotest <cenario>) a partir de bin/<plataforma>/.
# Uso (na raiz do repositório, depois de compilar):  bash tests/run_e2e.sh [cenario...]
# Sem argumentos roda todos os cenários listados por "main --autotest-list".
# No Windows, E2E_BIN_DIR=bin/Release testa o build Release (padrão: bin/Debug).
# Screenshots ficam em bin/<plataforma>/e2e/. Sai com status != 0 se algum falhar.

set -u

cd "$(dirname "$0")/.." || exit 1

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) BIN_DIR="${E2E_BIN_DIR:-bin/Debug}"; EXE="./main.exe" ;;
    Darwin*)              BIN_DIR="bin/macOS";  EXE="./main" ;;
    *)                    BIN_DIR="bin/Linux";  EXE="./main" ;;
esac

if [ ! -x "$BIN_DIR/$EXE" ]; then
    echo "E2E: executavel $BIN_DIR/$EXE nao encontrado (compile antes)." >&2
    exit 1
fi

TIMEOUT_S=60
# macOS não traz o comando "timeout" (só com coreutils): sem ele os cenários rodam
# sem limite de tempo (um cenário travado precisa ser interrompido com Ctrl+C).
if command -v timeout >/dev/null 2>&1; then
    run_with_timeout() { timeout "$TIMEOUT_S" "$@"; }
elif command -v gtimeout >/dev/null 2>&1; then
    run_with_timeout() { gtimeout "$TIMEOUT_S" "$@"; }
else
    echo "E2E: aviso: comando 'timeout' ausente; rodando sem limite de tempo." >&2
    run_with_timeout() { "$@"; }
fi
cd "$BIN_DIR" || exit 1

if [ "$#" -gt 0 ]; then
    SCENARIOS="$*"
else
    SCENARIOS="$($EXE --autotest-list)" || { echo "E2E: falha ao listar cenarios" >&2; exit 1; }
    SCENARIOS="$(printf '%s' "$SCENARIOS" | tr -d '\r')"
    [ -n "$SCENARIOS" ] || { echo "E2E: nenhum cenario listado" >&2; exit 1; }
fi

failed=0
total=0
for name in $SCENARIOS; do
    total=$((total + 1))
    echo "--- cenario: $name"
    out="$(run_with_timeout $EXE --autotest "$name" 2>&1)"
    code=$?
    printf '%s\n' "$out" | tr -d '\r' | grep -E '^E2E|ERROR' 
    if [ "$code" -eq 0 ] && printf '%s' "$out" | grep -q "^E2E $name OK"; then
        echo "PASS $name"
    else
        echo "FAIL $name (codigo de saida $code)"
        [ "$code" -eq 124 ] && echo "     (timeout de ${TIMEOUT_S}s)"
        failed=$((failed + 1))
    fi
done

echo "E2E: $((total - failed))/$total cenarios passaram"
[ "$failed" -eq 0 ] || exit 1
