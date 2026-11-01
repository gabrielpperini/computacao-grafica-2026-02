#ifndef _GAME_CONFIG_H
#define _GAME_CONFIG_H

// Constantes de gameplay do Rail Rush. Sem dependência de OpenGL, para poder
// ser incluído tanto por main.cpp quanto pelos testes (tests/logic_tests.cpp).

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

// Trens em movimento (Tarefa 4.3): vêm na direção do jogador (velocity < 0).
const float MOVING_TRAIN_SPEED   = 8.0f;        // m/s (módulo da velocity do trem)
const float MOVING_TRAIN_CHANCE  = 1.0f / 3.0f; // fração dos trens que nascem móveis
const float MOVING_TRAIN_WARNING = 3.0f;        // s de aviso visível antes do contato
const float TRAIN_STOP_GAP       = 0.5f;        // m que o trem deixa de outro obstáculo ao parar

// Rampas e andar sobre os trens (Tarefa 4.4).
const float RAMP_CHANCE           = 0.5f;  // fração dos trens parados que ganham rampa
const float RAMP_TUNNEL_CLEARANCE = 20.0f; // m sem túnel depois do fim de um trem com rampa
// Tolerância (m) de pouso no teto do trem: a base do jogador a até isto
// abaixo do teto ainda conta como "vindo de cima"; e o trem só mata abaixo de
// teto - ROOF_TOLERANCE (bater de frente/de lado), nunca quem está em cima.
const float ROOF_TOLERANCE        = 0.25f;
// Degrau máximo (m) ao entrar de lado numa rampa: acima disso a lateral da
// rampa é parede e a troca de faixa é desfeita (sem morrer, sem subir).
const float RAMP_STEP_HEIGHT      = 0.4f;

#endif // _GAME_CONFIG_H
