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
// acima de um vão livre de BARRIER_HIGH_CLEARANCE) só rolando; SIGN e TRAIN só
// desviando de faixa. RAMP fica para a Tarefa 4.4 (não é gerada ainda).
enum class ObstacleType { BARRIER_LOW, BARRIER_HIGH, SIGN, TRAIN, RAMP };
const int OBSTACLE_TYPE_COUNT = 5;

// box: caixa em coordenadas de pista, já posicionada (z = s). s = centro da
// caixa ao longo da pista. velocity: trens em movimento (Tarefa 4.4; 0 aqui).
struct Obstacle { ObstacleType type; int lane; float s, velocity; AABB box; };

// Moeda: esfera de raio COIN_RADIUS centrada em (lane*LANE_WIDTH, y, s).
struct Coin { int lane; float s, y; bool collected; };

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

// Partes sólidas de um obstáculo (caixas em coordenadas de pista), usadas tanto
// na colisão quanto no desenho: a mesma geometria nos dois. A barreira alta tem
// 3 partes (viga = o.box e dois postes até o chão nas bordas); as demais, 1
// (o.box). Devolve o número de partes escritas em parts[0..2].
int ObstacleParts(const Obstacle& o, AABB parts[3]);

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
    // PLAYING. Ordem: movimento do jogador; colisões com a caixa varrida do
    // jogador (obstáculo -> GAME_OVER; moeda -> coletada); pontuação; spawn de
    // chunks até player.s + SPAWN_AHEAD (se spawn_enabled) e descarte do que
    // ficou mais de DESPAWN_BEHIND para trás.
    void Update(float dt);

    // Caixa do jogador em coordenadas de pista: altura PLAYER_ROLL_HEIGHT
    // rolando, PLAYER_HEIGHT em pé; base em player.y.
    AABB PlayerBox() const;

    // Semi-extensões de cada tipo de obstáculo (main.cpp passa as dos modelos
    // escalados, depois de carregar os OBJ). A largura é limitada a
    // LANE_WIDTH/2 - LANE_OBSTACLE_MARGIN e a profundidade ao miolo do chunk,
    // para manter a garantia de faixa livre. Vale para obstáculos criados
    // depois da chamada.
    void SetObstacleExtents(ObstacleType type, glm::vec3 half_extents);
    glm::vec3 ObstacleExtents(ObstacleType type) const;

    // Obstáculo do tipo na faixa, centrado em s, apoiado no chão (a barreira
    // alta começa em y = BARRIER_HIGH_CLEARANCE).
    Obstacle MakeObstacle(ObstacleType type, int lane, float s) const;

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

    std::vector<Obstacle> obstacles; // ordenados por s (ordem de geração)
    std::vector<Coin> coins;
    int coins_collected;
    float score;     // += distância percorrida; += 10*multiplier por moeda
    int multiplier;  // 1 (o power-up de multiplicador vem na Fase 5)

    // false = não gera obstáculos/moedas (testes de física e cenários E2E que
    // não devem terminar em colisão). Colisões com os que já existem continuam.
    bool spawn_enabled;

private:
    void GenerateChunk(int index);
    float Random01();             // [0, 1), portável (só usa a saída do mt19937)
    int RandomInt(int lo, int hi); // [lo, hi]

    unsigned seed_;
    std::mt19937 rng_;
    int next_chunk_;   // próximo chunk a gerar
    int safe_lane_;    // faixa sem placa/trem do último chunk gerado
    glm::vec3 extents_[OBSTACLE_TYPE_COUNT];
};

#endif // _GAME_H
