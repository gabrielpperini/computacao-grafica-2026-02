#ifndef _GAME_H
#define _GAME_H

#include <random>
#include <vector>

#include <glm/vec3.hpp>

#include "collisions.h"
#include "game_config.h"
#include "track.h"

// Núcleo do jogo Rail Rush (estado, jogador e regras). Sem OpenGL: pode ser
// testado em tests/logic_tests.cpp. Power-ups das fases seguintes estendem
// esta mesma classe.

enum class GameMode { MENU, PLAYING, PAUSED, FREE_CAMERA, GAME_OVER };

enum class Input { LEFT, RIGHT, JUMP, ROLL };

// Coordenadas de pista: x = lateral (faixa i -> i*LANE_WIDTH), y = altura,
// s = distância percorrida ao longo da pista.
struct Player {
    int lane;          // faixa alvo, em [-1, 1]
    float x;           // posição lateral atual (anda até lane*LANE_WIDTH)
    float y;           // altura do pé do jogador (0 = chão)
    float vy;          // velocidade vertical
    float s;           // distância percorrida
    float speed;       // velocidade de avanço
    float roll_timer;  // tempo restante de rolamento (> 0 = rolando)
    bool grounded;
};

// ---- Obstáculos e moedas (Tarefa 2.2) ----
//
// Regras de passagem: BARRIER_LOW (até 0.9 m) só pulando; BARRIER_HIGH (viga
// acima de um vão livre de BARRIER_HIGH_CLEARANCE) só rolando; SIGN só
// desviando de faixa; TRAIN desviando ou por cima (Tarefa 4.4): só mata quem
// bate na frente/lateral abaixo do teto. RAMP (Tarefa 4.4) fica colada à
// frente de um TRAIN parado da mesma faixa, sobe do chão até o teto dele e
// nunca mata. TRAIN e RAMP só são gerados inteiros em retas da pista (4.1).
enum class ObstacleType { BARRIER_LOW, BARRIER_HIGH, SIGN, TRAIN, RAMP };
const int OBSTACLE_TYPE_COUNT = 5;

// Dimensões (m) de cada ObstacleType, na ordem do enum: (largura, altura,
// comprimento ao longo da pista). FONTE ÚNICA de verdade, usada pela colisão
// (Game, via extensões padrão) e pelo desenho (main.cpp escala cada modelo
// para que a AABB dele coincida com a caixa de colisão; os modelos de
// data/procedural já são feitos nessas medidas). Para BARRIER_HIGH a altura é
// a da viga, que começa em y = BARRIER_HIGH_CLEARANCE (vai de 1.1 a 3.0 m).
// O trem são 3 carros do Train Kit (Kenney) em fila: 13 m deixa a escala dos
// carros quase uniforme (2.0 x 2.2 x 1.7; a altura vai até o teto plano dos
// carros, onde o jogador anda, Tarefa 4.4). São floats simples (e não
// glm::vec3) para terem inicialização constante: o g_Game global de main.cpp
// é construído antes de main() e lê esta tabela.
const float OBSTACLE_SIZE[OBSTACLE_TYPE_COUNT][3] = {
    {2.0f, 0.9f, 0.3f},   // BARRIER_LOW: pula-se por cima
    {2.0f, 1.9f, 0.3f},   // BARRIER_HIGH: viga de y = 1.1 a 3.0, rola-se por baixo
    {2.0f, 3.0f, 0.3f},   // SIGN: placa de bloqueio
    {2.2f, 3.2f, 13.0f},  // TRAIN: 3 carros de metrô
    {2.2f, 3.2f, 8.0f},   // RAMP: sobe de 0 até a altura do TRAIN (mesma largura)
};

// box: caixa em coordenadas de pista, já posicionada (z = s). s = centro da
// caixa ao longo da pista. velocity (m/s ao longo da pista): 0 = parado; < 0 =
// trem em movimento, vindo na direção do jogador (Tarefa 4.3); a caixa anda
// junto com s. min_s: limite do movimento, a ponta dianteira (box.min.z) do
// trem móvel não passa daí (começo da reta onde ele nasceu; -1e9 = sem limite).
struct Obstacle { ObstacleType type; int lane; float s, velocity; AABB box; float min_s; };

// Moeda: esfera de raio COIN_RADIUS centrada em (lane*LANE_WIDTH, y, s). Com o
// ímã (Tarefa 5.2) a moeda vira "puxada" (pulled): passa a ter posição livre
// (px, py, ps), em coordenadas de pista, e voa até o jogador. As moedas soltas
// pelo porco voador (Tarefa 5.3) nascem na altura dele e caem (falling, com
// velocidade vertical vy) até COIN_HEIGHT. Os campos extras ficam depois de
// 'collected' para que {lane, s, y, false} continue valendo.
struct Coin { int lane; float s, y; bool collected; bool pulled; float px, py, ps; bool falling; float vy; };

// Centro da moeda em coordenadas de pista (x, y, s): o da faixa, ou a posição
// livre se ela está sendo puxada pelo ímã.
glm::vec3 CoinCenter(const Coin& c);

// ---- Power-ups (Tarefa 5.2) ----
//
// Ímã, tênis e multiplicador duram POWERUP_DURATION; pegar o mesmo tipo de
// novo só renova o tempo (não empilha). O cofrinho não tem duração: pegá-lo
// só registra o evento (Game::piggy_pickups), que o Update consome (Tarefa 5.3)
// para lançar o porco voador. Aparecem na faixa segura do chunk, a altura de
// moeda, nunca em cima de obstáculo ou moeda.
enum class PowerUpType { MAGNET, SNEAKERS, MULTIPLIER, PIGGY };
const int POWERUP_TYPE_COUNT = 4;
// Tipos com duração (todos menos o cofrinho, que é sempre o último do enum).
const int POWERUP_TIMED_TYPE_COUNT = POWERUP_TYPE_COUNT - 1;
static_assert((int)PowerUpType::PIGGY == POWERUP_TYPE_COUNT - 1, "PIGGY deve ser o último PowerUpType");

// Esfera de coleta de raio POWERUP_RADIUS centrada em (lane*LANE_WIDTH,
// POWERUP_HEIGHT, s).
struct PowerUp { PowerUpType type; int lane; float s; bool collected; };

// Raio de coleta e altura (m) do centro dos power-ups: maiores que os da moeda.
const float POWERUP_RADIUS = 0.6f;
const float POWERUP_HEIGHT = COIN_HEIGHT;
// Probabilidade de um chunk ter um power-up: em média 1 a cada 6 (spawn raro).
const float POWERUP_CHUNK_CHANCE = 1.0f / 6.0f;
// Folga (m) de s entre um power-up e o obstáculo ou a moeda mais próximos.
const float POWERUP_CLEARANCE = 1.5f;
// Velocidade (m/s) com que o ímã puxa uma moeda, somada à do jogador (assim a
// moeda alcança o jogador mesmo vindo de trás).
const float MAGNET_PULL_SPEED = 20.0f;

// ---- Porco voador (Tarefa 5.3) ----
//
// Ao pegar um cofrinho, um porco com asas voa numa curva de Bézier cúbica no
// céu e solta moedas pelo caminho. Os 4 pontos de controle p[] estão em
// coordenadas de pista RELATIVAS ao jogador (x lateral, y altura, z a
// distância à frente dele): a posição absoluta soma player.s ao z, então o
// porco acompanha o jogador. O parâmetro t anda com dt / PIGGY_FLIGHT_TIME;
// em t = 1 o porco some. Só há um porco por vez.
struct FlyingPig { bool active; float t, coin_timer; glm::vec3 p[4]; };

// Pontos de controle do voo (x, y, z relativos ao jogador). Floats simples (e
// não glm::vec3) para terem inicialização constante: o g_Game global de
// main.cpp é construído antes de main() e lê esta tabela no Reset.
// (Valores ajustados em relação ao plano, que subia a 8-12 m: ali o porco
// ficava na borda superior do quadro da câmera de perseguição e atrás dos
// prédios. Aqui ele voa de 5 a ~7 m de altura, dentro do quadro, e continua
// bem curvo na lateral (S: -5 -> 5) e na altura (arco 5 -> 7 -> 5).)
const float PIGGY_CONTROL[4][3] = {
    {-5.0f, 5.0f, 12.0f},
    {3.0f, 8.0f, 20.0f},
    {-3.0f, 8.0f, 28.0f},
    {5.0f, 5.0f, 36.0f},
};

// O voo inteiro (a curva vai até PIGGY_CONTROL[3][2] à frente do jogador, que
// anda até 4 s * PLAYER_MAX_SPEED) cabe em PIGGY_SPAWN_TUNNEL_CLEARANCE m a
// partir do cofrinho: ele só nasce se não houver túnel nesse trecho (o porco
// voa mais alto que a abóbada e as moedas cairiam através do teto).
const float PIGGY_SPAWN_TUNNEL_CLEARANCE = 160.0f; // m

// Câmera nos pulos altos: acima de CAMERA_JUMP_FREE_HEIGHT m do chão ela sobe
// CAMERA_JUMP_FOLLOW do excesso (o pulo comum, de ~2 m, não a move).
const float CAMERA_JUMP_FREE_HEIGHT = 2.2f; // m
const float CAMERA_JUMP_FOLLOW      = 0.8f;

// Teto do túnel na aproximação: a cabeça do jogador no ar não pode estar acima
// de (teto + TUNNEL_APPROACH_SLOPE * distância até a entrada), nos últimos
// TUNNEL_APPROACH_LENGTH metros antes dela. A folga some linearmente até a
// boca do túnel, onde vale o teto de verdade: o pulo (principalmente o do
// tênis) é achatado aos poucos, sem o tranco de uma correção de uma vez só.
const float TUNNEL_APPROACH_SLOPE  = 0.25f;
const float TUNNEL_APPROACH_LENGTH = 16.0f; // m

// Geração por chunks de CHUNK_LENGTH. Em cada chunk os obstáculos ficam só em
// [início + CHUNK_CLEAR_HEAD, fim - CHUNK_CLEAR_TAIL]: a emenda entre dois
// chunks (CHUNK_CLEAR_TAIL + CHUNK_CLEAR_HEAD = 16 m) é livre em todas as
// faixas e dá para atravessar duas faixas mesmo na velocidade máxima.
const float CHUNK_CLEAR_HEAD        = 10.0f; // m livres no começo de cada chunk
const float CHUNK_CLEAR_TAIL        = 6.0f;  // m livres no fim de cada chunk
const float OBSTACLE_FREE_START     = 40.0f; // m iniciais sem nenhum obstáculo
const float LOW_HIGH_MIN_GAP        = 6.0f;  // m mínimos entre barreira baixa e alta na mesma faixa
const float LANE_OBSTACLE_MARGIN    = 0.1f;  // folga mínima (m) entre obstáculo e borda da faixa
const float BARRIER_HIGH_CLEARANCE  = 1.1f;  // vão livre (m) sob a barreira alta
const float BARRIER_POST_HALF_WIDTH = 0.06f; // postes da barreira alta (m)
const float COIN_SPACING            = 2.0f;  // m entre moedas de uma fileira
const int   WELCOME_COIN_COUNT      = 8;     // fileira de moedas do 1º chunk (faixa 0)
// A pista é mantida um pouco além de DESPAWN_BEHIND para trás: o cenário
// começa as faixas na grade de 1 m (até 1 m antes) e FrameAt fora dos
// segmentos mantidos extrapolaria o primeiro deles (errado numa curva).
const float TRACK_KEEP_MARGIN       = 5.0f;  // m

// dt de um quadro, sanitizado (Tarefa 7.1a): NaN, zero e negativo viram 0 (a
// simulação não anda nem recua); o resto é limitado a MAX_DT. Usado pelo laço
// principal e pelo Game::Update.
float ClampFrameDt(float dt);

// Partes sólidas de um obstáculo (caixas em coordenadas de pista), usadas tanto
// na colisão quanto no desenho: a mesma geometria nos dois. A barreira alta tem
// 3 partes (viga = o.box e dois postes até o chão nas bordas); as demais, 1
// (o.box). Devolve o número de partes escritas em parts[0..2].
int ObstacleParts(const Obstacle& o, AABB parts[3]);

// ---- Eventos de coleta (Tarefa 6.1) ----
//
// Cada moeda ou power-up coletado registra um evento em Game::events, com a
// posição em coordenadas de PISTA (x lateral, y altura, z = s) do centro do que
// foi pego. Quem desenha (main.cpp) converte para o mundo, dispara as partículas
// e limpa a lista a cada quadro. O Game não conhece partículas nem OpenGL. Se
// ninguém consumir, a lista guarda no máximo COLLECT_EVENT_CAPACITY eventos (os
// seguintes são descartados), então nunca cresce nem realoca depois do reserve.
enum class CollectKind { COIN, POWERUP };
struct CollectEvent {
    CollectKind kind;
    PowerUpType powerup;  // só vale para kind == POWERUP
    glm::vec3 position;
};
const size_t COLLECT_EVENT_CAPACITY = 64;

class Game {
public:
    explicit Game(unsigned seed);

    // Volta ao início de uma corrida (mode = PLAYING), de forma determinística:
    // re-semeia os geradores, recria a pista com a mesma semente e zera
    // pontuação, moedas e obstáculos (que voltam a ser gerados no próximo
    // Update, iguais aos de uma corrida nova). spawn_enabled e as dimensões
    // dos obstáculos são configuração e são mantidos.
    void Reset();

    // Ignorado fora de PLAYING.
    void HandleInput(Input in);

    // Avança a simulação em dt segundos (limitado a [0, MAX_DT]). Só avança em
    // PLAYING. Ordem: avanço e troca de faixa do jogador; movimento dos trens
    // móveis; movimento vertical (Tarefa 4.4: o chão sob o jogador é o mais
    // alto entre o leito (0), as rampas sob a pegada e o teto dos trens em que
    // ele pousou; sem apoio, cai; no túnel a cabeça não passa da abóbada);
    // colisões com a caixa varrida do jogador e dos obstáculos que andaram
    // (obstáculo -> GAME_OVER, exceto rampas e o trem em cujo teto ele está;
    // o passo que termina em GAME_OVER para aqui: não coleta nem pontua (7.1a);
    // power-up -> coletado (Tarefa 5.2: os tempos correm no início do passo,
    // o ímã puxa as moedas antes da coleta); moeda -> coletada); pontuação; spawn de chunks até
    // player.s + SPAWN_AHEAD (se spawn_enabled) e descarte do que ficou mais
    // de DESPAWN_BEHIND para trás (a pista: + TRACK_KEEP_MARGIN).
    void Update(float dt);

    // Altura do chão sob o jogador (Tarefa 4.4): a do apoio mais alto sob a
    // pegada dele que não está acima dos pés (leito = 0, rampa, teto de trem).
    // No ar continua sendo a do que está embaixo (pulando do teto, o teto),
    // ignorando o arco do pulo. Usada pela câmera.
    float GroundHeight() const;

    // Altura que a câmera acompanha (Tarefa 5.2): GroundHeight() e, nos pulos
    // que passam de CAMERA_JUMP_FREE_HEIGHT acima do chão (os do tênis), a
    // fração CAMERA_JUMP_FOLLOW do que passou, para o jogador não sair do
    // quadro. O pulo comum (2 m) não mexe na câmera.
    float CameraTargetHeight() const;

    // Caixa do jogador em coordenadas de pista: altura PLAYER_ROLL_HEIGHT
    // rolando, PLAYER_HEIGHT em pé; base em player.y.
    AABB PlayerBox() const;

    // Semi-extensões de cada tipo de obstáculo (padrão: OBSTACLE_SIZE / 2;
    // os testes usam o setter para casos-limite). A largura é limitada a
    // LANE_WIDTH/2 - LANE_OBSTACLE_MARGIN e a profundidade ao miolo do chunk,
    // para manter a garantia de faixa livre. Vale para obstáculos criados
    // depois da chamada.
    void SetObstacleExtents(ObstacleType type, glm::vec3 half_extents);
    glm::vec3 ObstacleExtents(ObstacleType type) const;

    // Obstáculo do tipo na faixa, centrado em s, apoiado no chão (a barreira
    // alta começa em y = BARRIER_HIGH_CLEARANCE).
    Obstacle MakeObstacle(ObstacleType type, int lane, float s) const;

    // Trem (TRAIN) centrado em s com a velocity dada (< 0: anda para o
    // jogador, MOVING_TRAIN_SPEED). min_s = começo da sequência de retas onde
    // ele está (track.StraightRunStart): o trem para ao chegar lá. Se o trem
    // não cabe inteiro numa reta, ou velocity >= 0, sai parado (velocity 0).
    Obstacle MakeMovingTrain(int lane, float s, float velocity) const;

    // Rampa (RAMP) colada à frente (lado de s menor) do trem dado, na mesma
    // faixa: termina onde o trem começa e sobe até o teto dele.
    Obstacle MakeRampFor(const Obstacle& train) const;

    // Faixa segura (sem placa/trem) do chunk, ou -2 se o chunk ainda não foi
    // gerado. Trens móveis nunca nascem na faixa segura dos chunks que
    // percorrerão (veja GenerateChunk); os testes usam isto para conferir.
    int SafeLaneOfChunk(int chunk) const;

    // Gera os chunks que terminam até s (independe de spawn_enabled; o Update
    // só chama isto quando spawn_enabled).
    void GenerateChunksUntil(float s);

    // Gerador pseudoaleatório do spawn (semente derivada da do jogo, distinta
    // da semente da pista). Nunca usar rand().
    std::mt19937& Rng() { return rng_; }

    GameMode mode;
    Player player;
    Track track;
    float elapsed;  // tempo simulado em PLAYING (s)

    std::vector<Obstacle> obstacles; // ordem de geração: por chunk, depois por faixa (-1, 0, 1), com a rampa antes do trem; só POR FAIXA fica em ordem de s
    std::vector<Coin> coins;
    int coins_collected;
    float score;     // += distância percorrida; += 10*multiplier por moeda
    int multiplier;  // 1; 2 enquanto o power-up de multiplicador está ativo

    // Power-ups (Tarefa 5.2). powerup_timer[tipo] = segundos restantes (0 =
    // inativo; o cofrinho nunca tem tempo). piggy_pickups = cofrinhos pegos e
    // ainda não consumidos (o Update zera no passo seguinte, Tarefa 5.3).
    std::vector<PowerUp> powerups;  // ordenados por s (ordem de geração)
    float powerup_timer[POWERUP_TYPE_COUNT];
    int piggy_pickups;
    bool PowerUpActive(PowerUpType type) const { return powerup_timer[(int)type] > 0.0f; }

    // Coletas do(s) último(s) passo(s) ainda não consumidas (Tarefa 6.1; veja
    // CollectEvent). Quem consome as lê e chama events.clear(). Reset() limpa.
    std::vector<CollectEvent> events;

    // Porco voador (Tarefa 5.3). pig_coins_dropped = moedas soltas por ele
    // desde o Reset (as puladas por cair dentro de obstáculo não contam).
    FlyingPig pig;
    int pig_coins_dropped;
    // Posição do porco em coordenadas de pista (x, y, s ABSOLUTO) e sua
    // velocidade (m/s) no mesmo sistema: a derivada da curva relativa ao
    // jogador, dividida por PIGGY_FLIGHT_TIME, mais o avanço do jogador em s.
    // Só têm sentido com pig.active.
    glm::vec3 PigTrackPosition() const;
    glm::vec3 PigTrackVelocity() const;

    // false = não gera obstáculos/moedas (testes de física e cenários E2E que
    // não devem terminar em colisão). Colisões com os que já existem continuam.
    bool spawn_enabled;
    // false = trens parados nunca ganham rampa (só para testes: o resto do
    // spawn sai idêntico, as rampas têm gerador próprio). Mantido no Reset.
    bool ramps_enabled;
    // false = não gera power-ups (só para testes: eles têm gerador próprio, o
    // resto do spawn sai idêntico). Mantido no Reset.
    bool powerups_enabled;

private:
    void GenerateChunk(int index);
    // Anda os trens móveis em dt (param/colisão entre eles) e guarda em
    // obstacle_dz_ o deslocamento de cada obstáculo neste passo.
    void MoveObstacles(float dt);
    // O trem de comprimento 2*hz centrado em s pode nascer móvel? (aviso de
    // MOVING_TRAIN_WARNING s e faixa não segura em todos os chunks por onde
    // ele passará antes de encontrar o jogador.)
    bool CanSpawnMoving(int lane, float s, float hz, float min_s) const;
    float MovingRandom01();       // sorteio do trem móvel (gerador próprio)
    float RampRandom01();         // sorteio da rampa (gerador próprio)
    // A rampa colada a este trem parado cabe? (reta, dentro do chunk e fora
    // do trecho inicial livre, sem túnel até RAMP_TUNNEL_CLEARANCE depois do
    // fim do trem; veja GenerateChunk.)
    bool CanHaveRamp(const Obstacle& train, float chunk_start);
    // Desfaz a troca de faixa do passo (caixa antes do passo: before; x
    // anterior: old_x) que entraria de lado numa rampa acima de
    // RAMP_STEP_HEIGHT (RampSideBlocks, com a pegada varrida em s no passo).
    void BlockRampSides(const AABB& before, float old_x);
    // Movimento vertical do passo (veja Update): apoio, queda, pouso e teto
    // do túnel. 'before' = caixa do jogador antes do passo.
    void MoveVertically(const AABB& before, float dt);
    // Apoio mais alto sob a pegada da caixa 'now' (o jogador na posição do
    // passo) entre o leito (0), as rampas e, se include_roofs, os tetos de
    // trem em que ele pousa vindo de 'before' (LandsOnTop).
    float SupportHeight(const AABB& before, const AABB& now, bool include_roofs) const;
    float PowerUpRandom01();      // sorteio dos power-ups (gerador próprio)
    // Sorteia (gerador próprio) e põe, se couber, um power-up na faixa segura
    // do chunk; first_new = índice do 1º obstáculo do chunk, first_coin = idem
    // das moedas. Consome sempre os mesmos 3 sorteios.
    void GeneratePowerUp(int index, int safe, size_t first_new, size_t first_coin);
    // Timers, coleta de power-ups e puxão do ímã do passo (veja Update).
    void StepPowerUpTimers(float dt);
    void CollectPowerUps(const AABB& swept);
    void PullCoins(float dt);
    // Lança o porco se há cofrinho pego e nenhum voando (descarta os pedidos
    // extras), avança t e solta as moedas do passo; depois anima a queda das
    // moedas soltas.
    void StepFlyingPig(float dt);
    void DropPigCoin();
    void StepFallingCoins(float dt);
    // Registra uma coleta em 'events' (descarta se a lista já está cheia).
    void PushCollectEvent(CollectKind kind, PowerUpType powerup, const glm::vec3& position);
    float Random01();             // [0, 1), portável (só usa a saída do mt19937)
    int RandomInt(int lo, int hi); // [lo, hi]

    unsigned seed_;
    std::mt19937 rng_;
    int next_chunk_;   // próximo chunk a gerar
    int safe_lane_;    // faixa sem placa/trem do último chunk gerado
    std::vector<int> safe_lanes_;        // faixa segura de cada chunk gerado (índice = chunk)
    std::mt19937 moving_rng_;            // decide quais trens são móveis (independe de rng_)
    std::mt19937 ramp_rng_;              // decide quais trens parados têm rampa (idem)
    std::mt19937 powerup_rng_;           // decide onde nascem power-ups (idem)
    std::vector<float> obstacle_dz_;     // deslocamento de cada obstáculo no último Update
    glm::vec3 extents_[OBSTACLE_TYPE_COUNT];
};

#endif // _GAME_H
