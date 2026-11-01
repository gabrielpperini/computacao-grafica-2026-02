# Compilação e execução

O jogo (*Rail Rush*) é C++11 + OpenGL 3.3 core. **Tudo o que ele usa já vem no repositório**:
GLFW pré-compilada (`lib-*/`), GLAD, GLM, stb_image, tinyobjloader e o Dear ImGui
(`third_party/imgui/`, copiado sem alterações, licença MIT). Não há nada de ImGui para instalar:
só o compilador, o CMake e as bibliotecas de sistema listadas abaixo.

Resumo do que foi testado (Tarefa 7.2):

| Plataforma | Estado |
|---|---|
| Windows 11, MinGW-w64 UCRT (GCC 16) + CMake + Ninja, Debug e Release | testado: compila sem warnings do nosso código, `logic_tests` OK, E2E completo OK |
| Windows com MSVC (`lib-vc2022/`) | **não testado** (sem Visual Studio na máquina de teste) |
| Linux (Ubuntu 24.04) com CMake e com `make` | testado num contêiner Docker: compila sem warnings, linka e `logic_tests` OK; o cenário E2E `smoke` rodou (OpenGL por software, Xvfb + Mesa). O jogo completo só foi jogado/inspecionado no Windows |
| macOS (`Makefile.macOS`) | **não testado** (sem macOS disponível); só se conferiu que a lista de fontes e as flags batem com o `CMakeLists.txt` |

> [!IMPORTANT]
> O executável precisa ser executado **de dentro de `bin/<plataforma>/`**, porque carrega os
> assets por caminhos relativos (`../../data/...`, `../../src/shader_*.glsl`). Rodar de outro
> diretório falha com "Cannot open image file" ou erro ao carregar o shader. Todos os alvos
> `run` já fazem esse `cd`.

Onde ficam os executáveis:

| Plataforma | Build | Executável |
|---|---|---|
| Windows | Debug (padrão) | `bin/Debug/main.exe` |
| Windows | Release | `bin/Release/main.exe` |
| Linux | qualquer | `bin/Linux/main` |
| macOS | `Makefile.macOS` | `bin/macOS/main` |

Para a **apresentação**, compile em **Release**: o build padrão do CMake é Debug (cerca de 150 fps
no cenário mais pesado numa GTX 1070, contra ~540 fps em Release). Use
`-DCMAKE_BUILD_TYPE=Release` (veja cada plataforma).

# Linux

Para compilar e executar este projeto no Linux, primeiro você precisa
instalar as bibliotecas necessárias (nos computadores dos laboratórios
do INF, estas bibliotecas já estão instaladas, então você pode pular
estes passos). Num Ubuntu (testado no 24.04), o comando abaixo é suficiente:

```bash
sudo apt-get install build-essential cmake make libx11-dev libxrandr-dev \
                     libxinerama-dev libxcursor-dev libgl-dev
```

(`cmake` só é necessário para o caminho com CMake; `libgl-dev` fornece o `libGL` e os headers
do OpenGL. O link é estático com `lib-linux/libglfw3.a`, então não precisa instalar a GLFW.)

Se você usa Linux Mint, talvez seja necessário instalar mais algumas bibliotecas:

```bash
sudo apt-get install libgl1-mesa-dev libxxf86vm-dev
```

Após a instalação das bibliotecas acima, você possui duas opções para compilação:

## Linux com CMake (melhor opção)

Abra um terminal, navegue até a pasta onde está este código fonte, e execute
o seguinte comando:

```bash
cmake --workflow --preset configure-build-run
```

isso é equivalente aos comandos:

```bash
cmake -B build -S .          # Cria e configura diretório de build
cmake --build build          # Faz a compilação
cmake --build build -- run   # Executa o código compilado
```

> [!TIP]
> Esta é a melhor opção, pois o CMake gera automaticamente uma
> configuração que reutiliza arquivos previamente compilados, e
> recompila somente os arquivos que foram modificados.

Para compilar em **Release** (recomendado para a apresentação):

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build -- run
```

Também é possível remover todos arquivos compilados com:

```bash
cmake --build build -- clean # Remove arquivos compilados
cmake --build build -- run   # Re-compila e executa
```

## Linux com Makefile

Abra um terminal, navegue até a pasta onde está este código fonte, e execute
o comando "make" para compilar (ele chama os presets do CMake, em Debug, e deixa o
executável em `bin/Linux/main`). Para executar o código compilado, execute o
comando "make run".

## Linux com VSCode

1) Instale o VSCode seguindo as instruções em https://code.visualstudio.com/ .

2) Instale as extensões "ms-vscode.cpptools" e "ms-vscode.cmake-tools"
no VSCode. Se você abrir o diretório deste projeto no VSCode,
automaticamente será sugerida a instalação destas extensões (pois
estão listadas no arquivo ".vscode/extensions.json").

3) Clique no botão de "Play" *NA BARRA INFERIOR* do VSCode para compilar
e executar o projeto. Na primeira compilação, a extensão do CMake para
o VSCode irá perguntar qual compilador você quer utilizar. Selecione
da lista o compilador que você deseja utilizar.

Veja mais instruções de uso do CMake no VSCode em:

https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/README.md

# macOS

> [!WARNING]
> **Não testado.** Esta equipe não tinha acesso a um macOS. O `Makefile.macOS` lista os mesmos
> fontes do `CMakeLists.txt` (incluindo o ImGui e os `-I` dos seus backends) e o mesmo comando
> foi compilado num Linux (sem warnings, e o binário rodou o E2E `smoke`) trocando só as flags específicas
> do macOS (`-framework OpenGL` por `-lGL`), mas isso não substitui um teste real.

Para compilar e executar esse projeto no macOS, primeiro você precisa instalar o
HOMEBREW, um gerenciador de pacotes para facilitar a instação de bibliotecas. O
HOMEBREW pode ser instalado com o seguinte comando no terminal:

```zsh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Após a instalação do HOMEBREW, a biblioteca GLFW deve ser instalada. Isso pode
ser feito pelo terminal com o comando:

```zsh
brew install glfw
```

## macOS com Makefile
Abra um terminal, navegue até a pasta onde está este código fonte, e execute
o comando "make -f Makefile.macOS" para compilar. Para executar o código
compilado, execute o comando "make -f Makefile.macOS run".

## macOS com CMake
Você utiliza macOS e conseguiu utilizar o CMake para compilar este projeto?
Então mande um e-mail para o professor em <eslgastal@inf.ufrgs.br> com
as alterações necessárias no CMakeLists.txt e uma lista de instruções
para colocar neste LEIAME.txt .

# Windows

O `CMakeLists.txt` detecta o compilador e escolhe a GLFW certa dentro do repositório
(`lib-ucrt-64/`, `lib-mingw-64/` ou `lib-mingw-32/` para MinGW; `lib-vc2022/` para MSVC).

## Windows com MinGW pela linha de comando (caminho testado)

Foi o caminho usado nos testes desta equipe, sem precisar de administrador:

1) Instale o **WinLibs MinGW-w64 UCRT** (traz `g++`, `cmake`, `ninja` e `mingw32-make`):

```powershell
winget install BrechtSanders.WinLibs.POSIX.UCRT
```

   O `winget` instala em
   `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin`;
   abra um terminal novo (ou coloque essa pasta no `PATH` da sessão).
   (Alternativa não testada aqui: CMake e Ninja avulsos, por exemplo `pip install cmake ninja`, com qualquer MinGW-w64 no `PATH`.)

2) Na pasta do projeto (**sem espaços no caminho**), configure e compile:

```bash
# Debug (padrão)
cmake -G Ninja -B build -S .
cmake --build build

# Release (recomendado para a apresentação)
cmake -G Ninja -B build-release -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

   O executável principal e os testes unitários saem em `bin/Debug/` (`main.exe`, `logic_tests.exe`)
   ou em `bin/Release/` — a pasta depende do tipo de build, não do diretório `build*`.

3) Execute **de dentro da pasta do executável**:

```bash
cd bin/Release       # ou bin/Debug
./main.exe
```

   Observações: o runtime do MinGW é linkado estaticamente (`-static-libgcc -static-libstdc++`), então o
   `main.exe` roda sem as DLLs do MinGW no `PATH`. Se o Windows (App Control / antivírus) recusar um
   `.exe` recém-linkado com "Permission denied", toque num `.cpp` (por exemplo, salve-o de novo),
   recompile e tente outra vez.

## Windows com VSCode (Visual Studio Code)

1) Instale o VSCode seguindo as instruções em https://code.visualstudio.com/ .

2) Instale o compilador GCC no Windows seguindo as instruções em
https://code.visualstudio.com/docs/cpp/config-mingw#_installing-the-mingww64-toolchain .

Alternativamente, se você já possui o Code::Blocks instalado no seu PC
(versão que inclui o MinGW), você pode utilizar o GCC que vem com esta
instalação no passo 5.

3) Instale o CMake seguindo as instruções em https://cmake.org/download/ .
Alternativamente, você pode utilizar algum package manager do
Windows para fazer esta instalação, como https://chocolatey.org/ .

4) Instale as extensões "ms-vscode.cpptools" e "ms-vscode.cmake-tools"
no VSCode. Se você abrir o diretório deste projeto no VSCode,
automaticamente será sugerida a instalação destas extensões (pois
estão listadas no arquivo ".vscode/extensions.json").

5) Abra as configurações da extensão cmake-tools (Ctrl-Shift-P e
busque por "CMake: Open CMake Tools Extension Settings"), e adicione o
caminho de instalação do GCC na opção de configuração "additionalCompilerSearchDirs".

Por exemplo, se você quiser utilizar o compilador MinGW que vem junto
com o Code::Blocks, pode preencher o diretório como
"C:\Program Files\CodeBlocks\MinGW\bin" (verifique se este é o local
de instalação do seu Code::Blocks).

6) Clique no botão de "Play" na barra inferior do VSCode para compilar
e executar o projeto. Na primeira compilação, a extensão do CMake para
o VSCode irá perguntar qual compilador você quer utilizar. Selecione
da lista o compilador GCC que você instalou com o MSYS/MinGW.
Para Release, escolha a variante "Release" na barra inferior do VSCode
(o executável vai para `bin/Release/`).

Veja mais instruções de uso do CMake no VSCode em:

https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/README.md

## Windows com MSVC (não testado)

O `CMakeLists.txt` também reconhece o MSVC e usa `lib-vc2022/glfw3.lib`. Não conseguimos testar
(sem Visual Studio na máquina de teste). Em princípio: abrir o "Developer PowerShell for VS" e
rodar `cmake -B build -S .` seguido de `cmake --build build --config Release`; o executável sai em
`bin/Release/` e, como no MinGW, deve ser executado de dentro dele.

# Testes

Há dois tipos de teste, ambos opcionais para jogar mas obrigatórios antes de entregar:

## Testes unitários da lógica (`logic_tests`)

Testam, sem janela e sem OpenGL, os módulos de lógica (pista, jogador, colisões, power-ups, Bézier,
câmera, partículas...). Rodam em poucos segundos e imprimem `OK` no fim (um `assert` que falha
derruba o programa com a linha do teste).

```bash
cmake --build build --target logic_tests      # use build-release no Release
./bin/Linux/logic_tests                       # Linux
./bin/Debug/logic_tests.exe                   # Windows (Debug)
```

O alvo existe só no CMake (não no `Makefile.macOS`).

## Testes ponta a ponta (E2E, `tests/run_e2e.sh`)

Abrem o jogo de verdade (precisam de **janela e GPU/OpenGL 3.3**), com `dt` fixo de 1/60 s, teclas e
cliques injetados por um roteiro, checagem de `glGetError` e screenshots PNG. No Windows use o
**Git Bash**; na raiz do repositório, depois de compilar:

```bash
bash tests/run_e2e.sh                  # todos os cenários (~10 min no Windows)
bash tests/run_e2e.sh smoke tunnel     # só alguns
E2E_BIN_DIR=bin/Release bash tests/run_e2e.sh   # Windows: testa o build Release
```

Cada cenário imprime `PASS <nome>` ou `FAIL <nome>`, e o script termina com o total
(`E2E: N/N cenarios passaram`) e código de saída diferente de zero se algum falhar. Os screenshots
ficam em `bin/<plataforma>/e2e/<cenario>_<rotulo>.png`. Cada cenário tem limite de 60 s; se o comando
`timeout` não existir (macOS padrão, sem `coreutils`), o script avisa e roda sem limite de tempo.

`./main --autotest-list` (dentro de `bin/<plataforma>/`) lista os cenários. Eles cobrem:

- **Jogabilidade:** `smoke`, `lane_jump_roll`, `long_run`, `obstacle_types`, `coins`, `showcase`,
  `game_over`, `restart`, `restart_space`;
- **Pista:** `curve`, `curve_obstacles`, `tunnel`, `tunnel_side`;
- **Trens e rampas:** `moving_train`, `moving_train_hit`, `ramp_train`, `roof_hop`, `ramp_side_block`;
- **Power-ups, partículas e Bézier:** `powerups`, `piggy`, `particles`;
- **Visual:** `lighting`, `visual_audit` (câmera livre em 13 poses rasantes, caça Z-fighting e textura
  esticada);
- **Câmera livre e GUI:** `free_camera`, `menu_pause`, `pause_freeze`, `gui_keys`, `gui_buttons`,
  `game_over_gui`, `game_over_menu`, `gui_small_window`;
- **Robustez e desempenho:** `marathon` (600 s de jogo com piloto automático: sem vazamento, sem erro
  de GL) e `perf_heavy` (cenário mais pesado: **falha se a média passar de 16 ms por quadro**, mesmo
  em Debug).

No Linux/macOS o script usa `bin/Linux` / `bin/macOS`. Sem monitor (por exemplo, num servidor), é
preciso um X virtual com OpenGL por software (`xvfb-run` e Mesa).

# Soluções de Problemas

Caso você tenha problemas de compilação no Windows com GCC, cuide para
extrair o código do laboratório em um caminho que não contenha espaços
no nome de algum diretório:

- Caminho OK: `C:\Users\JohnDoe\Documents\CGVis\Lab1`

- Caminho NÃO OK: `C:\Users\JohnDoe\Documents\Fundamentos de CG\Lab1`

Caso você tenha problemas em executar o código deste projeto, tente atualizar o
driver da sua placa de vídeo (é preciso OpenGL 3.3 ou superior).

Se o jogo abrir e fechar na hora, ou reclamar de arquivo não encontrado, confira que você o
executou de dentro de `bin/<plataforma>/` (veja o aviso no início).

O shader pode ser editado sem recompilar: salve `src/shader_vertex.glsl` ou
`src/shader_fragment.glsl` e aperte `R` com a janela do jogo em foco.
