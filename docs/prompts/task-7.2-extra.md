# Tarefa 7.2 — itens adicionais decididos pelo coordenador

A. Build Linux real via Docker (Docker Desktop está rodando nesta máquina; `docker` no PATH):
   rode um contêiner ubuntu:24.04 com o repositório montado (ex.: `docker run --rm -v "C:/Users/gabri/Desktop/Faculdade/computacao-grafica-2026-02:/src" -w /src ubuntu:24.04 bash -c "..."`),
   instale cmake g++ make e as dependências de X11/GL que o link estático de lib-linux/libglfw3.a exige
   (libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev, etc.), configure num diretório de build
   DENTRO do contêiner (ex.: /tmp/build) para não sujar build/ do Windows, compile `main` e `logic_tests`, rode logic_tests.
   O binário Linux sai em bin/Linux (ignorado pelo git). Corrija o que quebrar (CMakeLists, includes, warnings -Wall).
   Também tente o `make` (atalho do Makefile raiz com presets) se fizer sentido. Anote em COMPILACAO.md os pacotes apt necessários.
B. macOS: não há macOS. Valide ao menos que a lista de fontes/flags de Makefile.macOS bate com o CMakeLists (todos os .cpp de SOURCES +
   ImGui, -I third_party/imgui e backends), e que o comando compila no Linux do contêiner trocando só as flags específicas de mac
   (frameworks) por equivalentes Linux — numa cópia temporária, não no arquivo. Documente que o macOS não foi testado.
C. Windows: documente em COMPILACAO.md o caminho que funcionou aqui (WinLibs MinGW UCRT via winget + CMake/Ninja via pip ou o
   CMake/VSCode já descritos), incluindo `cmake -DCMAKE_BUILD_TYPE=Release` recomendado para a apresentação e onde fica o .exe
   (bin/Debug ou bin/Release) e que precisa rodar com cwd = bin/<config>. Não remova as instruções existentes; atualize-as.
D. Testes: documente em COMPILACAO.md (ou seção nova) como rodar logic_tests e `bash tests/run_e2e.sh` (Git Bash no Windows),
   tempo aproximado, e o que são os cenários. run_e2e.sh: se `timeout` não existir (macOS padrão), rodar sem timeout (fallback).
E. Rastreabilidade de prompts (regra do enunciado): (1) copiar o conteúdo de
   .superpowers/sdd/PLANO-IMPLEMENTACAO/task-7.1a-fixlist.md e task-7.1b-fixlist.md para docs/PLANO-IMPLEMENTACAO.md como subseções
   "Tarefa 7.1a" e "Tarefa 7.1b" dentro da Tarefa 7.1 (texto integral, marcado como executado); (2) acrescentar ao PROMPTS.md,
   dentro da entrada "Execução do plano por subagentes", uma tabela "Tarefa → commits" com TODAS as tarefas executadas
   (0.1 … 7.2) e seus hashes curtos (obtenha com `git log --oneline` — cada commit cita a tarefa no corpo: `git log --format='%h %s%n%b'`).
   Não reescreva entradas antigas.
F. Polimento pequeno: atrito das partículas no chão escalado por dt (ex.: v *= pow(PARTICLE_GROUND_FRICTION, dt*60) ou exp(-k*dt));
   texto de debug (tecla H) desligado por padrão (g_ShowInfoText = false); Check_perf_heavy falha se média > 16 ms (Debug) para ser um
   teste de regressão de verdade.
G. CLAUDE.md: tabela de requisitos final (todos ✅ com onde cada um está), seção de controles final, módulos, como testar; remova
   afirmações desatualizadas. Lista final de atalhos para o manual do README (o README é escrito pela dupla — NÃO edite README.md):
   coloque essa lista em CLAUDE.md e no final de COMPILACAO.md? Não — só em CLAUDE.md (seção "Controles (para o manual do README)").
   Inclua limitações conhecidas (voar longe com a câmera livre mostra vazio; tênis permite pular placas; etc.).
