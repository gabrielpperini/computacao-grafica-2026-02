# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Contexto

Trabalho final de **INF01047 — Computação Gráfica e Visualização I (INF/UFRGS)**, feito em dupla.
O código base é uma aplicação OpenGL 3.3 core profile em C++11 (GLFW + GLAD + GLM), que carrega
modelos `.obj` via tinyobjloader e texturas via stb_image.

O enunciado completo está em `ordem-trabalho.md` (versão 2026-09-16). **Em caso de conflito entre
este arquivo e `ordem-trabalho.md`, o enunciado prevalece** — várias regras abaixo são
eliminatórias (nota zero) ou geram desconto direto de nota.

---

## ⚠️ Regras obrigatórias (violação = nota zero ou desconto)

### 1. Commits de código gerado por IA

Todo commit cujo código foi gerado ou editado por IA **deve ter a mensagem iniciada por `IA: `**.

```
IA: implementa curva de Bézier cúbica para movimentação do inimigo
```

Commits escritos à mão pela dupla **não** levam esse prefixo. Não misture código humano e código
de IA no mesmo commit — a separação no histórico do Git é obrigatória.

### 2. Registro dos prompts

O prompt que gerou o código precisa estar no repositório. Convenção deste repo: registrar em
**`PROMPTS.md`**, commitado junto com o código gerado. A alternativa permitida pelo enunciado é
colocar o prompt na própria mensagem do commit, precedido de `PROMPT:` em maiúsculo — nesse caso
use uma das duas formas de forma consistente, nunca metade em cada.

### 3. Arquivos que a IA NÃO pode escrever

- **`SPEC.md`** — proibido pelo enunciado ("não pode utilizar ferramentas de IA para escrever
  esta especificação"). Leia para entender o escopo; nunca edite.
- **`README.md`** (relatório final) — "O uso de IA para escrever o relatório é proibido".

Se for pedido para preencher esses arquivos, recuse e explique o motivo. Ajudar a **revisar** ou
apontar inconsistências é aceitável; escrever o conteúdo não é.

### 4. Código copiado de terceiros

Todo trecho de código de fonte externa deve ter comentário com a palavra **`FONTE`** em maiúsculo,
identificando de onde veio, e a licença precisa permitir o uso:

```cpp
// FONTE: https://github.com/opengl-tutorials/ogl — Ray-OBB intersection (licença WTFPL)
```

Assets (modelos 3D, texturas, fontes, áudio) só podem entrar no repositório público se a licença
permitir redistribuição. As fontes dos assets vão em **`data/FONTES.txt`** (já existe, com os
links do Polyhaven). Arquivo externo que não possa ir para o GitHub deve ser documentado no
`README.md` com instruções explícitas de como obtê-lo.

### 5. APIs e bibliotecas permitidas

Somente **APIs gráficas de baixo nível**: OpenGL, Direct3D, Metal, WebGPU, Vulkan. Motores
gráficos são proibidos (Unity, Unreal, Godot, raylib, pygame, Ogre, Three.js, Bevy, …).
Bibliotecas utilitárias não-gráficas são permitidas; o enunciado sugere explicitamente
`imgui` (GUI), `freetype-gl` (texto) e `miniaudio` (áudio).

### 6. Funções de matriz proibidas

A dupla precisa implementar Model, View e Projection por conta própria. **Nunca** use:

```
gluLookAt()  gluOrtho2D()  gluPerspective()  gluPickMatrix()  gluProject()  gluUnProject()
glm::lookAt()  glm::ortho()  glm::perspective()  glm::pickMatrix()
glm::rotate()  glm::scale()  glm::translate()
```

Use sempre os helpers de `include/matrices.h`: `Matrix_Translate`, `Matrix_Scale`,
`Matrix_Rotate_X/Y/Z`, `Matrix_Rotate`, `Matrix_Camera_View`, `Matrix_Perspective`,
`Matrix_Orthographic`, `crossproduct`, `dotproduct`, `norm`. GLM só para os *tipos*
(`glm::vec4`, `glm::mat4`) e para `glm::value_ptr`.

### 7. Testes de colisão em arquivo separado

Os testes de intersecção **devem** ficar em **`src/collisions.cpp`** (nome exigido pelo
enunciado, é critério de avaliação). Ainda não existe — ao criá-lo, registre-o nos dois builds
(veja "Ao adicionar um novo arquivo `.cpp`").

### 8. Histórico do Git

- Avanço **incremental**: um commit único com 100% do código final é inaceitável.
- **Ambos os integrantes** da dupla precisam ter commits no histórico (nota individual depende
  disso). Não crie commits em nome do outro integrante — em pair programming, alternem quem
  commita.
- Sempre `git push` antes de coletar o hash do commit final para a entrega.

---

## Prazos

| Data | O quê |
|---|---|
| 21/09/2026 | Google Forms + primeira versão completa do `SPEC.md` (depois da revisão do professor, `SPEC.md` fica congelado) |
| 04 e 11/11/2026 | Avaliação parcial em laboratório (precisa compilar e rodar) |
| 07 e 09/12/2026 | Apresentação final para a turma (máx. 10 min) |
| 11/12/2026, 23h55 | Entrega final no Moodle |

---

## Plano de implementação

O plano do jogo *Rail Rush* (o que está em `SPEC.md`) está em
**[`docs/PLANO-IMPLEMENTACAO.md`](docs/PLANO-IMPLEMENTACAO.md)**, dividido em 8 fases (0 a 7) com
prazos alinhados às avaliações parciais. Antes de implementar qualquer coisa, leia a fase/tarefa
correspondente: ele fixa a arquitetura (lógica sem OpenGL em módulos testáveis, coordenadas de
pista `(lateral, altura, s)`), nomes de arquivos, assinaturas e constantes. Ao concluir uma tarefa,
marque os checkboxes no plano e atualize o checklist abaixo no mesmo commit. Se a implementação
precisar divergir do plano, atualize o plano junto.

---

## Requisitos técnicos e estado atual do código base

Checklist de avaliação, com o que o código base já entrega. **Nada marcado como "falta" está
implementado** — o base é só um visualizador.

| Requisito | Estado |
|---|---|
| Matrizes Model/View/Projection próprias | ✅ `include/matrices.h` |
| Malhas poligonais complexas | ✅ `bunny.obj`; quanto mais variedade, melhor |
| Interação por mouse **e** teclado | ⚠️ só teclado (controles do jogador em `HandleKey()`); a órbita de mouse do laboratório foi removida na Tarefa 1.3 — o mouse volta com a câmera livre |
| Transformações geométricas controladas pelo usuário | ✅ o jogador (troca de faixa, pulo, rolamento achatando o modelo) é transformado pela matriz de pista a partir do teclado |
| Duas câmeras consideravelmente distintas | ⚠️ câmera look-at em 3ª pessoa (`ThirdPersonCamera`, `camera.cpp`); falta a câmera livre |
| Instâncias (≥2 Model matrices sobre o mesmo VAO) | ✅ prédios: o VAO de `the_cube` desenhado a cada 12 m nos dois lados, cada um com sua `model` |
| Testes de intersecção com propósito, em `collisions.cpp` | ⚠️ API pronta, integração na 2.2 |
| Iluminação **não-trivial** em todos os objetos | ⚠️ só Lambert difuso com luz fixa; precisa de Phong/Blinn-Phong, especular, etc. |
| Texturas em todos os objetos, ≥3 imagens distintas | ✅ `DrawObject()` liga uma textura por objeto na unidade 0; 3 imagens (`red_brick`, `rocky_terrain_02`, `metal_plate`); revisar quando entrarem os modelos finais |
| Curva de Bézier cúbica movimentando ≥1 objeto | ❌ |
| Animações baseadas em Δt | ✅ `ComputeFrameDt()` em `main.cpp` (limitado a `MAX_DT`); animações acumulam `dt` |
| Objetivo e lógica de controle não-triviais | ❌ |
| Funcionalidade extra obrigatória | ❌ (sugestões: partículas, sombras, billboards, GUI, picking, texto com fontes, patches de Bézier) |

Bugs que custam nota na avaliação: Z-fighting, texturas esticadas, colisões incorretas,
movimentação travada ou com flickering, e qualquer crash durante a demonstração.

**Resto do laboratório anterior, sem efeito:** `src/buildtriangles.cpp` não é compilado por
nenhum build. (As variáveis `g_AngleX/Y/Z`, `g_ForearmAngle*` e `g_TorsoPosition*` e as teclas
`X`/`Y`/`Z` foram removidas na Tarefa 0.1.)

---

## Build e execução

Os módulos de lógica sem GL têm testes com `assert` em `tests/logic_tests.cpp` (alvo CMake `logic_tests`):
`cmake --build build --target logic_tests` e depois `./bin/Linux/logic_tests` (no Windows,
`bin/Debug/logic_tests.exe`); imprime `OK` se tudo passar. Não entra no `Makefile.macOS`. A parte gráfica
não tem teste automatizado: a verificação é compilar e inspecionar visualmente a aplicação rodando.

**macOS** (requer `brew install glfw`):

```bash
make -f Makefile.macOS       # compila para bin/macOS/main
make -f Makefile.macOS run   # compila e executa
make -f Makefile.macOS clean
```

**Linux / CMake** (opção preferida no Linux):

```bash
cmake --workflow --preset configure-build-run   # configura + compila + executa
cmake -B build -S . && cmake --build build      # só compila (bin/Linux/main)
cmake --build build -- run
cmake --build build -- clean
make                                            # atalho: chama os presets do CMake
make run
```

**Windows**: só via CMake/VSCode com MinGW ou MSVC — o `CMakeLists.txt` detecta a toolchain e
escolhe o `libglfw3.a`/`glfw3.lib` correto dentro de `lib-mingw-32/`, `lib-mingw-64/`,
`lib-ucrt-64/` ou `lib-vc2022/`. Detalhes de setup em `COMPILACAO.md`.

O `README.md` da entrega final precisa explicar todos os passos de compilação e execução, e o
critério "facilidade de compilação e execução, com instruções corretas e reproduzíveis" é
avaliado — mantenha `COMPILACAO.md` e os builds coerentes com a realidade.

### O executável só funciona a partir de `bin/<plataforma>/`

`main.cpp` carrega assets por caminhos relativos fixos: `../../data/*.obj`, `../../data/*.jpg` e
`../../src/shader_*.glsl`. Por isso todos os alvos `run` fazem `cd bin/<plataforma> && ./main`.
Rodar o binário de outro diretório falha com "Cannot open image file" ou erro ao carregar shader.

### Tecla `R` recarrega os shaders em runtime

Editar `src/shader_vertex.glsl` ou `src/shader_fragment.glsl` **não exige recompilar**: basta
apertar `R` com a janela em foco (`LoadShadersFromFiles()` é chamada de novo).

### Testes E2E (`--autotest`)

`src/autotest.cpp` roda o executável real com dt fixo (1/60 s), teclas injetadas, screenshots
PNG e checagem de `glGetError`. Depois de compilar, na raiz do repo: `bash tests/run_e2e.sh`
(todos os cenários, ou `bash tests/run_e2e.sh smoke`); sai com código ≠ 0 se algum falhar.
Manualmente: `cd bin/<plataforma> && ./main --autotest <cenario>` (`--autotest-list` lista os
nomes). Screenshots em `bin/<plataforma>/e2e/<cenario>_<rotulo>.png`: abra com a ferramenta
Read para conferir visualmente. **Para criar um cenário** edite só `BuildScenarios()` em
`src/autotest.cpp` (o roteiro está documentado no topo do arquivo). Toda tarefa que muda
comportamento visível deve acrescentar um cenário. As teclas do harness passam por
`HandleKey()` (em `main.cpp`), nunca por `Correcao_KeyCallback()`. O `final_check` lê o estado
do jogo pelo global `g_Game` (semente fixa 12345 no autotest). Um cenário falha também com
passos além da duração ou se a janela fechar antes do fim. Cenários atuais: `smoke`,
`lane_jump_roll` (faixa, pulo, rolamento) e `long_run` (60 s de corrida).

### Ao adicionar um novo arquivo `.cpp`

É preciso registrá-lo em **dois** lugares independentes:

1. a lista `SOURCES` no topo de `CMakeLists.txt` (o CMake aborta com `FATAL_ERROR` se um arquivo
   listado não existir);
2. a linha do `g++` em `Makefile.macOS` (ela lista os fontes explicitamente).

`Makefile.macOS` já lista `src/correcao.cpp` (onde `Correcao_KeyCallback()` está definida).
(Não verificado por build local no macOS.)

---

## Arquitetura

`src/main.cpp` (~1600 linhas) cuida de janela, shaders, texturas, VAOs, desenho e callbacks. A
lógica sem OpenGL fica em módulos testados por `tests/logic_tests.cpp`: `track.cpp` (pista e
referencial `FrameAt(s)`), `game.cpp` (estado, jogador, regras; global `g_Game` em `main.cpp`),
`camera.cpp` (`ThirdPersonCamera`: pose look-at suavizada no referencial da pista) e
`scenery.cpp` (faixas de chão/trilhos e prédios do trecho visível `[s-20, s+150]`). Os demais
`.cpp` são bibliotecas de apoio: `textrendering.cpp` (texto na janela via `include/dejavufont.h`),
`tiny_obj_loader.cpp`, `stb_image.cpp` e `glad.c` são apenas *wrappers* de header-only libs,
`autotest.cpp` é o harness E2E e `correcao.cpp` é infraestrutura de correção automatizada.

**Faixas dinâmicas:** chão e trilhos não são OBJ: `DynamicStripMesh` (em `main.cpp`) cria VAO e
VBOs uma vez com capacidade máxima (`GL_DYNAMIC_DRAW`), e a cada quadro `scenery.cpp` gera os
vértices em coordenadas de mundo e `UpdateDynamicStripMesh()` os reenvia com `glBufferSubData`;
o desenho é `GL_TRIANGLE_STRIP` com `model` = identidade.

### Pipeline de carga de geometria

```
ObjModel("../../data/x.obj")            // tinyobjloader; lança se algum shape estiver sem nome
  → ComputeNormals(&model)              // gera normais por vértice se o OBJ não trouxer
  → BuildTrianglesAndAddToVirtualScene  // cria VAO + VBOs + índices, calcula a AABB
  → g_VirtualScene["<nome do shape>"]   // std::map<std::string, SceneObject>
  → DrawVirtualObject("<nome do shape>")// bind VAO, envia bbox_min/max, glDrawElements
```

A chave de `g_VirtualScene` é o **nome do objeto dentro do arquivo OBJ** (`the_bunny`,
`the_cube`; hoje só esses dois são carregados), não o nome do arquivo. Um OBJ com vários shapes vira várias entradas
compartilhando o mesmo VAO. Objetos sem nome no OBJ fazem o programa lançar exceção.

Para **instâncias** (requisito obrigatório), basta chamar `DrawObject()` (ou `DrawVirtualObject()`)
várias vezes com `model` diferente entre as chamadas — o VAO é reaproveitado, nenhum vértice é duplicado.
A AABB em `SceneObject.bbox_min/bbox_max` está em coordenadas de modelo e é a base natural para
os testes de colisão.

### Contrato C++ ↔ GLSL

Três pontos precisam ser mantidos em sincronia manualmente:

- **Atributos de vértice**: `location 0` = posição (`vec4`), `1` = normal (`vec4`), `2` = texcoords
  (`vec2`). Definidos em `BuildTrianglesAndAddToVirtualScene()` e declarados em
  `shader_vertex.glsl`. Os VBOs de normal e textura só são criados se o OBJ tiver esses dados.
- **Uniforms**: `model`, `view`, `projection`, `object_id`, `uv_mode`, `uv_scale`, `bbox_min`, `bbox_max` têm seus
  locations cacheados nos globais `g_*_uniform` em `LoadShadersFromFiles()`. Um uniform novo
  precisa ser declarado no shader, ter o global criado e o `glGetUniformLocation()` adicionado ali.
- **IDs de objeto e modos de UV**: os `#define` (`PLAYER`, `GROUND`, ..., `PARTICLE` e
  `UV_OBJ`/`UV_PLANAR_XZ`/`UV_BBOX_XY`/`UV_SPHERICAL`/`UV_BOX`) vivem em `include/object_ids.h` e são
  **copiados** para o topo de `shader_fragment.glsl` — alterar um exige alterar o outro. O fragment
  shader usa `uv_mode` para gerar U,V (UV do OBJ, planar XZ em coordenadas de mundo, planar XY
  normalizado pela AABB, ou esférico) e multiplica por `uv_scale`; `object_id` fica reservado para
  a iluminação por tipo de objeto.

**Texturas**: há **uma** unidade de textura (0, sampler `TextureImage0`). `LoadTextureImage()`
devolve o id da textura (`GL_REPEAT` em S e T, parâmetros guardados na própria textura, sem sampler
object) e não depende mais da ordem das chamadas. Quem desenha usa
`DrawObject(nome, model, object_id, textura, uv_mode, uv_scale)`, que liga a textura na unidade 0,
envia os uniforms e chama `DrawVirtualObject()`. Para textura repetida (chão, paredes) use
`uv_scale > 1` ou `UV_PLANAR_XZ`. Para caixas escaladas de forma não uniforme (prédios) use
`UV_BOX`: projeção por face em metros do objeto (escala lida das colunas de `model`), com
`uv_scale` = repetições por metro — os tijolos não esticam.

### Matemática e câmera

Além da proibição do item 6, note que `near`/`far` são **negativos** no sistema de coordenadas da
câmera (`nearplane = -0.3f`, `farplane = -250.0f`; o near maior evita Z-fighting dos trilhos a
0.03 m do chão), e que as matrizes de `matrices.h` são escritas em row-major no código mas
armazenadas column-major.

A câmera é a `ThirdPersonCamera` (`include/camera.h`): `Snap()` no início da corrida e
`Update(frame, x, in_tunnel, dt)` a cada quadro; `Pose()` dá posição/view/up para
`Matrix_Camera_View()`. Fica 6 m atrás e 3.5 m acima (4 m e 2.2 m no túnel), olha para
jogador + forward·4 + up·1, segue 1/3 do deslocamento lateral e suaviza com
`1 - exp(-6·dt)` no referencial da pista. Objetos na pista usam a matriz
`T(frame.position + right·x + up·y) · R_y(heading) · <modelo>`; `Matrix_OnGround()` encaixa a
AABB de um OBJ com a base em y = 0. `PushMatrix()`/`PopMatrix()` implementam uma pilha de
matrizes de modelagem para hierarquias de objetos.

### Estado e entrada

Todo o estado da aplicação são variáveis globais `g_*` mutadas pelos callbacks do GLFW (o estado
do jogo fica em `g_Game`). Controles atuais (só ao pressionar, sem repetição): `←`/`→` ou
`A`/`D` trocam de faixa; `↑`, `W` ou `Espaço` pulam; `↓` ou `S` rolam; `H` liga e desliga o
texto informativo (distância, velocidade, faixa, FPS); `R` recarrega shaders; `ESC` fecha.
Não há controle por mouse no momento. Todo atalho precisa acabar documentado no manual dentro
do `README.md`.

`Correcao_KeyCallback()` (`src/correcao.cpp`) deve continuar sendo o primeiro comando de
`KeyCallback()`: `Shift+0..9` encerra o processo com código `100+N` para a correção automatizada.
Não altere esse trecho. O restante do tratamento de teclas fica em `HandleKey()` (chamada logo
em seguida pelo `KeyCallback()`), que o harness E2E também usa; novos atalhos entram em `HandleKey()`.

---

## Áreas que não devem ser editadas

- `include/glm/`, `include/glad/`, `include/GLFW/`, `include/KHR/`, `include/stb_image.h`,
  `include/tiny_obj_loader.h`, `include/dejavufont.h` — dependências vendorizadas.
- `lib-*/` — binários pré-compilados da GLFW por toolchain.
- `src/correcao.cpp` e sua chamada em `KeyCallback()` — correção automatizada.
- `src/buildtriangles.cpp` — resquício de laboratório anterior; não é compilado por nenhum build.

`bin/` e `build/` são ignorados pelo git.

---

## Entrega final (11/12/2026)

ZIP no Moodle contendo `metadados-entrega.json` com as chaves `titulo_trabalho`,
`git_hash_commit_final`, `url_github`, `periodo_letivo` (`2026/2`), `local_do_video` e
`integrantes` (array com `nome`, `cartao_ufrgs`, `turma`, `nome_de_usuario_no_github`, `email`) —
formato exato no `ordem-trabalho.md`. Mais: vídeo de 3 a 5 minutos e `README.md` no GitHub com
descrição da aplicação, funcionalidade extra, contribuições de cada integrante, parágrafo de
análise crítica do uso de IA, ≥2 imagens, manual de uso e instruções de compilação.

Imagens da especificação vão em `images/spec/` (`image1`, `image2`, `image3`) — diretório ainda
não criado.
