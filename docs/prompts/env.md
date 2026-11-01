# Ambiente de build (máquina Windows 11, sem admin)

Toolchain instalado nesta sessão (não está no PATH padrão). Em TODO comando de bash — inclusive para RODAR main.exe (sem isso ele crasha por DLL de runtime errada vinda do Git Bash) — faça antes:

    export PATH="$LOCALAPPDATA/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin:$PATH"

Isso fornece g++ 16.2 (MinGW-w64 UCRT), cmake, ninja e mingw32-make.

- Configurar (uma vez; o diretório build/ já existe configurado com Ninja):
    cmake -G Ninja -B build -S .
- Compilar tudo:            cmake --build build
- Compilar só os testes:    cmake --build build --target logic_tests
- Executáveis vão para bin/Debug/ (main.exe, logic_tests.exe).
- Rodar testes unitários:   ./bin/Debug/logic_tests.exe
- O jogo precisa rodar com cwd = bin/Debug (caminhos ../../data). Há GPU e desktop; a janela abre normalmente.
- Para rodar o jogo sem travar o shell, use timeout: `cd bin/Debug && timeout 20 ./main.exe --autotest <cenario>`
  (o harness E2E existe a partir da Tarefa 0.3; antes disso, `timeout 4 ./main.exe` e checar que não crashou: exit 124 = ainda vivo = ok).
- Screenshots PNG gerados pelo E2E podem ser inspecionados com a ferramenta Read (ela mostra imagens).
- Não há Makefile.macOS testável aqui (sem macOS): só mantenha a linha do g++ coerente quando adicionar .cpp.
- git: commitar direto na branch main. Mensagem começando com `IA: `, corpo citando `Tarefa X.Y`, e terminando com a linha
    Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  NÃO dar push. NÃO editar SPEC.md nem README.md. NÃO usar --no-verify.
- PROMPTS.md: NÃO precisa nova entrada por tarefa (a entrada "Execução do plano por subagentes" cobre todos os commits de tarefa).
- Ao concluir a tarefa, marque os checkboxes `- [ ]` → `- [x]` da SUA tarefa em docs/PLANO-IMPLEMENTACAO.md no mesmo commit, e
  atualize a tabela "Requisitos técnicos e estado atual" do CLAUDE.md se o estado de algum requisito mudou.
- Windows App Control às vezes bloqueia um .exe recém-linkado ("Permission denied" ao rodar). Não é bug: toque um .cpp, recompile e rode de novo.
- E2E: `bash tests/run_e2e.sh` (da raiz). Para adicionar cenário: editar só `BuildScenarios()` em src/autotest.cpp (ver comentário no topo). `main.exe --autotest-list` lista cenários. Screenshots em bin/Debug/e2e/*.png — ABRA-OS com Read para conferir visualmente.
- Desde a Tarefa 0.3 o runtime MinGW é linkado estaticamente: main.exe roda mesmo sem o PATH do WinLibs (mas compile sempre com ele).
