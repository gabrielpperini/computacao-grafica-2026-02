#ifndef _GAME_H
#define _GAME_H

#include <random>

#include "game_config.h"
#include "track.h"

// Núcleo do jogo Rail Rush (estado, jogador e regras). Sem OpenGL: pode ser
// testado em tests/logic_tests.cpp. Obstáculos, moedas e power-ups das fases
// seguintes estendem esta mesma classe.

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

class Game {
public:
    explicit Game(unsigned seed);

    // Volta ao início de uma corrida (mode = PLAYING), de forma determinística:
    // re-semeia o gerador e recria a pista com a mesma semente.
    void Reset();

    // Ignorado fora de PLAYING.
    void HandleInput(Input in);

    // Avança a simulação em dt segundos (limitado a [0, MAX_DT]). Só avança em
    // PLAYING.
    void Update(float dt);

    // Gerador pseudoaleatório do jogo (spawn de obstáculos/moedas nas fases
    // seguintes). Nunca usar rand().
    std::mt19937& Rng() { return rng_; }

    GameMode mode;
    Player player;
    Track track;
    float elapsed;  // tempo simulado em PLAYING (s)

private:
    unsigned seed_;
    std::mt19937 rng_;
};

#endif // _GAME_H
