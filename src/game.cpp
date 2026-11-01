#include "game.h"

#include <algorithm>
#include <cmath>

Game::Game(unsigned seed)
    : mode(GameMode::PLAYING), track(seed), elapsed(0.0f), seed_(seed), rng_(seed)
{
    Reset();
}

void Game::Reset()
{
    mode = GameMode::PLAYING;
    elapsed = 0.0f;

    player.lane = 0;
    player.x = 0.0f;
    player.y = 0.0f;
    player.vy = 0.0f;
    player.s = 0.0f;
    player.speed = PLAYER_START_SPEED;
    player.roll_timer = 0.0f;
    player.grounded = true;

    rng_.seed(seed_);
    track = Track(seed_);
    track.EnsureGeneratedUntil(player.s + SPAWN_AHEAD);
}

void Game::HandleInput(Input in)
{
    if (mode != GameMode::PLAYING)
        return;

    switch (in) {
    case Input::LEFT:
        player.lane = std::max(player.lane - 1, -1);
        break;
    case Input::RIGHT:
        player.lane = std::min(player.lane + 1, 1);
        break;
    case Input::JUMP:
        // Só pula do chão (sem pulo duplo); pular cancela o rolamento.
        if (player.grounded) {
            player.vy = JUMP_VELOCITY;
            player.grounded = false;
            player.roll_timer = 0.0f;
        }
        break;
    case Input::ROLL:
        player.roll_timer = ROLL_DURATION;
        // No ar, rolar faz o jogador descer rápido (sem frear uma queda que já
        // seja mais rápida que isso).
        if (!player.grounded)
            player.vy = std::min(player.vy, -JUMP_VELOCITY);
        break;
    }
}

void Game::Update(float dt)
{
    if (mode != GameMode::PLAYING)
        return;

    dt = std::min(std::max(dt, 0.0f), MAX_DT);
    elapsed += dt;

    // Avanço: a velocidade cresce até o máximo; a posição usa a velocidade
    // média do passo (exato para aceleração constante), o que mantém o
    // resultado praticamente independente do tamanho de dt.
    float old_speed = player.speed;
    player.speed = std::min(player.speed + PLAYER_ACCEL * dt, PLAYER_MAX_SPEED);
    player.s += 0.5f * (old_speed + player.speed) * dt;

    // Troca de faixa: anda até o alvo e para exatamente nele (sem overshoot).
    float target_x = player.lane * LANE_WIDTH;
    float max_step = LANE_SWITCH_SPEED * dt;
    float diff = target_x - player.x;
    if (std::fabs(diff) <= max_step)
        player.x = target_x;
    else
        player.x += (diff > 0.0f ? max_step : -max_step);

    // Pulo/queda (cinemática exata para gravidade constante).
    if (!player.grounded) {
        player.y += player.vy * dt - 0.5f * GRAVITY * dt * dt;
        player.vy -= GRAVITY * dt;
        if (player.y <= 0.0f) {
            player.y = 0.0f;
            player.vy = 0.0f;
            player.grounded = true;
        }
    }

    // Rolamento
    player.roll_timer = std::max(player.roll_timer - dt, 0.0f);

    // Mantém a pista gerada à frente e descarta a que ficou para trás.
    track.EnsureGeneratedUntil(player.s + SPAWN_AHEAD);
    track.DiscardBefore(player.s - DESPAWN_BEHIND);
}
