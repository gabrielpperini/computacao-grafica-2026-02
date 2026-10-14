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

## Requisitos técnicos e estado atual do código base

Checklist de avaliação, com o que o código base já entrega. **Nada marcado como "falta" está
implementado** — o base é só um visualizador.

| Requisito | Estado |
|---|---|
| Matrizes Model/View/Projection próprias | ✅ `include/matrices.h` |
| Malhas poligonais complexas | ✅ `bunny.obj`; quanto mais variedade, melhor |
| Interação por mouse **e** teclado | ✅ callbacks em `main.cpp` |
| Transformações geométricas controladas pelo usuário | ⚠️ parcial — `g_AngleX`/`g_AngleY` afetam esfera/coelho |
| Duas câmeras consideravelmente distintas | ❌ só look-at; falta câmera livre |
| Instâncias (≥2 Model matrices sobre o mesmo VAO) | ❌ cada objeto é desenhado uma vez |
| Testes de intersecção com propósito, em `collisions.cpp` | ❌ inexistente |
| Iluminação **não-trivial** em todos os objetos | ⚠️ só Lambert difuso com luz fixa; precisa de Phong/Blinn-Phong, especular, etc. |
| Texturas em todos os objetos, ≥3 imagens distintas | ⚠️ só 2 imagens carregadas; `TextureImage2` está declarado no shader mas nunca é preenchido |
| Curva de Bézier cúbica movimentando ≥1 objeto | ❌ |
| Animações baseadas em Δt | ⚠️ usa `glfwGetTime()` absoluto; não há acumulação de Δt por frame |
| Objetivo e lógica de controle não-triviais | ❌ |
| Funcionalidade extra obrigatória | ❌ (sugestões: partículas, sombras, billboards, GUI, picking, texto com fontes, patches de Bézier) |

Bugs que custam nota na avaliação: Z-fighting, texturas esticadas, colisões incorretas,
movimentação travada ou com flickering, e qualquer crash durante a demonstração.

**Restos do laboratório anterior, atualmente sem efeito:** `g_AngleZ`, `g_ForearmAngleX/Z` e
`g_TorsoPositionX/Y` são atualizados pelos callbacks mas não entram em nenhuma matriz de
modelagem. `src/buildtriangles.cpp` não é compilado por nenhum build.

---

## Build e execução

Não há suíte de testes. A verificação é compilar e inspecionar visualmente a aplicação rodando.

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

### Ao adicionar um novo arquivo `.cpp`

É preciso registrá-lo em **dois** lugares independentes:

1. a lista `SOURCES` no topo de `CMakeLists.txt` (o CMake aborta com `FATAL_ERROR` se um arquivo
   listado não existir);
2. a linha do `g++` em `Makefile.macOS` (ela lista os fontes explicitamente).

**Divergência conhecida entre os dois builds:** `Makefile.macOS` não compila `src/correcao.cpp`,
que é onde `Correcao_KeyCallback()` — chamada em `main.cpp:1218` — está definida. No macOS isso
resulta em símbolo indefinido no link; adicione `src/correcao.cpp` à linha do `g++`.
(Não verificado por build local: `glfw` não estava instalado no ambiente.)

---

## Arquitetura

Toda a aplicação vive em `src/main.cpp` (~1600 linhas). Os demais `.cpp` são bibliotecas de apoio:
`textrendering.cpp` (texto na janela via `include/dejavufont.h`), `tiny_obj_loader.cpp`,
`stb_image.cpp` e `glad.c` são apenas *wrappers* de header-only libs, e `correcao.cpp` é
infraestrutura de correção automatizada.

### Pipeline de carga de geometria

```
ObjModel("../../data/x.obj")            // tinyobjloader; lança se algum shape estiver sem nome
  → ComputeNormals(&model)              // gera normais por vértice se o OBJ não trouxer
  → BuildTrianglesAndAddToVirtualScene  // cria VAO + VBOs + índices, calcula a AABB
  → g_VirtualScene["<nome do shape>"]   // std::map<std::string, SceneObject>
  → DrawVirtualObject("<nome do shape>")// bind VAO, envia bbox_min/max, glDrawElements
```

A chave de `g_VirtualScene` é o **nome do objeto dentro do arquivo OBJ** (`the_sphere`,
`the_bunny`, `the_plane`), não o nome do arquivo. Um OBJ com vários shapes vira várias entradas
compartilhando o mesmo VAO. Objetos sem nome no OBJ fazem o programa lançar exceção.

Para **instâncias** (requisito obrigatório), basta chamar `DrawVirtualObject()` várias vezes com
`model` diferente entre as chamadas — o VAO é reaproveitado, nenhum vértice é duplicado.
A AABB em `SceneObject.bbox_min/bbox_max` está em coordenadas de modelo e é a base natural para
os testes de colisão.

### Contrato C++ ↔ GLSL

Três pontos precisam ser mantidos em sincronia manualmente:

- **Atributos de vértice**: `location 0` = posição (`vec4`), `1` = normal (`vec4`), `2` = texcoords
  (`vec2`). Definidos em `BuildTrianglesAndAddToVirtualScene()` e declarados em
  `shader_vertex.glsl`. Os VBOs de normal e textura só são criados se o OBJ tiver esses dados.
- **Uniforms**: `model`, `view`, `projection`, `object_id`, `bbox_min`, `bbox_max` têm seus
  locations cacheados nos globais `g_*_uniform` em `LoadShadersFromFiles()`. Um uniform novo
  precisa ser declarado no shader, ter o global criado e o `glGetUniformLocation()` adicionado ali.
- **IDs de objeto**: os `#define SPHERE/BUNNY/PLANE` existem duplicados dentro de `main()` e no
  topo de `shader_fragment.glsl` — alterar um exige alterar o outro. O fragment shader usa
  `object_id` para escolher o modo de texturização (projeção esférica, planar XY, ou UV do OBJ).

**Texturas**: `LoadTextureImage()` atribui a texture unit pela *ordem das chamadas* — a primeira
vira `TextureImage0`, a segunda `TextureImage1`, etc. (`g_NumLoadedTextures` como contador).
Inserir uma chamada no meio da lista remapeia todas as seguintes. Cada sampler novo precisa ser
declarado no shader e vinculado com `glUniform1i()` no final de `LoadShadersFromFiles()`.

### Matemática e câmera

Além da proibição do item 6, note que `near`/`far` são **negativos** no sistema de coordenadas da
câmera (`nearplane = -0.1f`, `farplane = -10.0f`), e que as matrizes de `matrices.h` são escritas
em row-major no código mas armazenadas column-major.

A câmera é look-at em coordenadas esféricas, recomputada a cada frame a partir de
`g_CameraTheta`, `g_CameraPhi` e `g_CameraDistance`. `PushMatrix()`/`PopMatrix()` implementam uma
pilha de matrizes de modelagem para hierarquias de objetos.

### Estado e entrada

Todo o estado da aplicação são variáveis globais `g_*` mutadas pelos callbacks do GLFW. Controles
atuais: botão esquerdo do mouse orbita a câmera; direito controla `g_ForearmAngle*`; do meio
controla `g_TorsoPosition*`; scroll dá zoom; `X`/`Y`/`Z` (com Shift para inverter) mexem nos
ângulos de Euler; `Espaço` reseta; `P`/`O` alternam projeção perspectiva/ortográfica; `H` liga e
desliga o texto informativo; `R` recarrega shaders; `ESC` fecha. Todo atalho precisa acabar
documentado no manual dentro do `README.md`.

`Correcao_KeyCallback()` (`src/correcao.cpp`) deve continuar sendo o primeiro comando de
`KeyCallback()`: `Shift+0..9` encerra o processo com código `100+N` para a correção automatizada.
Não altere esse trecho.

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
