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
| Malhas poligonais complexas | ✅ Tarefa 3.2: personagem articulado (Kenney Blocky Characters, 6 partes), trem de metrô (3 carros do Kenney Train Kit, ~2–2.5k triângulos cada), 6 prédios (Kenney City Kit, 2.2–3.6k), poste curvo, e malhas procedurais (`tools/gen_models.py`): barreiras, placa, moeda com aro, ímã, tênis, ficha x2, cofrinho, porco voador (até ~3.9k), túnel (módulo de abóbada e portal com moldura) e rampa (estrutura metálica + tábuas) |
| Interação por mouse **e** teclado | ✅ teclado: controles do jogador em `HandleKey()`; mouse: na câmera livre (Tarefa 5.1, `C`) o cursor é capturado e o movimento gira o olhar (`CursorPosCallback`). O mouse não faz nada fora da câmera livre (botões clicáveis virão com a GUI da Tarefa 6.2) |
| Transformações geométricas controladas pelo usuário | ✅ o jogador (troca de faixa, pulo, rolamento achatando o modelo) é transformado pela matriz de pista a partir do teclado; braços/pernas giram em torno de ombro/quadril (hierarquia com `PushMatrix`/`PopMatrix`) conforme corre, pula ou rola |
| Duas câmeras consideravelmente distintas | ✅ (1) 3ª pessoa look-at (`ThirdPersonCamera`, `camera.cpp`), com a adaptação ao túnel do SPEC (Tarefa 4.2: dentro do túnel desce para 2.2 m e chega a 4 m do jogador, suavemente, e volta ao sair; nunca encosta no teto de 7 m); (2) câmera livre "fly" (`FreeCamera`, Tarefa 5.1): `C` alterna `PLAYING` ↔ `FREE_CAMERA` com o jogo congelado, WASD/Espaço/Shift voam a 10 m/s, o mouse gira (pitch ±89°) |
| Instâncias (≥2 Model matrices sobre o mesmo VAO) | ✅ cada modelo é carregado uma vez e desenhado com uma `model` por ocorrência: moedas, barreiras, placas, carros de trem, prédios (6 modelos em ~28 posições), postes, módulos de túnel (um a cada 4 m) e portais |
| Testes de intersecção com propósito, em `collisions.cpp` | ✅ `Game::Update()`: caixa varrida do jogador × partes dos obstáculos (`AABBIntersectsAABB`) → game over; esfera da moeda × caixa do jogador (`SphereIntersectsAABB`) → coleta; Tarefa 4.4: rampa sob a pegada (`RampHeightUnder`/`RampHeightAt`) → o jogador sobe nela; pouso no teto do trem (`LandsOnTop`, com a base interpolada no instante em que entra na pegada do trem) → anda em cima dele (o trem só mata abaixo do teto); teto do túnel (`CeilingPenetration`) → a cabeça não atravessa a abóbada |
| Iluminação **não-trivial** em todos os objetos | ✅ Blinn-Phong (ambiente + difuso + especular) com sol direcional + até 8 luzes pontuais (lâmpadas dos postes e do teto dos túneis, atenuação quadrática); dentro dos túneis o sol e o céu não chegam (`SkyVisibility()` no shader) e, com a câmera lá dentro, o sol cai para 20% numa rampa de 0.5 s; **Phong** (por fragmento) em todos os objetos e **Gouraud** (por vértice, `shading_mode = 1`) nas moedas; material por tipo de objeto em `MaterialFor()` (`main.cpp`) |
| Texturas em todos os objetos, ≥3 imagens distintas | ✅ `DrawObject()` liga uma textura por objeto na unidade 0; 10 imagens (`g_Tex`): terreno, brita, madeira dos dormentes, metal dos trilhos, tijolos dos túneis (Poly Haven), colormaps/skin da Kenney (trem, personagem, prédios, poste) e o atlas procedural (barreiras, placa, moeda, power-ups); padrões em densidade fixa por metro (nada esticado) |
| Curva de Bézier cúbica movimentando ≥1 objeto | ❌ |
| Túneis (SPEC) | ✅ Tarefa 4.2: o gerador às vezes (~1 em 3) põe um túnel de 40–80 m (múltiplo de 4 m) entre duas retas (reta, túnel, reta; nunca na largada); módulos de abóbada de tijolos (vão de 10 m, 7 m de altura: trens passam em todas as faixas) e portais como instâncias, luminárias no teto a cada 10 m como luzes pontuais; prédios e postes de rua são omitidos ao longo do túnel |
| Pista com trechos curvos (SPEC) | ✅ Tarefa 4.1: o gerador de `track.cpp` alterna reta (60–150 m; a da largada ≥ 100 m) e curva (raio 40 m, 45°–90°, para os dois lados); jogador, câmera, obstáculos, moedas, chão/trilhos/dormentes, prédios e postes seguem `FrameAt(s)`; trens (e rampas) só em retas — no spawn, o que cairia numa curva vira placa |
| Animações baseadas em Δt | ✅ `ComputeFrameDt()` em `main.cpp` (limitado a `MAX_DT`); animações acumulam `dt`; trens em movimento: `velocity * dt` (Tarefa 4.3) |
| Rampas para subir nos trens (SPEC) | ✅ Tarefa 4.4: metade dos trens parados ganha uma rampa (8 m, 0 → 3.2 m) colada à frente; o jogador sobe correndo, anda no teto, pula de um teto para outro ou troca de faixa para o teto vizinho, e cai no fim do trem ou ao trocar para uma faixa vazia; a câmera sobe junto com o chão (não com o pulo) (veja "Rampas e tetos") |
| Trens em movimento (SPEC: velocidade dos trens) | ✅ Tarefa 4.3: ~1 em 3 trens nasce móvel (`MOVING_TRAIN_SPEED` = 8 m/s na direção do jogador), com ≥ 3 s de aviso e sem quebrar a garantia de faixa livre; para ao chegar ao começo da reta onde nasceu ou a 0.5 m de outro obstáculo da faixa; colisão varre também a caixa do obstáculo (veja "Trens móveis") |
| Objetivo e lógica de controle não-triviais | ⚠️ parcial — corrida infinita com obstáculos gerados por chunks (sempre há faixa livre), pular/rolar/desviar, moedas, pontuação e game over com reinício; faltam menu, pausa e power-ups |
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
passos além da duração ou se a janela fechar antes do fim. Opcionais por cenário: `setup`
(configura `g_Game` antes do 1º quadro — `SetupNoSpawn` desliga o spawn de obstáculos para
cenários que não podem terminar em colisão) e `frame_hook` (registra estados intermediários).
Cenários atuais: `smoke`, `lane_jump_roll` (faixa, pulo, rolamento; sem spawn), `long_run`
(60 s de corrida; sem spawn), `obstacle_types` (close-up de cada obstáculo e das moedas),
`coins` (fileira de boas-vindas coletada), `lighting` (Blinn-Phong com spawn), `showcase`
(arranjo fixo com um exemplar de cada obstáculo, 2 trens e moedas, + foto do pulo — imagem para
o README/vídeo), `showcase_props` (power-ups e porco voador em exposição: `show_props = true`
faz `main.cpp` chamar `DrawShowcaseProps()`), `game_over` (bate sem entrada) e `restart` /
`restart_space` (Enter/Espaço reiniciam; Espaço não vira pulo), `curve` (jogador posto 24 m
antes da 1ª curva, troca de faixa dentro dela; fotos de entrada, meio e saída),
`curve_obstacles` (barreiras, placa e moedas dentro da curva e trem na reta seguinte),
`moving_train` / `moving_train_hit` (trem móvel posto na faixa do jogador: ele desvia e o trem passa
ao lado / não desvia e bate no instante previsto pela cinemática), `tunnel`
(atravessa o 1º túnel com um trem parado na faixa -1 e troca de faixa lá dentro; confere altura
da câmera antes/dentro/depois, transições sem salto, folga até o teto e o escurecimento) e
`tunnel_side` (portal visto de longe, saindo da curva anterior). **Não fixe valores de s ligados
à semente**: os cenários de curva/túnel acham o segmento com `FindFirstSegment()` (no `setup`, ou
numa cópia de `g_Game.track` em `BuildScenarios()` quando os tempos dependem do comprimento).

### Ao adicionar um novo arquivo `.cpp`

É preciso registrá-lo em **dois** lugares independentes:

1. a lista `SOURCES` no topo de `CMakeLists.txt` (o CMake aborta com `FATAL_ERROR` se um arquivo
   listado não existir);
2. a linha do `g++` em `Makefile.macOS` (ela lista os fontes explicitamente).

`Makefile.macOS` já lista `src/correcao.cpp` (onde `Correcao_KeyCallback()` está definida).
(Não verificado por build local no macOS.)

---

## Arquitetura

`src/main.cpp` (~1800 linhas) cuida de janela, shaders, texturas, VAOs, desenho e callbacks. A
lógica sem OpenGL fica em módulos testados por `tests/logic_tests.cpp`: `track.cpp` (pista e
referencial `FrameAt(s)`), `game.cpp` (estado, jogador, regras, spawn de obstáculos/moedas por
chunks, colisões, pontuação; global `g_Game` em `main.cpp`), `collisions.cpp` (todos os testes
de intersecção),
`camera.cpp` (`ThirdPersonCamera`: pose look-at suavizada no referencial da pista) e
`scenery.cpp` (faixas de chão/dormentes/trilhos, prédios, postes e peças/lâmpadas dos túneis do
trecho visível `[s-20, s+150]`, sem prédios/postes junto aos túneis; as faixas começam na grade de 1 m em s para o chão facetado das curvas não
"deslizar" entre quadros). Os demais
`.cpp` são bibliotecas de apoio: `textrendering.cpp` (texto na janela via `include/dejavufont.h`),
`tiny_obj_loader.cpp`, `stb_image.cpp` e `glad.c` são apenas *wrappers* de header-only libs,
`autotest.cpp` é o harness E2E e `correcao.cpp` é infraestrutura de correção automatizada.

**Faixas dinâmicas:** chão, dormentes e trilhos não são OBJ: `DynamicStripMesh` (em `main.cpp`)
cria VAO e VBOs uma vez com capacidade máxima (`GL_DYNAMIC_DRAW`), e a cada quadro `scenery.cpp`
gera os vértices em coordenadas de mundo e `UpdateDynamicStripMesh()` os reenvia com
`glBufferSubData`; o desenho é `GL_TRIANGLE_STRIP` com `model` = identidade, todas as faixas num
único `glMultiDrawArrays` (os dormentes são ~3.4k faixas de 4 vértices, uma por face). O chão tem
2 texturas: a faixa 0 (leito, brita) e as faixas 1–2 (terreno) são desenhadas separadas
(`first_strip`/`strip_count`). Trilhos = topo + 2 laterais (`AppendWallStrip`) sobre os dormentes
(`RAIL_HEIGHT` 0.18 m, `SLEEPER_HEIGHT` 0.06 m).

### Pipeline de carga de geometria

```
LoadObjToScene("../../data/x.obj", merge_as = NULL)
  → ObjModel(...)                       // tinyobjloader; lança se algum shape estiver sem nome
  → ComputeNormals(&model)              // gera normais por vértice se o OBJ não trouxer
  → BuildTrianglesAndAddToVirtualScene  // cria VAO + VBOs + índices, calcula a AABB
  → g_VirtualScene["<nome do shape>"]   // std::map<std::string, SceneObject>
  → DrawObject("<nome do shape>", ...)  // textura + uniforms + DrawVirtualObject (glDrawElements)
```

A chave de `g_VirtualScene` é o **nome do objeto dentro do arquivo OBJ** (`coin`,
`barrier_low`, `torso`, `the_cube`...), não o nome do arquivo — **exceto** com `merge_as`: todos os
shapes do arquivo viram um único objeto com esse nome (usado nos modelos da Kenney, que repetem o
nome do grupo e usam nomes genéricos como `wheels-front` em vários arquivos: `train_front`,
`building_a`, `lamp_post`...). Use `FindSceneObject()`/`ObjectSize()`, que
abortam com mensagem se o nome não existir (nunca `g_VirtualScene[nome]`, que insere um objeto
vazio em silêncio). Um OBJ com vários shapes vira várias entradas
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
- **Uniforms**: `model`, `view`, `projection`, `object_id`, `uv_mode`, `uv_scale`, `bbox_min`, `bbox_max` e os de
  iluminação (`sun_direction`, `sun_color`, `ambient_color`, `num_point_lights`, `point_light_position[8]`,
  `point_light_color[8]`, `shading_mode`, `material_kd_tint`, `material_ks`, `material_q`, `material_emissive`,
  e dos túneis: `num_tunnels`, `tunnel_start[4]`, `tunnel_forward[4]`, `tunnel_shape`) têm seus
  locations cacheados nos globais `g_*_uniform` em `LoadShadersFromFiles()`. Um uniform novo
  precisa ser declarado no shader, ter o global criado e o `glGetUniformLocation()` adicionado ali.
- **IDs de objeto e modos de UV**: as constantes `const int` (`PLAYER`, `GROUND`, ..., `LAMP_POLE` e
  `UV_OBJ`/`UV_PLANAR_XZ`/`UV_BBOX_XY`/`UV_SPHERICAL`/`UV_BOX`) vivem em `include/object_ids.h` e são
  **copiadas** para o topo de `shader_fragment.glsl` (a sintaxe é a mesma em C++ e GLSL) — alterar
  um exige alterar o outro. Não são `#define` porque `BARRIER_LOW`, `SIGN`, `TRAIN` etc. também
  são nomes de `ObstacleType` (`game.h`). O fragment
  shader usa `uv_mode` para gerar U,V (UV do OBJ, planar XZ em coordenadas de mundo, planar XY
  normalizado pela AABB, ou esférico) e multiplica por `uv_scale`. O "visual" de cada tipo de objeto
  (tom sobre a textura, Ks, q, emissivo, Phong/Gouraud) **não** é ramificado por `object_id` no
  shader: `MaterialFor(object_id)` em `main.cpp` devolve o `Material` e `ApplyMaterial()` o envia
  como uniforms a cada desenho (`DrawObject()`/`DrawDynamicStripMesh()`).

**Iluminação** (Tarefa 3.1): Blinn-Phong `Kd*(ambiente + Σ I·max(0,n·l)) + Ks*Σ I·max(0,n·h)^q`
(`h = normalize(l+v)`), com o sol (`sun_direction` aponta da superfície PARA o sol) e as
`MAX_POINT_LIGHTS` (8) lâmpadas de poste mais próximas do jogador (`ComputeLamps()` +
`PickNearestPoints()` em `scenery.cpp`; atenuação `1/(1 + 0.09d + 0.032d²)`). A função `BlinnPhong()`
está **duplicada** em `shader_vertex.glsl` (Gouraud: `gouraud_diffuse`/`gouraud_specular` por vértice)
e `shader_fragment.glsl` (Phong): GLSL não tem include, então os dois blocos devem ficar textualmente
idênticos. As cabeças dos postes (`LAMP`) são emissivas.

**Túneis** (Tarefa 4.2): `Track` gera segmentos retos com `tunnel = true` (veja `track.h`);
`TunnelOverlaps()` tira prédios e postes de rua de perto deles. As medidas da seção (`TUNNEL_*` em
`scenery.h`) são as mesmas de `tools/gen_models.py` (`tunnel_section.obj`, `tunnel_portal.obj`):
alterar nos dois. O túnel não tem piso (o chão é o leito: sem Z-fighting). O shader recebe os
túneis visíveis (`ComputeVisibleTunnels()`) e `SkyVisibility(p)` (no bloco duplicado de
`BlinnPhong`) corta o sol e reduz o ambiente para `tunnel_shape.w` dentro do volume — a
"sombra" do túnel, visível também de fora. As luminárias do teto (`ComputeTunnelLamps()`, bloco
emissivo `LAMP`) entram na mesma escolha das 8 luzes mais próximas que os postes. Com a CÂMERA
(em `s - g_Camera.Back()`) no túnel, `g_TunnelDimming` (rampa linear de `StepTunnelDimming()`)
multiplica `sun_color` por até 0.2 e `ambient_color` por até 0.6.

**Obstáculos e moedas**: `Game` gera um chunk de `CHUNK_LENGTH` por vez (até `s + SPAWN_AHEAD`)
e descarta o que fica `DESPAWN_BEHIND` para trás. Cada obstáculo é uma `AABB` em coordenadas
de pista; `ObstacleParts()` devolve as caixas sólidas (a barreira alta tem viga + 2 postes) e é
usada **tanto na colisão quanto no desenho**. As medidas vêm de **uma única tabela**,
`OBSTACLE_SIZE` em `game.h` (extensões padrão do `Game`); `DrawObstacle()` escala o modelo do tipo
para a união das partes, então a AABB do modelo desenhado **coincide** com a caixa de colisão (os
modelos procedurais já são feitos nessas medidas; o trem são 3 carros em fila, escala
≈ 2.0 × 2.2 × 1.7, com o **teto plano** dos carros — `TRAIN_MODEL_ROOF_Y`, abaixo de pequenas
nervuras que passam 0.2 m da caixa — no topo da caixa, onde o jogador anda). Mudou uma medida? Mude só `OBSTACLE_SIZE` (e, se for um modelo procedural,
`tools/gen_models.py`). A colisão usa a caixa varrida do jogador (antes ∪ depois do passo) contra
tunneling. `spawn_enabled = false` desliga a geração (testes de física).

**Trens móveis (Tarefa 4.3)**: `Obstacle.velocity < 0` (`-MOVING_TRAIN_SPEED`) anda o trem
`velocity * dt` por quadro (`Game::MoveObstacles`, antes das colisões; só `velocity < 0` anda); a
caixa e `s` andam juntos. O trem para de vez (`velocity = 0`) quando a ponta dianteira
(`box.min.z`) passaria de `Obstacle.min_s` (começo da sequência de retas/túneis onde nasceu,
`Track::StraightRunStart`, guardado no spawn) ou entraria na folga `TRAIN_STOP_GAP` de outro
obstáculo da mesma faixa à frente. A colisão varre também o obstáculo (caixa antes ∪ depois do
passo): a velocidade de fechamento chega a 38 m/s. **Spawn** (`GenerateChunk` + `CanSpawnMoving`):
só trens (já garantidos em reta) e só se `MovingRandom01() < MOVING_TRAIN_CHANCE` — gerador próprio
`moving_rng_`, então a sequência de tipos/posições dos obstáculos NÃO muda — com
`s - player.s >= (speed + 8) * MOVING_TRAIN_WARNING` e a faixa do trem **não segura em nenhum chunk**
de `[ponta mais baixa que ele alcança antes do jogador − 16 m, s + metade do comprimento]`
(`SafeLaneOfChunk`): assim quem segue a faixa segura de cada chunk nunca cruza com ele, mesmo que
ele entre em chunks anteriores. Se alguma condição falha, o trem nasce parado. Garantias testadas
em `logic_tests` (`Test_MovingTrainsSpawnAndStayPassable`: piloto que segue a faixa segura em 30
sementes × 5 km nunca bate em placa/trem).

**Rampas e tetos (Tarefa 4.4)**: `RAMP` (mesma largura/altura do trem em `OBSTACLE_SIZE`) fica
sempre colada à frente (lado de s menor) de um `TRAIN` **parado** da mesma faixa
(`Game::MakeRampFor`). Spawn: cada trem parado sorteia `RampRandom01() < RAMP_CHANCE` (gerador
próprio `ramp_rng_`, sorteio sempre feito: o resto do spawn não muda — `ramps_enabled = false` só
para o teste que confere isso) e só ganha rampa se ela cabe no chunk do trem (entra na emenda livre
do começo dele; ela nunca mata, então a troca de faixa ali continua possível), fora de túnel e sem
túnel até `RAMP_TUNNEL_CLEARANCE` (20 m) depois do fim do trem — quem está no teto nunca entra num
túnel. Física (`Game::MoveVertically`): o "chão" é o apoio mais alto sob a pegada do jogador entre
leito (0), rampas (`RampHeightUnder`: altura na frente da pegada, então os pés chegam a 3.2 m
exatamente na emenda com o trem) e tetos em que ele pousou (`LandsOnTop`, tolerância
`ROOF_TOLERANCE`; no ar só com `vy <= 0`). Em pé, segue o apoio (sobe a rampa); sem apoio (fim do
trem, faixa vazia), cai. A rampa nunca mata, e a **lateral dela é parede**
(`Game::BlockRampSides` + `RampSideBlocks`): uma troca de faixa que entraria de lado na rampa
acima de `RAMP_STEP_HEIGHT` (0.4 m) dos pés é desfeita (para encostado por fora, faixa alvo volta
para a de onde veio, altura igual); no começo baixo da rampa entra e sobe. No
teste de morte o trem em cujo teto ele está (ou em pé na altura do teto, sobre ele) é ignorado e os demais trens só matam abaixo de
`teto - ROOF_TOLERANCE` (senão a caixa varrida de quem sai pela lateral do teto encostaria nele).
No túnel a cabeça é limitada pela abóbada (`TunnelCeilingHeight` + `CeilingPenetration`).
`Game::GroundHeight()` (apoio sob os pés, ignorando o arco do pulo) ergue a câmera
(`ThirdPersonCamera::Update(..., ground_y)`, suavizada). Modelo: `data/procedural/ramp.obj` com
`ramp_frame` (metal_plate) e `ramp_deck` (tábuas, rough_wood), desenhados com a AABB de
`ramp_frame`; o topo das tábuas é a mesma reta de `RampHeightAt`. E2E: `ramp_train`, `roof_hop`,
`ramp_side_block`.

**Texturas**: há **uma** unidade de textura (0, sampler `TextureImage0`). `LoadTextureImage()`
devolve o id da textura (`GL_REPEAT` em S e T, parâmetros guardados na própria textura, sem sampler
object) e não depende mais da ordem das chamadas; os ids ficam em `g_Tex` (`main.cpp`). Quem desenha usa
`DrawObject(nome, model, object_id, textura, uv_mode, uv_scale, tint)`, que liga a textura na unidade 0,
envia os uniforms (o `tint` multiplica o Kd do material: tons dos prédios) e chama
`DrawVirtualObject()`. Para textura repetida (chão, paredes) use
`uv_scale > 1` ou `UV_PLANAR_XZ`. Para caixas escaladas de forma não uniforme (prédios) use
`UV_BOX`: projeção por face em metros do objeto (escala lida das colunas de `model`), com
`uv_scale` = repetições por metro — os tijolos não esticam.

**Assets (Tarefa 3.2)** — fontes e licenças em `data/FONTES.txt`, todos CC0:
- `data/kenney/<kit>/`: OBJ + MTL + `Textures/*.png` + `License.txt` da Kenney, **sem modificação**
  (UV para uma "colormap"; desenhar com `UV_OBJ`). Para trocar/adicionar um modelo da Kenney,
  copie OBJ+MTL+textura do pacote e carregue com `merge_as`.
- `data/procedural/`: gerados por `tools/gen_models.py` (só stdlib; `python tools/gen_models.py`
  na raiz regera tudo, inclusive `atlas.png`). **Não edite os OBJ à mão**: edite o script. Convenção:
  metros, base em y = 0, frente para +Z, objetos nomeados, normais e UV em todos os vértices,
  triângulos anti-horários (o script corrige pela normal). Obstáculos têm exatamente as medidas de
  `OBSTACLE_SIZE`.
- Personagem: partes `leg-left`, `leg-right`, `torso`, `arm-left`, `arm-right`, `head`;
  `DrawPlayer()` gira braços/pernas em torno do topo da AABB de cada parte (`g_RunPhase` acumula
  `speed·dt`). O modelo olha para +Z, por isso `PLAYER_MODEL_YAW = π`.
- Postes: `light-curved` com a cabeça em `LAMP_MODEL_HEAD` (coordenadas do modelo);
  `LampHeadPosition()` dá o centro da cabeça no mundo, usado **tanto** para a luz pontual quanto
  para o bloco emissivo `LAMP`.
- `bunny.obj`, `sphere.obj` e `plane.obj` não são mais carregados; `red_brick` (Poly Haven) é a
  textura dos túneis (os OBJ do túnel têm UV em metros/2, não usam o atlas).

### Matemática e câmera

Além da proibição do item 6, note que `near`/`far` são **negativos** no sistema de coordenadas da
câmera (`nearplane = -0.3f`, `farplane = -250.0f`; o near maior dá precisão ao Z-buffer para
dormentes/trilhos rentes ao chão), e que as matrizes de `matrices.h` são escritas em row-major no código mas
armazenadas column-major.

A câmera é a `ThirdPersonCamera` (`include/camera.h`): `Snap(track, s, x, in_tunnel)` no início
da corrida e `Update(track, s, x, in_tunnel, dt)` a cada quadro; `Pose()` dá posição/view/up para
`Matrix_Camera_View()`. Fica sobre a pista 6 m atrás (em s − 6) e 3.5 m acima (4 m e 2.2 m no
túnel), olha para o ponto da pista em s + 4 (+ up·1) — nas curvas o view vector é a corda entre
os dois pontos e gira sem tranco nas junções —, segue 1/3 do deslocamento lateral e suaviza com
`1 - exp(-6·dt)` no referencial da pista. Objetos na pista usam a matriz
`T(frame.position + right·x + up·y) · R_y(heading) · <modelo>`; `Matrix_OnGround()` encaixa a
AABB de um OBJ com a base em y = 0. `PushMatrix()`/`PopMatrix()` implementam uma pilha de
matrizes de modelagem para hierarquias de objetos.

**Câmera livre (Tarefa 5.1, `FreeCamera` em `camera.h/.cpp`):** `C` (em `HandleKey()`) alterna
`PLAYING` ↔ `FREE_CAMERA`. Ao entrar, `StartAt(g_Camera.Pose())` e o cursor é capturado
(`GLFW_CURSOR_DISABLED`, movimento bruto se houver; o 1º evento após a captura é ignorado); ao sair,
`g_Camera.Snap(...)` restaura a 3ª pessoa e o jogo continua de onde parou (`Game::Update` só avança
em `PLAYING`; moedas, trens e passada também ficam parados). O mouse gira sem precisar de botão
(`Look(dx, dy)`, 0.003 rad/px, yaw = convenção de heading da pista, pitch ±89°); `Move((direita,
cima, frente), dt)` a 10 m/s, normalizado, com "frente" incluindo o pitch e "cima" = Y do mundo.
As teclas seguradas ficam em `g_KeyHeld[]` (atualizada por `HandleKey()` em PRESS/RELEASE, vale
também no E2E) e o movimento é aplicado todo quadro com `dt`. Limitação aceita: o cenário só é
gerado ao redor do jogador (≈ −20…+150 m), então voar para longe mostra o vazio; o escurecimento do
túnel usa a posição da câmera livre (`PointInsideTunnel`, em `collisions.cpp`: ponto de mundo dentro da abóbada, com `TunnelCeilingHeight`). O E2E injeta o mouse pelos campos
`mouse_dx/mouse_dy` de `AutotestStep` (cenário `free_camera`).

### Estado e entrada

Todo o estado da aplicação são variáveis globais `g_*` mutadas pelos callbacks do GLFW (o estado
do jogo fica em `g_Game`). Controles atuais (só ao pressionar, sem repetição): `←`/`→` ou
`A`/`D` trocam de faixa; `↑`, `W` ou `Espaço` pulam; `↓` ou `S` rolam; no GAME OVER, `Enter`
(ou `Espaço`) reinicia a corrida; `H` liga e desliga o texto informativo (velocidade, faixa,
FPS — o placar de pontos/moedas/distância fica sempre visível); `C` liga/desliga a câmera livre
(jogo pausado; com ela ativa: mouse olha, `W`/`A`/`S`/`D` movem, `Espaço` sobe, `Shift` desce,
`C` volta ao jogo); `R` recarrega shaders; `ESC` fecha. Fora da câmera livre o mouse não faz
nada. Todo atalho precisa acabar documentado no manual dentro
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
