# Rail Rush — Plano de Implementação

> **Para agentes:** use superpowers:subagent-driven-development ou superpowers:executing-plans para
> executar este plano tarefa a tarefa. Os passos usam checkbox (`- [ ]`) para acompanhamento —
> marque `[x]` ao concluir, no mesmo commit da tarefa.

**Objetivo:** transformar o visualizador do código base no jogo *Rail Rush* descrito em `SPEC.md`
(corrida infinita em 3 faixas, com obstáculos, moedas, power-ups, pista curva, túneis, câmera livre,
Bézier, partículas e GUI), cumprindo todos os critérios de `ordem-trabalho.md`.

**Arquitetura:** a lógica de jogo (pista, jogador, obstáculos, colisões, Bézier, partículas) vive
em módulos C++ **sem dependência de OpenGL**, testáveis por um executável de testes separado.
`main.cpp` continua dono do contexto OpenGL, do carregamento de recursos e do desenho: a cada
frame ele chama `Game::Update(dt)` e depois desenha o estado do jogo. Todo objeto do mundo é
posicionado em **coordenadas de pista** `(lateral, altura, s)` — `s` é a distância percorrida ao
longo da pista — e convertido para coordenadas de mundo pelo referencial local da pista
(`Track::FrameAt(s)`). Assim pista curva, colisões e faixas usam a mesma matemática em retas e
curvas.

**Stack:** C++11, OpenGL 3.3 core, GLFW, GLAD, GLM (só tipos), tinyobjloader, stb_image,
Dear ImGui (GUI, licença MIT).

**Spec:** [`SPEC.md`](../SPEC.md) (congelado) e [`ordem-trabalho.md`](../ordem-trabalho.md).
Regras do repositório: [`CLAUDE.md`](../CLAUDE.md).

---

## Restrições globais

Valem para **todas** as tarefas; quebrar qualquer uma delas zera ou desconta nota.

- Commits com código de IA começam com `IA: ` e o prompt vai para `PROMPTS.md` no mesmo commit.
  Nunca misturar código humano e de IA no mesmo commit.
- Os dois integrantes (Gabriel e Bruno) precisam ter commits. Alternem quem executa cada tarefa.
- IA não escreve `SPEC.md` nem `README.md`.
- Proibido: `glm::lookAt/ortho/perspective/pickMatrix/rotate/scale/translate` e `glu*`. Usar
  `include/matrices.h`.
- Somente OpenGL de baixo nível. ImGui é biblioteca de GUI (permitida, sugerida pelo enunciado).
- Todos os testes de intersecção ficam em `src/collisions.cpp`.
- Todo código copiado leva comentário `// FONTE: <url> (licença)`. Todo asset novo entra em
  `data/FONTES.txt` com link e licença que permita redistribuição.
- Todo movimento (jogador, câmeras, trens, partículas, `t` da Bézier) é multiplicado por `dt`.
- Arquivo `.cpp` novo → registrar em `SOURCES` do `CMakeLists.txt` **e** na linha `g++` do
  `Makefile.macOS`.
- `Correcao_KeyCallback()` continua sendo a primeira linha de `KeyCallback()`.
- O executável roda de `bin/<plataforma>/` (caminhos `../../data/...`).

## Foco de revisão

Situações que o SPEC não testa explicitamente mas que um avaliador vai encontrar na demo, em ordem
de probabilidade. Cada uma tem um teste na tarefa dona do código.

1. **Padrão de obstáculos impossível** — as três faixas bloqueadas sem pulo/rolamento possível.
   Esperado: todo chunk gerado tem pelo menos uma faixa transponível. (Tarefa 2.2)
2. **Pouso em cima do trem vira game over** — jogador que vem da rampa ou cai de um pulo sobre o
   teto do trem deve aterrissar, não morrer. Só bater de frente mata. (Tarefa 4.4)
3. **Frame longo (janela arrastada, breakpoint, carregamento)** — `dt` enorme atravessa obstáculos
   ou dispara a câmera. Esperado: `dt` limitado a `0.05 s`. (Tarefa 0.1)
4. **Trocar de faixa durante a curva ou dentro do túnel** — jogador não pode sair da pista nem
   atravessar a parede do túnel; faixa continua -1..1. (Tarefas 1.2, 4.1)
5. **Coletar power-up do mesmo tipo já ativo / dois power-ups ao mesmo tempo** — renova a duração,
   não duplica efeito, e o cofrinho não spawna um segundo porco enquanto o primeiro voa.
   (Tarefa 5.2)

---

## Estrutura de arquivos

| Arquivo | Responsabilidade | Depende de GL? |
|---|---|---|
| `include/game_config.h` | Constantes de gameplay (faixas, velocidades, gravidade, durações) | não |
| `include/collisions.h`, `src/collisions.cpp` | AABB, esfera, rampa, pouso — **todos** os testes de intersecção | não |
| `include/bezier.h`, `src/bezier.cpp` | Avaliação de Bézier cúbica e tangente | não |
| `include/track.h`, `src/track.cpp` | Segmentos (reta/curva/túnel), referencial `FrameAt(s)`, geração de faixas de malha do chão | não |
| `include/game.h`, `src/game.cpp` | Estados (menu, jogando, pausa, câmera livre, game over), jogador, spawn, pontuação, power-ups | não |
| `include/camera.h`, `src/camera.cpp` | Câmera 3ª pessoa (com ajuste no túnel) e câmera livre; devolvem posição/view vector | não |
| `include/particles.h`, `src/particles.cpp` | Pool de partículas, emissão e atualização | não |
| `src/main.cpp` | Janela, shaders, texturas, VAOs, desenho, callbacks, ImGui | sim |
| `src/shader_vertex.glsl`, `src/shader_fragment.glsl` | Blinn-Phong (Phong e Gouraud), mapeamento de textura | sim |
| `third_party/imgui/` | Dear ImGui + backends GLFW/OpenGL3, com `LICENSE.txt` | sim |
| `tests/logic_tests.cpp` | Testes com `assert` dos módulos sem GL; alvo CMake `logic_tests` | não |

### Convenções compartilhadas (decididas aqui, usadas em todas as fases)

- **Coordenadas de pista:** `glm::vec3 (x = lateral, y = altura, z = s)`. Faixa `i ∈ {-1,0,1}` →
  `x = i * LANE_WIDTH`. Jogador avança em `+s`.
- **Heading:** ângulo `h` em torno de Y; `forward = (-sin h, 0, -cos h)`,
  `right = (cos h, 0, -sin h)`. Com `h = 0` o jogador corre para `-Z` do mundo.
- **Model matrix de algo na pista:**
  `Matrix_Translate(frame.position + frame.right*x + up*y) * Matrix_Rotate_Y(frame.heading) * <rotação/escala do modelo>`.
- **Testes:** `tests/logic_tests.cpp` tem um `main()` que chama funções `Test_*()` com `assert` e
  imprime `OK`. Rodar com `cmake --build build --target logic_tests && ./bin/<plat>/logic_tests`
  (no Windows, `bin/Debug/logic_tests.exe`). Não entra no `Makefile.macOS`.
- **Verificação visual:** toda tarefa que mexe em desenho termina com "compilar, rodar de
  `bin/<plataforma>/`, conferir X". Sem erro de GL no console e sem crash.

### Constantes (`include/game_config.h`)

```cpp
const float LANE_WIDTH          = 2.5f;   // m
const float PLAYER_START_SPEED  = 12.0f;  // m/s
const float PLAYER_MAX_SPEED    = 30.0f;
const float PLAYER_ACCEL        = 0.15f;  // m/s por segundo de jogo
const float LANE_SWITCH_SPEED   = 12.0f;  // m/s lateral
const float GRAVITY             = 30.0f;  // m/s²
const float JUMP_VELOCITY       = 11.0f;  // ~2 m de altura
const float ROLL_DURATION       = 0.6f;   // s
const float PLAYER_HEIGHT       = 1.8f;
const float PLAYER_ROLL_HEIGHT  = 0.8f;
const float PLAYER_HALF_WIDTH   = 0.4f;
const float PLAYER_HALF_DEPTH   = 0.4f;
const float MAX_DT              = 0.05f;  // s
const float CHUNK_LENGTH        = 30.0f;  // m por bloco de spawn
const float SPAWN_AHEAD         = 150.0f; // m gerados à frente do jogador
const float DESPAWN_BEHIND      = 20.0f;  // m mantidos atrás do jogador
const float COIN_RADIUS         = 0.5f;
const float COIN_HEIGHT         = 1.0f;
const float POWERUP_DURATION    = 10.0f;  // s
const float MAGNET_RADIUS       = 6.0f;
const float SNEAKERS_JUMP_MULT  = 1.5f;
const float PIGGY_FLIGHT_TIME   = 4.0f;   // s para t ir de 0 a 1
const float PIGGY_COIN_INTERVAL = 0.15f;  // s entre moedas soltas
```

---

## Cronograma

| Fase | Entrega | Prazo-alvo |
|---|---|---|
| 0 — Fundação | Δt, IDs, texturas por objeto, testes unitários e E2E, limpeza do lab | 02/11 |
| 1 — Corrida jogável | Pista infinita reta, jogador com faixas/pulo/rolamento, câmera 3ª pessoa | **03/11 (avaliação parcial 04/11)** |
| 2 — Regras do jogo | `collisions.cpp`, obstáculos, moedas instanciadas, pontuação, game over | 09/11 |
| 3 — Iluminação e texturas | Blinn-Phong, Gouraud nas moedas, luzes pontuais, assets finais | **10/11 (avaliação parcial 11/11)** |
| 4 — Pista curva, túneis e trens | Segmentos curvos, túneis com câmera adaptativa, trens móveis, rampas | 20/11 |
| 5 — Câmera livre e power-ups | Tecla C, ímã, tênis, multiplicador, cofrinho com Bézier | 26/11 |
| 6 — Funcionalidade extra | Partículas na coleta, GUI ImGui (menu, pausa, game over, HUD) | 01/12 |
| 7 — Polimento e entrega | Bugs visuais, builds nas 3 plataformas, apresentação | 06/12 |

---

## Fase 0 — Fundação

### Tarefa 0.1: Laço com Δt, limpeza do laboratório e alvo de testes

**Arquivos:**
- Modificar: `src/main.cpp` (globais `g_AngleZ`, `g_ForearmAngle*`, `g_TorsoPosition*` e seus
  callbacks; laço principal)
- Criar: `include/game_config.h`, `tests/logic_tests.cpp`
- Modificar: `CMakeLists.txt` (novo alvo `logic_tests`), `Makefile.macOS` (adicionar
  `src/correcao.cpp`, divergência já documentada no `CLAUDE.md`)

**Interfaces:**
- Produz: constantes de `game_config.h`; `float ComputeFrameDt(double now, double* last)` em
  `main.cpp` (retorna `min(now - *last, MAX_DT)` e atualiza `*last`); alvo `logic_tests`.

- [x] **Passo 1:** criar `tests/logic_tests.cpp` com `main()` que só imprime `OK`, e o alvo
  `add_executable(logic_tests tests/logic_tests.cpp)` no CMake (mesmo `include/`, sem GLFW).
- [x] **Passo 2:** `cmake -B build -S . && cmake --build build --target logic_tests` e rodar —
  esperado: imprime `OK`.
- [x] **Passo 3:** criar `game_config.h` com as constantes da seção acima.
- [x] **Passo 4:** em `main.cpp`, remover as variáveis/controles de laboratório sem efeito
  (`g_AngleZ`, `g_ForearmAngle*`, `g_TorsoPosition*`, teclas X/Y/Z) e substituir
  `glfwGetTime()` nas animações por `dt` de `ComputeFrameDt`.
- [x] **Passo 5:** compilar e rodar `main`; esperado: cena do lab funcionando, sem warnings novos.
- [x] **Passo 6:** commit `IA: adiciona dt por frame, alvo de testes e remove restos do lab` +
  entrada em `PROMPTS.md`.

### Tarefa 0.2: IDs de objeto, texturas por objeto e modo de UV

Hoje a textura é escolhida pela *unidade* (ordem de carga) e o modo de UV pelo `object_id`. Com
~15 objetos isso não escala. Passa a existir **uma** unidade de textura (0) com a textura do objeto
ligada antes de cada desenho, e um uniform `uv_mode` que diz como gerar UV.

**Arquivos:**
- Criar: `include/object_ids.h`
- Modificar: `src/main.cpp` (`LoadTextureImage`, `DrawVirtualObject`, `LoadShadersFromFiles`),
  `src/shader_fragment.glsl`
- Criar: `data/cube.obj` (cubo unitário centrado na origem, objeto `the_cube`, com `vt` e `vn`) —
  placeholder até os modelos finais

**Interfaces:**
- Produz:
  - `GLuint LoadTextureImage(const char* filename)` — devolve o id da textura; sampler com
    `GL_REPEAT` em S e T.
  - `void DrawObject(const char* name, const glm::mat4& model, int object_id, GLuint texture, int uv_mode, float uv_scale = 1.0f)`
    — envia `model`, `object_id`, `uv_mode`, `uv_scale`, liga `texture` na unidade 0 e chama
    `DrawVirtualObject`.
  - `#define UV_OBJ 0`, `UV_PLANAR_XZ 1` (mundo, para chão/paredes sem esticar), `UV_BBOX_XY 2`
    em `object_ids.h` **e** no topo do fragment shader (manter em sincronia). Implementação
    acrescentou `UV_SPHERICAL 3` (esfera sem UV no OBJ).
  - IDs em `object_ids.h`: `PLAYER, GROUND, RAIL, BUILDING, BARRIER_LOW, BARRIER_HIGH, SIGN,
    TRAIN, RAMP, TUNNEL, COIN, POWERUP_MAGNET, POWERUP_SNEAKERS, POWERUP_MULTIPLIER,
    POWERUP_PIGGY, FLYING_PIG, PARTICLE` — espelhados no shader.

- [x] **Passo 1:** implementar as interfaces acima; desenhar coelho, esfera, plano e `the_cube`
  via `DrawObject` com três texturas diferentes.
- [x] **Passo 2:** compilar, rodar, apertar `R` — esperado: os 4 objetos texturizados, plano com
  textura repetida (`uv_scale = 4`) sem esticar.
- [x] **Passo 3:** atualizar a seção "Texturas" do `CLAUDE.md` (o mapeamento por ordem de carga
  deixou de existir).
- [x] **Passo 4:** commit `IA: textura por objeto, uv_mode e ids compartilhados`.

### Tarefa 0.3: Harness E2E (`--autotest`)

Testes ponta a ponta rodando o executável real: ele abre a janela, injeta uma sequência de teclas
com tempo simulado, salva screenshots e sai com código de status. Todas as fases seguintes
acrescentam cenários aqui.

**Arquivos:**
- Criar: `include/autotest.h`, `src/autotest.cpp` (builds), `include/stb_image_write.h`
  (FONTE: https://github.com/nothings/stb — domínio público/MIT; comentário `FONTE` no topo),
  `tests/run_e2e.sh` (roda cada cenário a partir de `bin/<plataforma>/` e falha se algum sair ≠ 0).
- Modificar: `src/main.cpp` (parse de `--autotest <cenário>`; o laço consulta o harness),
  `src/stb_image.cpp` ou novo wrapper para `STB_IMAGE_WRITE_IMPLEMENTATION`.

**Interfaces:**
- Produz:
  ```cpp
  struct AutotestStep { float time; int key; int action; const char* screenshot; };  // key/action no formato GLFW; screenshot pode ser NULL
  struct AutotestScenario { const char* name; float duration; std::vector<AutotestStep> steps;
                            bool (*final_check)(std::string* why); };          // NULL = só "não crashou, sem erro de GL"
  bool  Autotest_Begin(int argc, char* argv[]);   // true se --autotest <nome> válido
  bool  Autotest_Active();
  float Autotest_FixedDt();                        // 1/60 — no autotest o dt é fixo
  void  Autotest_AfterFrame(GLFWwindow* w, float sim_time);  // injeta teclas via a mesma função que KeyCallback usa, tira screenshots, checa glGetError
  ```
  Screenshots em `bin/<plataforma>/e2e/<cenário>_<rótulo>.png` (`glReadPixels` do back buffer
  antes do swap, invertido verticalmente). Ao fim imprime `E2E <nome> OK` e sai com `0`; falha
  imprime `E2E <nome> FAIL: <motivo>` e sai com `1`. **Nunca** usar códigos 100–109 (correção
  automatizada). A janela continua visível (ler o framebuffer de janela oculta é indefinido).
- Cenário inicial `smoke`: 3 s de simulação, screenshot em 1 s, sem erro de GL.

- [x] **Passo 1:** implementar; `tests/run_e2e.sh` roda `smoke`.
- [x] **Passo 2:** rodar; esperado: `E2E smoke OK`, exit 0, PNG legível mostrando a cena.
- [x] **Passo 3:** commit `IA: harness E2E com cenários scriptados e screenshots`.

**A partir daqui, toda tarefa que muda comportamento visível acrescenta pelo menos um cenário E2E**
(por exemplo, `lane_jump_roll` na 1.3, `game_over` na 2.2, `curve` na 4.1, `free_camera` na 5.1,
`piggy` na 5.3, `menu_pause` na 6.2) com `final_check` sobre o estado do `Game` e screenshots
para inspeção visual.

---

## Fase 1 — Corrida jogável (meta: avaliação parcial de 04/11)

### Tarefa 1.1: Pista (somente retas) e referencial `FrameAt`

**Arquivos:** criar `include/track.h`, `src/track.cpp`; testes em `tests/logic_tests.cpp`;
registrar `src/track.cpp` nos dois builds e no alvo de testes.

**Interfaces:**
- Produz:
  ```cpp
  struct TrackSegment { float s_start, length, curvature; bool tunnel;
                        glm::vec4 start_position; float start_heading; };
  struct TrackFrame   { glm::vec4 position, forward, right; float heading; };
  class Track {
  public:
      explicit Track(unsigned seed);
      void EnsureGeneratedUntil(float s);   // anexa segmentos até cobrir s
      void DiscardBefore(float s);          // descarta segmentos que terminam antes de s
      TrackFrame FrameAt(float s) const;
      bool IsTunnelAt(float s) const;
      const TrackSegment* SegmentAt(float s) const;
      // Faixa de malha entre lateral [x0,x1] na altura y, de s0 a s1 em passos de step.
      // Gera 2 vértices por passo (triangle strip). UV: u = (x - x0)/(x1 - x0) * u_repeat, v = s / v_tile.
      void BuildStrip(float s0, float s1, float step, float x0, float x1, float y,
                      float u_repeat, float v_tile,
                      std::vector<float>* positions, std::vector<float>* normals,
                      std::vector<float>* uvs) const;
  };
  ```
  `curvature = 1/R` (0 = reta; sinal positivo = curva à esquerda). Nesta fase só gera retas
  (`curvature = 0`, `tunnel = false`, comprimento 60 m).

- [x] **Passo 1: testes**
  ```cpp
  void Test_TrackStraightFrame() {
      Track t(1); t.EnsureGeneratedUntil(200);
      TrackFrame f = t.FrameAt(10.0f);
      assert(near(f.position, vec4(0,0,-10,1)));
      assert(near(f.forward,  vec4(0,0,-1,0)));
      assert(near(f.right,    vec4(1,0,0,0)));
  }
  void Test_TrackDiscardKeepsFrames() {
      Track t(1); t.EnsureGeneratedUntil(500); t.DiscardBefore(300);
      assert(near(t.FrameAt(400).position, vec4(0,0,-400,1)));
  }
  void Test_StripVertexCount() {  // 0..10 com passo 1 → 11 pares
      Track t(1); t.EnsureGeneratedUntil(50);
      std::vector<float> p, n, uv; t.BuildStrip(0,10,1,-4,4,0,3,4,&p,&n,&uv);
      assert(p.size() == 11*2*4 && uv.size() == 11*2*2);
  }
  ```
  (`near` com tolerância `1e-4`, definido no arquivo de testes.)
- [x] **Passo 2:** rodar `logic_tests` — esperado: falha de link/compilação (Track inexistente).
- [x] **Passo 3:** implementar `track.cpp`. Fórmula fechada por segmento, com `u = s - s_start`,
  `h = start_heading + curvature*u`:
  - reta: `p = p0 + u * forward(h0)`
  - curva: `x = x0 + (cos h - cos h0)/k`, `z = z0 - (sin h - sin h0)/k`
  O próximo segmento começa no `FrameAt` do fim do anterior.
- [x] **Passo 4:** rodar `logic_tests` — esperado: `OK`.
- [x] **Passo 5:** commit `IA: pista procedural com referencial local`.

### Tarefa 1.2: Jogador, faixas, pulo e rolamento

**Arquivos:** criar `include/game.h`, `src/game.cpp` (registrar nos builds e nos testes).

**Interfaces:**
- Consome: `Track` (1.1), `game_config.h`.
- Produz:
  ```cpp
  enum class GameMode { MENU, PLAYING, PAUSED, FREE_CAMERA, GAME_OVER };
  struct Player { int lane; float x, y, vy, s, speed, roll_timer; bool grounded; };
  enum class Input { LEFT, RIGHT, JUMP, ROLL };
  class Game {
  public:
      explicit Game(unsigned seed);
      void Reset();                       // volta a PLAYING do início
      void HandleInput(Input in);
      void Update(float dt);              // só avança em PLAYING
      GameMode mode; Player player; Track track; float elapsed;
  };
  ```
  Regras: `lane` limitado a [-1,1]; `x` anda até `lane*LANE_WIDTH` a `LANE_SWITCH_SPEED`;
  pulo só com `grounded`; rolar no ar força `vy = -JUMP_VELOCITY` (desce rápido) e inicia
  `roll_timer`; pular cancela rolamento; `speed` cresce `PLAYER_ACCEL` por segundo até o máximo.

- [x] **Passo 1: testes**
  ```cpp
  void Test_LaneClamp()   { Game g(1); g.HandleInput(Input::LEFT); g.HandleInput(Input::LEFT);
                            assert(g.player.lane == -1); }
  void Test_LaneMoveTime(){ Game g(1); g.HandleInput(Input::RIGHT);
                            for (int i=0;i<60;i++) g.Update(1/60.f);
                            assert(fabs(g.player.x - LANE_WIDTH) < 1e-4); }
  void Test_JumpLands()   { Game g(1); g.HandleInput(Input::JUMP); g.Update(0.01f);
                            assert(g.player.y > 0);
                            for (int i=0;i<200;i++) g.Update(0.01f);
                            assert(g.player.y == 0 && g.player.grounded); }
  void Test_NoDoubleJump(){ Game g(1); g.HandleInput(Input::JUMP); g.Update(0.1f);
                            float vy = g.player.vy; g.HandleInput(Input::JUMP);
                            assert(g.player.vy == vy); }
  void Test_SpeedCap()    { Game g(1); for (int i=0;i<100000;i++) g.Update(MAX_DT);
                            assert(g.player.speed == PLAYER_MAX_SPEED); }
  void Test_DtIndependent(){ Game a(1), b(1);
                             for (int i=0;i<60;i++)  a.Update(1/60.f);
                             for (int i=0;i<240;i++) b.Update(1/240.f);
                             assert(fabs(a.player.s - b.player.s) < 0.01f); }
  ```
- [x] **Passo 2:** rodar — falha; **Passo 3:** implementar; **Passo 4:** rodar — `OK`.
- [x] **Passo 5:** commit `IA: jogador com troca de faixa, pulo e rolamento`.

### Tarefa 1.3: Câmera 3ª pessoa e desenho da corrida

**Arquivos:** criar `include/camera.h`, `src/camera.cpp`; modificar `src/main.cpp`.

**Interfaces:**
- Consome: `Game`, `Track::FrameAt`, `Track::BuildStrip`, `DrawObject` (0.2).
- Produz:
  ```cpp
  struct CameraPose { glm::vec4 position, view_vector, up; };
  class ThirdPersonCamera {
  public:
      void Snap(const TrackFrame& player_frame, float player_x, bool in_tunnel);
      void Update(const TrackFrame& player_frame, float player_x, bool in_tunnel, float dt);
      CameraPose Pose() const;
  };
  ```
  Alvo fora do túnel: `back = 6.0`, `height = 3.5`; dentro: `back = 4.0`, `height = 2.2`.
  Suavização exponencial `pos += (alvo - pos) * (1 - exp(-6*dt))`. Olha para
  `jogador + forward*4 + up*1`. Usa só 1/3 do deslocamento lateral do jogador (a câmera não
  "pula" de faixa).

- [x] **Passo 1:** em `main.cpp`: `Game g_Game(seed)`, setas e `W/A/S/D` → `HandleInput`;
  projeção com `nearplane = -0.3f`, `farplane = -250.0f` (evita Z-fighting com far grande).
- [x] **Passo 2:** VAO dinâmico `"track_ground"` (`GL_DYNAMIC_DRAW`, `GL_TRIANGLE_STRIP`)
  reconstruído por frame com `BuildStrip(s-20, s+150, 1, -4.5, 4.5, 0, 1, 4)`; e 6 faixas de
  trilho (2 por faixa, largura 0.12, `y = 0.03`).
- [x] **Passo 3:** desenhar o jogador (coelho/cubo provisório) com a matriz de pista; prédios
  provisórios (`the_cube` com tijolo) como **instâncias** a cada 12 m em `x = ±9`.
- [x] **Passo 4:** compilar e rodar. Esperado: personagem corre sozinho, troca de faixa suave,
  pula e rola; chão e trilhos sem buracos nem Z-fighting; câmera estável a 30 e 144 Hz.
- [x] **Passo 5:** commit `IA: corrida jogável com câmera em terceira pessoa`.
- [x] **Passo 6:** **checkpoint da avaliação parcial** — compilar do zero (`rm -rf build`) na
  máquina que vai para o laboratório e rodar.

---

## Fase 2 — Regras do jogo

### Tarefa 2.1: `collisions.cpp`

**Arquivos:** criar `include/collisions.h`, `src/collisions.cpp` (builds + testes).

**Interfaces (todas em coordenadas de pista):**
```cpp
struct AABB { glm::vec3 min, max; };
AABB  MakeAABB(glm::vec3 center, glm::vec3 half_extents);
bool  AABBIntersectsAABB(const AABB& a, const AABB& b);
bool  SphereIntersectsAABB(glm::vec3 center, float radius, const AABB& box);
bool  SphereIntersectsSphere(glm::vec3 c1, float r1, glm::vec3 c2, float r2);
// Altura da rampa (sobe linear de min.y em min.z até max.y em max.z) se (x, s) está sobre ela.
bool  RampHeightAt(const AABB& ramp, float x, float s, float* out_height);
// Jogador vindo de cima: base anterior >= topo do obstáculo - tolerância.
bool  LandsOnTop(const AABB& player_prev, const AABB& player_now, const AABB& obstacle, float tolerance);
```

- [x] **Passo 1: testes** — `Test_AABBTouchingIsHit` (faces encostadas contam como colisão),
  `Test_AABBSeparatedInS`, `Test_SphereCornerMiss` (esfera perto do canto mas fora → `false`),
  `Test_SphereInside`, `Test_RampMidHeight` (rampa `min.y=0,max.y=2`, `s` no meio → `1.0`),
  `Test_RampOutsideLane` (x fora → `false`), `Test_LandsOnTopFromAbove`,
  `Test_LandsOnTopFrontalHitIsFalse`.
- [x] **Passo 2:** falha; **Passo 3:** implementar; **Passo 4:** `OK`.
- [x] **Passo 5:** commit `IA: testes de intersecção em collisions.cpp`.

### Tarefa 2.2: Obstáculos, moedas, pontuação e game over

**Arquivos:** modificar `include/game.h`, `src/game.cpp`, `src/main.cpp`.

**Interfaces:**
- Consome: `collisions.h` (2.1).
- Produz:
  ```cpp
  enum class ObstacleType { BARRIER_LOW, BARRIER_HIGH, SIGN, TRAIN, RAMP };
  struct Obstacle { ObstacleType type; int lane; float s, velocity; AABB box; };  // box em pista, já posicionada
  struct Coin     { int lane; float s, y; bool collected; };
  // em Game:
  std::vector<Obstacle> obstacles; std::vector<Coin> coins;
  int coins_collected; float score; int multiplier;
  AABB PlayerBox() const;   // altura PLAYER_ROLL_HEIGHT se roll_timer > 0
  ```
  Spawn por chunks de `CHUNK_LENGTH` até `player.s + SPAWN_AHEAD`; remove o que ficou mais de
  `DESPAWN_BEHIND` atrás. Regras de colisão: BARRIER_LOW (altura 0.9) passa pulando;
  BARRIER_HIGH (vão 0–1.1 livre) passa rolando; SIGN e TRAIN só desviando. Colisão com obstáculo
  → `mode = GAME_OVER`. Moeda: `SphereIntersectsAABB` → coletada, `+1 coin`, `score += 10*multiplier`.
  `score += speed*dt` continuamente.
  Dimensões dos obstáculos vêm das AABBs dos modelos (`SceneObject.bbox_*` escaladas) — passar
  para o `Game` via `void SetObstacleExtents(ObstacleType, glm::vec3 half_extents)` chamado pelo
  `main.cpp` depois de carregar os OBJ.

- [x] **Passo 1: testes** — `Test_EveryChunkHasPassableLane` (gerar 500 chunks com seeds 1..20;
  para cada `s` de chunk, pelo menos uma faixa sem SIGN/TRAIN), `Test_HitBarrierGameOver`,
  `Test_JumpOverLowBarrier`, `Test_RollUnderHighBarrier`, `Test_CoinCollectedOnce`,
  `Test_DespawnBehind`.
- [x] **Passo 2:** falha; **Passo 3:** implementar; **Passo 4:** `OK`.
- [x] **Passo 5:** desenhar obstáculos e moedas como instâncias (um VAO por tipo, uma `model` por
  instância); moedas girando com `dt`. Texto provisório de pontuação com `TextRendering_PrintString`.
  Tecla `Espaço`/`Enter` em GAME_OVER chama `Reset()`.
- [x] **Passo 6:** rodar: bater, pular e rolar funcionam como descrito; sem colisão "fantasma".
- [x] **Passo 7:** commit `IA: obstáculos, moedas e game over`.

---

## Fase 3 — Iluminação e texturas (meta: avaliação parcial de 11/11)

### Tarefa 3.1: Blinn-Phong por fragmento, Gouraud nas moedas e luzes pontuais

**Arquivos:** `src/shader_vertex.glsl`, `src/shader_fragment.glsl`, `src/main.cpp`.

**Uniforms novos** (declarar no shader, global `g_*_uniform`, `glGetUniformLocation` em
`LoadShadersFromFiles`):
`vec4 sun_direction`, `vec3 sun_color`, `vec3 ambient_color`, `int num_point_lights`,
`vec4 point_light_position[8]`, `vec3 point_light_color[8]`, `int shading_mode`
(`0 = Phong`, `1 = Gouraud`), `vec3 material_ks`, `float material_q`.

- Blinn-Phong: `Kd*(ambiente + Σ I·max(0,n·l)) + Ks*Σ I·max(0,n·h)^q`, `h = normalize(l+v)`;
  luzes pontuais com atenuação `1/(1 + 0.09d + 0.032d²)`.
- Gouraud (`shading_mode == 1`): o vertex shader calcula difuso+ambiente e especular e passa
  `out vec3 gouraud_diffuse, gouraud_specular`; o fragment faz `Kd(textura)*gouraud_diffuse + gouraud_specular`.
  O cálculo de luz fica duplicado nos dois shaders (GLSL não tem include) — manter idênticos.
- C++: luzes pontuais = as 8 lâmpadas mais próximas do jogador (por enquanto, postes a cada 20 m).

- [x] **Passo 1:** implementar; moedas com `shading_mode = 1`, resto `0`.
- [x] **Passo 2:** rodar e apertar `R` após cada ajuste. Esperado: brilho especular visível no
  trem/moedas, moedas com destaque facetado (Gouraud), lâmpadas iluminando o chão ao redor.
- [x] **Passo 3:** commit `IA: Blinn-Phong com Phong e Gouraud e luzes pontuais`.

### Tarefa 3.2: Assets finais (modelos e texturas)

Escolher assets cuja licença permita redistribuição (CC0 / CC-BY). Na execução automatizada, a
IA pode baixar assets **CC0** (preferência) com link direto e registrá-los em `data/FONTES.txt`;
o que não for obtenível assim vira malha procedural gerada por script versionado
(`tools/gen_models.py`, commit `IA:`), com UVs próprias. A dupla pode trocar qualquer asset depois. Fontes sugeridas: Poly Haven (texturas CC0), Kenney e Quaternius (modelos CC0),
Poly Pizza (filtrar por CC0/CC-BY). Modelos e texturas devem ser **originais da fonte**, nada
extraído do jogo de referência.

| Objeto | Malha | Textura |
|---|---|---|
| Personagem | humanoide low-poly | própria do modelo |
| Chão | `BuildStrip` | cascalho/terra (Poly Haven) |
| Trilhos | `BuildStrip` | metal |
| Prédios | modelo de prédio ou cubo | tijolo (já existe `red_brick`) |
| Barreiras, placa | modelos | próprias |
| Trem, rampa | modelos | próprias |
| Túnel | arco/meio-cilindro | pedra (`rocky_terrain_02` já existe) |
| Moeda | cilindro achatado | ouro |
| Power-ups (ímã, tênis, ×2, cofrinho), porco voador | modelos | próprias |

- [ ] **Passo 1:** baixar para `data/`, registrar cada um em `data/FONTES.txt` (link + licença).
  Se algum arquivo não puder ir para o GitHub, anotar para o `README.md` (escrito pela dupla).
- [ ] **Passo 2:** garantir que cada OBJ tem objeto nomeado (tinyobjloader lança exceção sem nome)
  e escalar via `Matrix_Scale` usando a bbox, não editando o arquivo.
- [ ] **Passo 3:** trocar placeholders; conferir que nenhuma textura está esticada.
- [ ] **Passo 4:** commits separados: assets (humano) e código de carga (`IA:` se gerado).

---

## Fase 4 — Pista curva, túneis e trens

### Tarefa 4.1: Segmentos curvos

**Arquivos:** `src/track.cpp`, `src/game.cpp`, testes.

- Gerador alterna: reta 60–150 m, curva com `|curvature| = 1/40` e ângulo total 45°–90°, sinal
  aleatório. Túneis, trens e rampas **só** em retas (garantia do gerador); em curvas só barreiras,
  placas e moedas.
- [ ] **Passo 1: testes** — `Test_CurveContinuity` (para cada junção de segmentos,
  `FrameAt(s_fim - 1e-3)` ≈ `FrameAt(s_fim + 1e-3)` em posição e heading),
  `Test_Curve90EndsPerpendicular` (curva de 90° à esquerda vira `forward` para `-X`),
  `Test_NoTrainOnCurve` (seeds 1..20: todo TRAIN/RAMP está em segmento com `curvature == 0`).
- [ ] **Passo 2:** falha; **Passo 3:** implementar; **Passo 4:** `OK`.
- [ ] **Passo 5:** rodar: chão, trilhos e prédios acompanham a curva sem buracos; dá para trocar
  de faixa no meio da curva.
- [ ] **Passo 6:** commit `IA: trechos curvos da pista`.

### Tarefa 4.2: Túneis e câmera adaptativa

- Segmento reto com `tunnel = true` (40–80 m). Desenhar o modelo de túnel como instâncias a cada
  4 m; lâmpadas pontuais no teto a cada 10 m alimentando a 3.1; entrada do túnel com luz do sol
  atenuada (`sun_color` multiplicado por 0.2 quando a câmera está dentro).
- Câmera já recebe `in_tunnel` (1.3) — verificar que a transição é suave nos dois sentidos.
- [ ] **Passo 1:** teste `Test_TunnelOnlyOnStraight`; implementar; `OK`.
- [ ] **Passo 2:** rodar: câmera abaixa ao entrar e volta ao sair, sem atravessar o teto.
- [ ] **Passo 3:** commit `IA: túneis com iluminação e câmera adaptativa`.

### Tarefa 4.3: Trens móveis

- `Obstacle.velocity < 0` (≈ -8 m/s) desloca `s` por `velocity*dt`. Se o trem chegaria ao fim da
  reta onde nasceu, para (`velocity = 0`). Spawn de trem móvel exige ≥ 3 s de antecedência visível
  (`s_spawn - player.s ≥ (speed - velocity) * 3`).
- [ ] **Passo 1:** testes `Test_MovingTrainAdvances`, `Test_MovingTrainStopsAtSegmentEnd`.
- [ ] **Passo 2:** implementar; `OK`; rodar; commit `IA: trens em movimento`.

### Tarefa 4.4: Rampas e andar sobre trens

- RAMP sempre colado à frente de um TRAIN estático na mesma faixa. Altura do chão do jogador =
  max(0, `RampHeightAt`, topo de trem se `LandsOnTop`). Cair do fim do trem volta a ter gravidade.
- [ ] **Passo 1: testes** — `Test_RampLiftsPlayerOntoTrain` (correr pela faixa da rampa: termina
  em cima do trem, sem GAME_OVER), `Test_JumpOntoTrainRoof` (pulo a partir da rampa e pouso no
  meio do trem), `Test_FallOffTrainEnd`, `Test_FrontalTrainHitIsGameOver`.
- [ ] **Passo 2:** implementar; `OK`; rodar; commit `IA: rampas e corrida sobre os trens`.

---

## Fase 5 — Câmera livre e power-ups

### Tarefa 5.1: Câmera livre (tecla C)

**Interfaces:**
```cpp
class FreeCamera {
public:
    void StartAt(const CameraPose& from);         // começa onde a 3ª pessoa estava
    void Look(float dx_pixels, float dy_pixels);   // yaw/pitch; pitch limitado a ±89°
    void Move(glm::vec3 local_dir, float dt);      // (direita, cima, frente), 10 m/s
    CameraPose Pose() const;
};
```
- `C` alterna `PLAYING ↔ FREE_CAMERA` (jogo congelado: `Game::Update` não avança). `WASD` move,
  `Espaço`/`Shift` sobe/desce, mouse com botão esquerdo pressionado gira. Ao voltar, a 3ª pessoa
  faz `Snap`.
- [ ] **Passo 1:** testes `Test_FreeCamPitchClamp`, `Test_FreeCamMoveDt` (mesma distância com
  dt 1/30 e 1/120). Implementar; `OK`.
- [ ] **Passo 2:** rodar: dá para dar a volta no cenário pausado; ao sair o jogo continua de onde
  parou. Commit `IA: câmera livre com jogo pausado`.

### Tarefa 5.2: Power-ups clássicos

- Tipos: **Ímã** (moedas a menos de `MAGNET_RADIUS` voam até o jogador, `SphereIntersectsSphere`),
  **Tênis** (pulo × `SNEAKERS_JUMP_MULT`), **Multiplicador** (`multiplier = 2`) — todos por
  `POWERUP_DURATION`; e **Cofrinho** (5.3). Spawn raro (≈ 1 a cada 6 chunks), nunca na mesma
  posição de obstáculo.
- Produz em `Game`: `struct PowerUp { PowerUpType type; int lane; float s; bool collected; };`,
  `float powerup_timer[4]`.
- [ ] **Passo 1: testes** — `Test_SameTypeRenewsTimer`, `Test_MagnetPullsCoin`,
  `Test_SneakersHigherJump`, `Test_MultiplierDoublesCoinScore`, `Test_PowerupExpires`.
- [ ] **Passo 2:** implementar; `OK`; rodar; commit `IA: power-ups ímã, tênis e multiplicador`.

### Tarefa 5.3: Cofrinho e porco voador na Bézier cúbica

**Arquivos:** criar `include/bezier.h`, `src/bezier.cpp` (builds + testes); `src/game.cpp`.

**Interfaces:**
```cpp
glm::vec3 CubicBezier(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t);
glm::vec3 CubicBezierTangent(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t);
struct FlyingPig { bool active; float t, coin_timer; glm::vec3 p[4]; };  // em Game
```
- Pontos de controle em coordenadas de pista **relativas ao jogador** (`z` somado a `player.s`):
  `P0=(-8,8,10)`, `P1=(-2,12,25)`, `P2=(4,12,35)`, `P3=(8,8,45)`. `t += dt / PIGGY_FLIGHT_TIME`.
  A cada `PIGGY_COIN_INTERVAL` solta uma moeda em `s` absoluto atual, na faixa mais próxima de `x`,
  altura `COIN_HEIGHT`. Orientação do porco pela tangente (heading = `atan2` da tangente). Coletar
  outro cofrinho com o porco ativo não cria um segundo porco.
- [ ] **Passo 1: testes** — `Test_BezierEndpoints` (`t=0 → P0`, `t=1 → P3`),
  `Test_BezierMidpoint` (`t=0.5` = `(P0+3P1+3P2+P3)/8`), `Test_PigDropsCoins`
  (após `PIGGY_FLIGHT_TIME` de updates: ~26 moedas, todas com `lane ∈ [-1,1]`),
  `Test_SinglePigActive`.
- [ ] **Passo 2:** implementar; `OK`; rodar: porco faz caminho curvo suave no céu e deixa rastro;
  commit `IA: cofrinho com porco voador em curva de Bézier cúbica`.

---

## Fase 6 — Funcionalidade extra

### Tarefa 6.1: Sistema de partículas na coleta

**Arquivos:** criar `include/particles.h`, `src/particles.cpp` (builds + testes); `src/main.cpp`.

**Interfaces:**
```cpp
struct Particle { glm::vec4 position, velocity; float life, max_life, size; glm::vec3 color; };
class ParticleSystem {
public:
    explicit ParticleSystem(size_t capacity);              // 512
    void Emit(glm::vec4 world_pos, int count, glm::vec3 color, unsigned seed);
    void Update(float dt);                                  // gravidade 6 m/s², remove life <= 0
    const std::vector<Particle>& Alive() const;
};
```
- `Game` registra eventos de coleta (`std::vector<CollectEvent> events`, esvaziado pelo
  `main.cpp` a cada frame) com posição em mundo; `main.cpp` chama `Emit` (30 para moeda, 80 para
  power-up), velocidade inicial aleatória em hemisfério, vida 0.4–0.9 s.
- Desenho: quad billboard (model matrix montada com `right`/`up` da câmera), textura de brilho
  com alpha, `glEnable(GL_BLEND)` aditivo, `glDepthMask(GL_FALSE)`, desenhado depois dos opacos.
  Alpha e tamanho decaem com `life/max_life` (uniform `particle_fade`).
- [ ] **Passo 1: testes** — `Test_ParticlesExpire`, `Test_CapacityRespected` (emitir 2000 →
  `Alive().size() <= 512`), `Test_ParticlesMoveWithDt`.
- [ ] **Passo 2:** implementar; `OK`; rodar: explosão dourada ao pegar moeda; commit
  `IA: sistema de partículas na coleta`.

### Tarefa 6.2: GUI com ImGui (menu, pausa, game over, HUD)

**Arquivos:** `third_party/imgui/` (core + `imgui_impl_glfw` + `imgui_impl_opengl3` + `LICENSE.txt`);
`CMakeLists.txt` e `Makefile.macOS` (fontes e include dir); `src/main.cpp`.

- Cada arquivo do ImGui recebe no topo `// FONTE: https://github.com/ocornut/imgui (licença MIT)`.
  Usar uma release fixa e anotar a versão em comentário no `CMakeLists.txt`.
- `ImGui_ImplGlfw_InitForOpenGL(window, true)` **depois** de registrar nossos callbacks, para o
  ImGui encadear os nossos (o `Correcao_KeyCallback` continua primeiro no `KeyCallback`).
  Callbacks de jogo ignoram mouse/teclado quando `io.WantCaptureMouse/Keyboard`.
- Telas:
  - **MENU:** título, botões *Jogar*, *Controles*, *Sair*.
  - **PAUSED** (`P` ou `ESC`): *Continuar*, *Reiniciar*, *Menu*. `ESC` deixa de fechar o programa.
  - **GAME_OVER:** pontuação, moedas, distância, *Jogar de novo*, *Menu*.
  - **HUD** em PLAYING: pontuação, moedas, power-ups ativos com barra de tempo.
- [ ] **Passo 1:** compilar nos dois builds (CMake no Windows e Linux, Makefile no macOS).
- [ ] **Passo 2:** rodar: todos os botões funcionam com mouse; pausa congela tudo, inclusive
  partículas e porco. Commit `IA: GUI com menu, pausa, game over e HUD`.

---

## Fase 7 — Polimento e entrega

### Tarefa 7.1: Caça a bugs avaliados

- [ ] Z-fighting: trilho × chão, faces coplanares de prédios; testar olhando rasante.
- [ ] Texturas esticadas: todo objeto com `uv_mode` adequado.
- [ ] Colisão: jogar 10 partidas seguidas tentando cada obstáculo, rampa e pouso.
- [ ] Desempenho: ≥ 60 FPS na máquina do laboratório (texto de FPS já existe, tecla `H`).
- [ ] Partida longa (10 min) sem crash, sem vazamento (contagem de obstáculos/moedas estável).
- [ ] Todos os `TODO`, `printf` de debug e restos do lab removidos.

### Tarefa 7.2: Builds e documentação de apoio

- [ ] Build limpo em Windows (MinGW e MSVC se possível), Linux (CMake) e macOS (Makefile).
- [ ] `COMPILACAO.md` coerente com a realidade (dependências do ImGui, alvo `logic_tests`).
- [ ] `CLAUDE.md`: checklist de requisitos atualizado, controles atualizados.
- [ ] Atalhos finais listados para a dupla documentar no manual do `README.md`:
  `←/→` ou `A/D` faixa, `↑`/`W` pula, `↓`/`S` rola, `P`/`ESC` pausa, `C` câmera livre
  (`WASD`, `Espaço`/`Shift`, mouse), `H` info, `R` recarrega shaders.

### Tarefa 7.3: Entrega (humana)

- [ ] `README.md` escrito **pela dupla** (descrição, extra, contribuições, análise do uso de IA,
  ≥ 2 imagens, manual, compilação).
- [ ] Vídeo de 3–5 min; `metadados-entrega.json`; `git push` antes de pegar o hash final.

---

## Mapa SPEC → tarefa

| Item do SPEC / enunciado | Tarefa |
|---|---|
| Malhas complexas (personagem, obstáculos, ambiente, power-ups) | 3.2 |
| Transformações pelo usuário (faixa, pulo, rolamento) | 1.2 |
| Câmera look-at 3ª pessoa + ajuste no túnel | 1.3, 4.2 |
| Câmera livre com jogo pausado (C) | 5.1 |
| Instâncias (moedas, trens, placas, barreiras, túneis, power-ups, prédios) | 1.3, 2.2, 4.2, 5.2 |
| Intersecções em `collisions.cpp` (obstáculo, moeda, power-up, rampa) | 2.1, 2.2, 4.4, 5.2 |
| Blinn-Phong, sol + luzes pontuais, Phong e Gouraud | 3.1, 4.2 |
| Texturas em todos os objetos, ≥ 3 imagens | 0.2, 3.2 |
| Bézier cúbica (porco do cofrinho) | 5.3 |
| Δt em tudo (jogador, obstáculos, trens, câmera, faixa, pulo, rolamento, partículas, Bézier) | 0.1 e todas |
| Pista curva com troca de faixa | 4.1 |
| Velocidade aumenta com o tempo, game over ao colidir | 1.2, 2.2 |
| Extra: partículas na coleta + GUI (menu, pausa, game over) | 6.1, 6.2 |
| Mouse e teclado | 1.3, 5.1, 6.2 |
