# Brief — Tarefa 0.1 (Rail Rush)

Leia também: .superpowers/sdd/PLANO-IMPLEMENTACAO/env.md (como compilar/testar/commitar nesta máquina) e CLAUDE.md (regras do repositório).

## Contexto global do plano (vale para todas as tarefas)

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


## SUA TAREFA

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

- [ ] **Passo 1:** criar `tests/logic_tests.cpp` com `main()` que só imprime `OK`, e o alvo
  `add_executable(logic_tests tests/logic_tests.cpp)` no CMake (mesmo `include/`, sem GLFW).
- [ ] **Passo 2:** `cmake -B build -S . && cmake --build build --target logic_tests` e rodar —
  esperado: imprime `OK`.
- [ ] **Passo 3:** criar `game_config.h` com as constantes da seção acima.
- [ ] **Passo 4:** em `main.cpp`, remover as variáveis/controles de laboratório sem efeito
  (`g_AngleZ`, `g_ForearmAngle*`, `g_TorsoPosition*`, teclas X/Y/Z) e substituir
  `glfwGetTime()` nas animações por `dt` de `ComputeFrameDt`.
- [ ] **Passo 5:** compilar e rodar `main`; esperado: cena do lab funcionando, sem warnings novos.
- [ ] **Passo 6:** commit `IA: adiciona dt por frame, alvo de testes e remove restos do lab` +
  entrada em `PROMPTS.md`.

