#include "game.h"

#include <algorithm>
#include <cmath>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "scenery.h" // TunnelCeilingHeight (perfil do teto do túnel)

namespace {

// Mistura a semente do jogo para o gerador do spawn, de modo que a sequência
// não seja a mesma do gerador da pista (que usa a semente pura).
const unsigned kSpawnSeedMix = 0x9E3779B9u;
// Idem para o sorteio de quais trens são móveis: gerador separado, para que
// ele não altere a sequência de tipos/posições dos obstáculos (rng_).
const unsigned kMovingSeedMix = 0x85EBCA6Bu;
// Idem para o sorteio de quais trens parados ganham rampa (Tarefa 4.4).
const unsigned kRampSeedMix = 0xC2B2AE35u;
// Idem para o sorteio dos power-ups (Tarefa 5.2).
const unsigned kPowerUpSeedMix = 0x27D4EB2Fu;

bool IsBarrier(ObstacleType t)
{
    return t == ObstacleType::BARRIER_LOW || t == ObstacleType::BARRIER_HIGH;
}

} // namespace

int ObstacleParts(const Obstacle& o, AABB parts[3])
{
    parts[0] = o.box;
    if (o.type != ObstacleType::BARRIER_HIGH)
        return 1;
    // Postes nas bordas laterais da viga, do chão até a base dela.
    float w = 2.0f * BARRIER_POST_HALF_WIDTH;
    parts[1].min = glm::vec3(o.box.min.x, 0.0f, o.box.min.z);
    parts[1].max = glm::vec3(o.box.min.x + w, o.box.min.y, o.box.max.z);
    parts[2].min = glm::vec3(o.box.max.x - w, 0.0f, o.box.min.z);
    parts[2].max = glm::vec3(o.box.max.x, o.box.min.y, o.box.max.z);
    return 3;
}

glm::vec3 CoinCenter(const Coin& c)
{
    if (c.pulled)
        return glm::vec3(c.px, c.py, c.ps);
    return glm::vec3(c.lane * LANE_WIDTH, c.y, c.s);
}

Game::Game(unsigned seed)
    : mode(GameMode::PLAYING), track(seed), elapsed(0.0f), coins_collected(0),
      score(0.0f), multiplier(1), piggy_pickups(0), spawn_enabled(true), ramps_enabled(true),
      powerups_enabled(true), seed_(seed),
      rng_(seed ^ kSpawnSeedMix), next_chunk_(0), safe_lane_(0),
      moving_rng_(seed ^ kMovingSeedMix), ramp_rng_(seed ^ kRampSeedMix),
      powerup_rng_(seed ^ kPowerUpSeedMix)
{
    for (int i = 0; i < OBSTACLE_TYPE_COUNT; i++)
        SetObstacleExtents((ObstacleType)i, 0.5f * glm::vec3(OBSTACLE_SIZE[i][0], OBSTACLE_SIZE[i][1],
                                                             OBSTACLE_SIZE[i][2]));
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

    obstacles.clear();
    coins.clear();
    powerups.clear();
    coins_collected = 0;
    score = 0.0f;
    multiplier = 1;
    piggy_pickups = 0;
    for (int t = 0; t < POWERUP_TYPE_COUNT; t++)
        powerup_timer[t] = 0.0f;
    next_chunk_ = 0;
    safe_lane_ = 0;
    safe_lanes_.clear();
    obstacle_dz_.clear();

    rng_.seed(seed_ ^ kSpawnSeedMix);
    moving_rng_.seed(seed_ ^ kMovingSeedMix);
    ramp_rng_.seed(seed_ ^ kRampSeedMix);
    powerup_rng_.seed(seed_ ^ kPowerUpSeedMix);
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
        // Só pula do chão (sem pulo duplo); pular cancela o rolamento. Com o
        // tênis (Tarefa 5.2) a velocidade inicial é SNEAKERS_JUMP_MULT vezes
        // maior (a altura, ~ o quadrado disso).
        if (player.grounded) {
            player.vy = JUMP_VELOCITY * (PowerUpActive(PowerUpType::SNEAKERS) ? SNEAKERS_JUMP_MULT : 1.0f);
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
    StepPowerUpTimers(dt);

    // Caixa do jogador antes do passo (para a caixa varrida, abaixo).
    AABB before = PlayerBox();
    float old_s = player.s;

    // Avanço: a velocidade cresce até o máximo; a posição usa a velocidade
    // média do passo (exato para aceleração constante), o que mantém o
    // resultado praticamente independente do tamanho de dt.
    float old_speed = player.speed;
    player.speed = std::min(player.speed + PLAYER_ACCEL * dt, PLAYER_MAX_SPEED);
    player.s += 0.5f * (old_speed + player.speed) * dt;

    // Troca de faixa: anda até o alvo e para exatamente nele (sem overshoot).
    const float old_x = player.x;
    float target_x = player.lane * LANE_WIDTH;
    float max_step = LANE_SWITCH_SPEED * dt;
    float diff = target_x - player.x;
    if (std::fabs(diff) <= max_step)
        player.x = target_x;
    else
        player.x += (diff > 0.0f ? max_step : -max_step);
    BlockRampSides(before, old_x);

    // Rolamento
    player.roll_timer = std::max(player.roll_timer - dt, 0.0f);

    // Trens móveis andam neste passo (a caixa deles anda junto). Antes do
    // movimento vertical: o teto de um trem móvel é o da posição nova.
    MoveObstacles(dt);

    // Pulo, queda, rampas e tetos dos trens (Tarefa 4.4).
    MoveVertically(before, dt);

    // Colisões com a caixa VARRIDA: a que envolve as caixas do jogador antes
    // e depois do passo. Na velocidade máxima com dt = MAX_DT o jogador anda
    // 1.5 m por passo, mais que a própria profundidade (0.8 m) somada à de uma
    // barreira (0.3 m); testando só a caixa final ele "atravessaria" a
    // barreira sem colidir (tunneling).
    AABB now = PlayerBox();
    AABB swept;
    swept.min = glm::min(before.min, now.min);
    swept.max = glm::max(before.max, now.max);

    // Obstáculo (qualquer parte sólida) -> fim de jogo. Um obstáculo que
    // andou neste passo também é varrido (união da caixa antes e depois), pelo
    // mesmo motivo: a velocidade de fechamento com um trem móvel chega a
    // 38 m/s (1.9 m por passo de MAX_DT).
    // Exceções (Tarefa 4.4): a rampa nunca mata; o trem em cujo teto o
    // jogador está (ou acabou de chegar, vindo de cima) também não; e o trem
    // só mata abaixo de teto - ROOF_TOLERANCE (batida de frente/de lado),
    // senão a caixa varrida de quem sai pela lateral do teto, ainda na altura
    // dele, "encostaria" no trem.
    AABB parts[3];
    for (size_t i = 0; i < obstacles.size() && mode == GameMode::PLAYING; ++i) {
        const Obstacle& o = obstacles[i];
        if (o.type == ObstacleType::RAMP)
            continue;
        if (o.type == ObstacleType::TRAIN && LandsOnTop(before, now, o.box, ROOF_TOLERANCE))
            continue;
        // Em pé (apoiado na rampa ou num teto) na altura do teto, sobre a
        // pegada do trem: está em cima dele, qualquer que seja a fase do passo.
        if (o.type == ObstacleType::TRAIN && player.grounded &&
            now.min.y >= o.box.max.y - ROOF_TOLERANCE && FootprintsOverlap(now, o.box))
            continue;
        int n = ObstacleParts(o, parts);
        if (o.type == ObstacleType::TRAIN)
            parts[0].max.y -= ROOF_TOLERANCE;
        const float dz = obstacle_dz_[i];
        for (int k = 0; k < n; ++k) {
            if (dz < 0.0f)
                parts[k].max.z -= dz;
            else if (dz > 0.0f)
                parts[k].min.z -= dz;
            if (AABBIntersectsAABB(swept, parts[k])) {
                mode = GameMode::GAME_OVER;
                break;
            }
        }
    }

    // Power-ups (esfera x caixa varrida) e puxão do ímã nas moedas, antes de
    // coletá-las: o multiplicador recém-pego já vale para as moedas do passo.
    CollectPowerUps(swept);
    PullCoins(dt);

    // Moedas: esfera da moeda x caixa do jogador -> coletada (uma única vez).
    for (size_t i = 0; i < coins.size(); ++i) {
        Coin& c = coins[i];
        if (c.collected)
            continue;
        glm::vec3 center = CoinCenter(c);
        if (SphereIntersectsAABB(center, COIN_RADIUS, swept)) {
            c.collected = true;
            coins_collected += 1;
            score += 10.0f * multiplier;
        }
    }

    // Pontuação contínua: a distância percorrida no passo (= velocidade * dt).
    score += player.s - old_s;

    // Mantém a pista e o conteúdo gerados à frente e descarta o que ficou
    // para trás.
    track.EnsureGeneratedUntil(player.s + SPAWN_AHEAD);
    track.DiscardBefore(player.s - DESPAWN_BEHIND - TRACK_KEEP_MARGIN);
    if (spawn_enabled)
        GenerateChunksUntil(player.s + SPAWN_AHEAD);

    float behind = player.s - DESPAWN_BEHIND;
    obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(),
                                   [behind](const Obstacle& o) { return o.box.max.z < behind; }),
                    obstacles.end());
    coins.erase(std::remove_if(coins.begin(), coins.end(),
                               [behind](const Coin& c) { return CoinCenter(c).z < behind; }),
                coins.end());
    powerups.erase(std::remove_if(powerups.begin(), powerups.end(),
                                  [behind](const PowerUp& p) { return p.s < behind; }),
                   powerups.end());
}

// Os tempos dos power-ups com duração correm só aqui (Update só roda em
// PLAYING, então pausa, câmera livre e fim de jogo os congelam). O
// multiplicador acompanha o tempo do power-up de multiplicador.
void Game::StepPowerUpTimers(float dt)
{
    for (int t = 0; t < POWERUP_TYPE_COUNT; t++)
        if (powerup_timer[t] > 0.0f)
            powerup_timer[t] = std::max(powerup_timer[t] - dt, 0.0f);
    multiplier = PowerUpActive(PowerUpType::MULTIPLIER) ? 2 : 1;
}

// Coleta os power-ups tocados pela caixa varrida do jogador. Pegar um tipo com
// duração (re)põe o tempo em POWERUP_DURATION: o mesmo tipo de novo só
// renova, não soma nem duplica o efeito. O cofrinho só registra o pedido de
// porco (Tarefa 5.3 o consome).
void Game::CollectPowerUps(const AABB& swept)
{
    for (size_t i = 0; i < powerups.size(); ++i) {
        PowerUp& p = powerups[i];
        if (p.collected)
            continue;
        glm::vec3 center(p.lane * LANE_WIDTH, POWERUP_HEIGHT, p.s);
        if (!SphereIntersectsAABB(center, POWERUP_RADIUS, swept))
            continue;
        p.collected = true;
        if (p.type == PowerUpType::PIGGY)
            piggy_pickups += 1;
        else
            powerup_timer[(int)p.type] = POWERUP_DURATION;
    }
    multiplier = PowerUpActive(PowerUpType::MULTIPLIER) ? 2 : 1;
}

// Ímã: com ele ativo, moedas a menos de MAGNET_RADIUS do centro do jogador
// (esfera x esfera) passam a ser "puxadas" e voam até ele; uma moeda puxada
// continua indo mesmo depois que o ímã acaba. A velocidade soma a do jogador,
// para alcançar moedas que ficaram para trás. Sempre com dt.
void Game::PullCoins(float dt)
{
    const AABB box = PlayerBox();
    const glm::vec3 target = 0.5f * (box.min + box.max);
    const float step = (player.speed + MAGNET_PULL_SPEED) * dt;
    const bool magnet = PowerUpActive(PowerUpType::MAGNET);
    for (size_t i = 0; i < coins.size(); ++i) {
        Coin& c = coins[i];
        if (c.collected)
            continue;
        glm::vec3 pos = CoinCenter(c);
        if (!c.pulled) {
            if (!magnet || !SphereIntersectsSphere(pos, COIN_RADIUS, target, MAGNET_RADIUS))
                continue;
            c.pulled = true;
        }
        const glm::vec3 d = target - pos;
        const float dist = glm::length(d);
        pos = (dist <= step) ? target : pos + d * (step / dist);
        c.px = pos.x;
        c.py = pos.y;
        c.ps = pos.z;
    }
}

AABB Game::PlayerBox() const
{
    float h = (player.roll_timer > 0.0f) ? PLAYER_ROLL_HEIGHT : PLAYER_HEIGHT;
    AABB b;
    b.min = glm::vec3(player.x - PLAYER_HALF_WIDTH, player.y, player.s - PLAYER_HALF_DEPTH);
    b.max = glm::vec3(player.x + PLAYER_HALF_WIDTH, player.y + h, player.s + PLAYER_HALF_DEPTH);
    return b;
}

// A lateral da rampa é parede (Tarefa 4.4): se a troca de faixa deste passo
// faria a pegada entrar de lado numa rampa acima de RAMP_STEP_HEIGHT dos pés,
// ela é desfeita: o jogador para encostado por fora da lateral, a faixa alvo
// volta a ser a vizinha do lado de onde veio, e a altura não muda (sem
// "teletransporte" para cima, sem game over). Como a rampa e o trem colado
// nela têm a mesma faixa x, na emenda rampa/trem o resultado é sempre o mesmo
// enquanto a pegada ainda toca a rampa, qualquer que seja a fase do quadro.
//
// A pegada testada é a VARRIDA em s no passo (antes ∪ agora), a mesma faixa
// de s com que SupportHeight mede as rampas: uma rampa que a pegada tocou em
// algum momento do passo e que a troca de faixa faria entrar de lado é barrada
// aqui, e por isso nunca pode erguer o jogador lá (a pegada barrada fica fora
// da faixa x da rampa, antes e depois). Sem isto, na emenda rampa/trem, uma
// pegada que deixa a rampa em s no mesmo passo em que entra de lado seria
// erguida até o teto (3.2 m num quadro).
void Game::BlockRampSides(const AABB& before, float old_x)
{
    if (player.x == old_x)
        return;
    for (size_t i = 0; i < obstacles.size(); ++i) {
        const Obstacle& o = obstacles[i];
        if (o.type != ObstacleType::RAMP)
            continue;
        AABB to = PlayerBox();
        to.min.z = std::min(to.min.z, before.min.z);
        to.max.z = std::max(to.max.z, before.max.z);
        AABB from = to;
        from.min.x += old_x - player.x;
        from.max.x += old_x - player.x;
        float x;
        if (RampSideBlocks(o.box, from, to, RAMP_STEP_HEIGHT, &x)) {
            const int back = (old_x < player.x) ? -1 : 1;
            player.x = x;
            player.lane = std::min(std::max(o.lane + back, -1), 1);
        }
    }
}

// Movimento vertical do jogador (Tarefa 4.4). O "chão" é o apoio mais alto
// sob a pegada: o leito da pista (y = 0), uma rampa (sobe com s) ou o teto de
// um trem em que ele pousou vindo de cima (LandsOnTop).
void Game::MoveVertically(const AABB& before, float dt)
{
    // 1) Em pé: segue o apoio sob a pegada na posição nova (rampa acima dos
    //    pés: sobe nela). Se o apoio ficou abaixo dos pés (passou do fim do
    //    trem ou da rampa, ou trocou de faixa para uma sem trem), a gravidade
    //    volta, a partir de vy = 0, já neste passo.
    if (player.grounded) {
        const float g = SupportHeight(before, PlayerBox(), true);
        if (g >= player.y - ROOF_TOLERANCE) {
            player.y = g;
            player.vy = 0.0f;
            return;
        }
        player.grounded = false;
        player.vy = 0.0f;
    }

    // 2) No ar: cinemática exata para gravidade constante.
    player.y += player.vy * dt - 0.5f * GRAVITY * dt * dt;
    player.vy -= GRAVITY * dt;

    // 3) Dentro do túnel a cabeça não passa da abóbada (o teto é mais baixo
    //    nas faixas de fora); ao bater, para de subir. "Dentro" é a caixa toda:
    //    basta a frente ou a traseira estar no túnel. Na aproximação (Tarefa
    //    5.2) o teto vale com uma folga que cresce com a distância até a boca
    //    (TUNNEL_APPROACH_SLOPE): um pulo alto (tênis, ou saindo do teto de um
    //    trem) que entra no túnel no ar é achatado aos poucos, não de uma vez
    //    quando a frente cruza o portal.
    {
        const AABB box = PlayerBox();
        float dist = track.TunnelDistanceAhead(box.max.z, TUNNEL_APPROACH_LENGTH);
        if (dist < 0.0f && track.IsTunnelAt(box.min.z))
            dist = 0.0f;
        if (dist >= 0.0f) {
            const float ceiling = std::min(TunnelCeilingHeight(box.min.x), TunnelCeilingHeight(box.max.x));
            const float push = CeilingPenetration(box, TunnelApproachCeiling(ceiling, dist, TUNNEL_APPROACH_SLOPE));
            if (push > 0.0f) {
                player.y -= push;
                player.vy = std::min(player.vy, 0.0f);
            }
        }
    }

    // 4) Pouso. O teto do trem só conta descendo (vy <= 0): quem pula de cima
    //    dele não é puxado de volta. Já a rampa é sólida: subindo por dentro
    //    dela (pulo mais lento que a subida da rampa), a superfície empurra o
    //    jogador para cima sem cancelar o pulo.
    const float g = SupportHeight(before, PlayerBox(), player.vy <= 0.0f);
    if (player.y <= g) {
        player.y = g;
        if (player.vy <= 0.0f) {
            player.vy = 0.0f;
            player.grounded = true;
        }
    }
}

float Game::SupportHeight(const AABB& before, const AABB& now, bool include_roofs) const
{
    // Rampas: pegada VARRIDA no passo (antes ∪ agora). A 30 m/s com
    // dt = MAX_DT o jogador anda 1.5 m, e a pegada (0.8 m) pode sair inteira
    // do fim da rampa num único passo, com a base anterior ainda abaixo de
    // teto - ROOF_TOLERANCE: sem isto ele não chegaria ao teto do trem (e
    // bateria na frente dele). A rampa varrida o leva até a altura em que a
    // frente da pegada está agora (no fim da rampa: o teto).
    AABB reach = now;
    reach.min = glm::min(before.min, now.min);
    reach.max = glm::max(before.max, now.max);

    float g = 0.0f; // leito da pista
    for (size_t i = 0; i < obstacles.size(); ++i) {
        const Obstacle& o = obstacles[i];
        float h;
        if (o.type == ObstacleType::RAMP && RampHeightUnder(o.box, reach, &h))
            g = std::max(g, h);
        else if (o.type == ObstacleType::TRAIN && include_roofs &&
                 LandsOnTop(before, now, o.box, ROOF_TOLERANCE))
            g = std::max(g, o.box.max.y);
    }
    return g;
}

float Game::GroundHeight() const
{
    const AABB p = PlayerBox();
    float g = 0.0f; // leito da pista
    for (size_t i = 0; i < obstacles.size(); ++i) {
        const Obstacle& o = obstacles[i];
        float h;
        if (o.type == ObstacleType::RAMP && RampHeightUnder(o.box, p, &h) &&
            h <= player.y + ROOF_TOLERANCE)
            g = std::max(g, h);
        else if (o.type == ObstacleType::TRAIN && FootprintsOverlap(p, o.box) &&
                 o.box.max.y <= player.y + ROOF_TOLERANCE)
            g = std::max(g, o.box.max.y);
    }
    return g;
}

float Game::CameraTargetHeight() const
{
    const float ground = GroundHeight();
    return ground + CAMERA_JUMP_FOLLOW * std::max(player.y - ground - CAMERA_JUMP_FREE_HEIGHT, 0.0f);
}

void Game::SetObstacleExtents(ObstacleType type, glm::vec3 half_extents)
{
    glm::vec3 h(std::fabs(half_extents.x), std::fabs(half_extents.y), std::fabs(half_extents.z));
    // Não invade a faixa vizinha, e cabe no miolo do chunk (fora das emendas
    // livres), senão a garantia de faixa transponível deixaria de valer.
    h.x = std::min(h.x, 0.5f * LANE_WIDTH - LANE_OBSTACLE_MARGIN);
    h.z = std::min(h.z, 0.5f * (CHUNK_LENGTH - CHUNK_CLEAR_HEAD - CHUNK_CLEAR_TAIL));
    extents_[(int)type] = h;
}

glm::vec3 Game::ObstacleExtents(ObstacleType type) const
{
    return extents_[(int)type];
}

Obstacle Game::MakeObstacle(ObstacleType type, int lane, float s) const
{
    glm::vec3 h = extents_[(int)type];
    float base = (type == ObstacleType::BARRIER_HIGH) ? BARRIER_HIGH_CLEARANCE : 0.0f;
    Obstacle o;
    o.type = type;
    o.lane = lane;
    o.s = s;
    o.velocity = 0.0f;
    o.box = MakeAABB(glm::vec3(lane * LANE_WIDTH, base + h.y, s), h);
    o.min_s = -1e9f;
    return o;
}

Obstacle Game::MakeMovingTrain(int lane, float s, float velocity) const
{
    Obstacle o = MakeObstacle(ObstacleType::TRAIN, lane, s);
    const float hz = extents_[(int)ObstacleType::TRAIN].z;
    if (velocity < 0.0f && track.IsStraightRange(s - hz, s + hz)) {
        o.velocity = velocity;
        o.min_s = track.StraightRunStart(s - hz);
    }
    return o;
}

Obstacle Game::MakeRampFor(const Obstacle& train) const
{
    const float len = 2.0f * extents_[(int)ObstacleType::RAMP].z;
    Obstacle r = MakeObstacle(ObstacleType::RAMP, train.lane, train.box.min.z - 0.5f * len);
    // Exatamente colada ao trem e na altura do teto dele (sem degrau na emenda).
    r.box.max.z = train.box.min.z;
    r.box.min.z = train.box.min.z - len;
    r.box.max.y = train.box.max.y;
    r.s = 0.5f * (r.box.min.z + r.box.max.z);
    return r;
}

bool Game::CanHaveRamp(const Obstacle& train, float chunk_start)
{
    const float r0 = train.box.min.z - 2.0f * extents_[(int)ObstacleType::RAMP].z;
    // Dentro do chunk do trem (entra só na emenda livre do começo dele: a
    // rampa nunca mata, então a troca de faixa na emenda continua possível)
    // e fora do trecho inicial sem obstáculos.
    if (r0 < chunk_start || r0 < OBSTACLE_FREE_START)
        return false;
    if (!track.IsStraightRange(r0, train.box.max.z))
        return false;
    // Nem dentro de túnel, nem com túnel logo depois do trem: quem está no
    // teto nunca entra num túnel (a abóbada é mais baixa nas faixas de fora).
    const float s1 = train.box.max.z + RAMP_TUNNEL_CLEARANCE;
    track.EnsureGeneratedUntil(s1);
    return !track.TunnelOverlaps(r0, s1);
}

float Game::RampRandom01()
{
    return (float)(ramp_rng_() >> 8) * (1.0f / 16777216.0f);
}

int Game::SafeLaneOfChunk(int chunk) const
{
    if (chunk < 0)
        return 0;
    if ((size_t)chunk >= safe_lanes_.size())
        return -2;
    return safe_lanes_[(size_t)chunk];
}

// Anda cada trem móvel (velocity < 0) em velocity*dt. Ele para (velocity = 0,
// de vez) se a ponta dianteira passaria de min_s (o começo da reta onde nasceu)
// ou entraria na folga TRAIN_STOP_GAP de outro obstáculo da mesma faixa que
// está à sua frente. Os obstáculos ficam em ordem de s, então quem está à
// frente já andou neste passo. Positivo não é suportado (nenhum trem sobe).
void Game::MoveObstacles(float dt)
{
    obstacle_dz_.assign(obstacles.size(), 0.0f);
    for (size_t i = 0; i < obstacles.size(); ++i) {
        Obstacle& o = obstacles[i];
        if (o.velocity >= 0.0f)
            continue;

        float limit = o.min_s;
        for (size_t j = 0; j < obstacles.size(); ++j) {
            const Obstacle& other = obstacles[j];
            if (j != i && other.lane == o.lane && other.box.max.z <= o.box.min.z + 1e-3f)
                limit = std::max(limit, other.box.max.z + TRAIN_STOP_GAP);
        }

        float target = o.box.min.z + o.velocity * dt;
        if (target < limit) {
            target = std::min(limit, o.box.min.z); // nunca recua para a folga
            o.velocity = 0.0f;
        }
        const float dz = target - o.box.min.z;
        o.s += dz;
        o.box.min.z += dz;
        o.box.max.z += dz;
        obstacle_dz_[i] = dz;
    }
}

bool Game::CanSpawnMoving(int lane, float s, float hz, float min_s) const
{
    // Aviso: o trem precisa ser visível por MOVING_TRAIN_WARNING segundos
    // antes do contato (jogador e trem se aproximam a speed + |velocity|).
    const float closing = player.speed + MOVING_TRAIN_SPEED;
    if (s - player.s < closing * MOVING_TRAIN_WARNING)
        return false;

    // Até onde o nariz do trem chega antes de o jogador passar por ele. A
    // velocidade do jogador só cresce, então o encontro acontece, no máximo,
    // em gap / closing segundos: este é um limite inferior seguro.
    const float front = s - hz;
    const float gap = front - (player.s + PLAYER_HALF_DEPTH);
    const float lowest = std::max(front - MOVING_TRAIN_SPEED * gap / closing, min_s);

    // Garantia de faixa livre: o trem ocupa os chunks de [lowest, s + hz] e
    // o jogador que segue a faixa segura de cada um (trocando na emenda de
    // CHUNK_CLEAR_TAIL + CHUNK_CLEAR_HEAD metros) nunca pode cruzar com ele.
    // Por isso a faixa do trem não pode ser a segura de nenhum desses chunks
    // (nem do chunk que cobre a emenda logo abaixo do ponto mais baixo).
    const float lo = lowest - (CHUNK_CLEAR_TAIL + CHUNK_CLEAR_HEAD);
    const int c0 = std::max(0, (int)std::floor(lo / CHUNK_LENGTH));
    const int c1 = (int)std::floor((s + hz) / CHUNK_LENGTH);
    for (int c = c0; c <= c1; ++c)
        if (SafeLaneOfChunk(c) == lane)
            return false;
    return true;
}

float Game::MovingRandom01()
{
    return (float)(moving_rng_() >> 8) * (1.0f / 16777216.0f);
}

void Game::GenerateChunksUntil(float s)
{
    while ((next_chunk_ + 1) * CHUNK_LENGTH <= s)
        GenerateChunk(next_chunk_++);
}

float Game::Random01()
{
    // 24 bits da saída do mt19937 (sequência fixada pelo padrão), em vez de
    // std::uniform_*_distribution, cujo resultado varia entre bibliotecas:
    // assim a mesma semente gera os mesmos obstáculos em qualquer compilador.
    return (float)(rng_() >> 8) * (1.0f / 16777216.0f);
}

int Game::RandomInt(int lo, int hi)
{
    int v = lo + (int)(Random01() * (float)(hi - lo + 1));
    return std::min(v, hi);
}

// Um chunk [c*CHUNK_LENGTH, (c+1)*CHUNK_LENGTH). Regras (garantia de que
// sempre há caminho, testada em logic_tests):
//  - uma "faixa segura" por chunk, sem placa nem trem; ela anda no máximo uma
//    faixa de um chunk para o outro;
//  - no máximo um obstáculo por faixa por chunk, inteiro dentro do miolo
//    [início + CHUNK_CLEAR_HEAD, fim - CHUNK_CLEAR_TAIL] (trens não cruzam a
//    emenda entre chunks, e a emenda é livre para trocar de faixa);
//  - barreiras baixa/alta são transponíveis (pulo/rolamento), então podem
//    aparecer na faixa segura; nunca uma baixa e uma alta a menos de
//    LOW_HIGH_MIN_GAP na mesma faixa;
//  - nada antes de OBSTACLE_FREE_START; o 1º chunk só tem a fileira de moedas
//    de boas-vindas na faixa inicial;
//  - trens/rampas só inteiros dentro de retas da pista (nas curvas viram
//    placas); barreiras, placas e moedas podem ficar nas curvas;
//  - rampa (Tarefa 4.4) só colada à frente de um trem parado, na faixa dele
//    (nunca a segura), dentro do chunk: é a única coisa que entra na emenda
//    livre do começo do chunk, e não bloqueia (nunca mata). Veja CanHaveRamp.
void Game::GenerateChunk(int index)
{
    float start = index * CHUNK_LENGTH;
    float end = start + CHUNK_LENGTH;

    // A pista precisa existir no chunk para saber onde há curvas.
    track.EnsureGeneratedUntil(end);

    if (index == 0) {
        // Fileira de boas-vindas: faixa 0, termina bem antes de OBSTACLE_FREE_START.
        for (int i = 0; i < WELCOME_COIN_COUNT; i++) {
            Coin c = {0, 14.0f + i * COIN_SPACING, COIN_HEIGHT, false};
            coins.push_back(c);
        }
        safe_lane_ = 0;
        safe_lanes_.push_back(0);
        return;
    }

    int safe = std::min(std::max(safe_lane_ + RandomInt(-1, 1), -1), 1);
    safe_lane_ = safe;
    safe_lanes_.push_back(safe); // índice = chunk (os chunks saem em ordem)

    float w0 = std::max(start + CHUNK_CLEAR_HEAD, OBSTACLE_FREE_START);
    float w1 = end - CHUNK_CLEAR_TAIL;

    size_t first_new = obstacles.size();
    for (int lane = -1; lane <= 1 && w0 < w1; lane++) {
        float r = Random01();
        bool has = true;
        ObstacleType type = ObstacleType::BARRIER_LOW;
        if (lane == safe) {
            // Faixa segura: vazia ou com uma barreira (pula/rola).
            if (r < 0.5f)       has = false;
            else if (r < 0.75f) type = ObstacleType::BARRIER_LOW;
            else                type = ObstacleType::BARRIER_HIGH;
        } else {
            if (r < 0.2f)        has = false;
            else if (r < 0.5f)   type = ObstacleType::TRAIN;
            else if (r < 0.75f)  type = ObstacleType::SIGN;
            else if (r < 0.875f) type = ObstacleType::BARRIER_LOW;
            else                 type = ObstacleType::BARRIER_HIGH;
        }
        float u = Random01(); // sempre consumido: a sequência não depende de 'has'
        if (!has)
            continue;

        float hz = extents_[(int)type].z;
        float lo = w0 + hz, hi = w1 - hz;
        if (hi < lo)
            continue; // não cabe no miolo (só se o chunk for curto demais)
        float s = lo + (hi - lo) * u;

        // Trens (e suas rampas, Tarefa 4.4) só em retas: um trem de 13 m numa curva
        // de raio 40 sairia dos trilhos nas pontas e a caixa de colisão (reta
        // em coordenadas de pista) não bateria com os carros desenhados. Se o
        // trem não cabe inteiro numa reta, vira placa no mesmo lugar: continua
        // bloqueando só esta faixa (a garantia da faixa segura não muda) e a
        // sequência sorteada é a mesma.
        if ((type == ObstacleType::TRAIN || type == ObstacleType::RAMP) &&
            !track.IsStraightRange(s - hz, s + hz))
            type = ObstacleType::SIGN;

        // Barreira baixa perto de uma alta (ou vice-versa) na mesma faixa seria
        // intransponível (não dá para pular e rolar ao mesmo tempo). Com um
        // obstáculo por faixa por chunk isso não acontece, mas fica a guarda
        // explícita caso as regras acima mudem.
        bool combo = false;
        if (IsBarrier(type)) {
            for (size_t k = obstacles.size(); k-- > 0 && obstacles[k].s > s - CHUNK_LENGTH - LOW_HIGH_MIN_GAP;) {
                const Obstacle& o = obstacles[k];
                if (o.lane == lane && IsBarrier(o.type) && o.type != type &&
                    std::fabs(o.s - s) < LOW_HIGH_MIN_GAP)
                    combo = true;
            }
        }
        if (!combo) {
            Obstacle o = MakeObstacle(type, lane, s);
            // Alguns trens nascem móveis (sorteio próprio, determinístico),
            // se houver aviso suficiente e a faixa continuar transponível.
            if (type == ObstacleType::TRAIN && MovingRandom01() < MOVING_TRAIN_CHANCE) {
                float min_s = track.StraightRunStart(s - hz);
                if (CanSpawnMoving(lane, s, hz, min_s)) {
                    o.velocity = -MOVING_TRAIN_SPEED;
                    o.min_s = min_s;
                }
            }
            // Parte dos trens parados ganha uma rampa colada à frente (sorteio
            // com gerador próprio, sempre feito: não altera a sequência dos
            // demais). Trem móvel nunca tem rampa. A faixa do trem não é a
            // segura, e a rampa não a bloqueia: a garantia continua valendo.
            if (type == ObstacleType::TRAIN && o.velocity == 0.0f) {
                const bool want = RampRandom01() < RAMP_CHANCE;
                if (want && ramps_enabled && CanHaveRamp(o, start))
                    obstacles.push_back(MakeRampFor(o)); // antes do trem: ordem de s
            }
            obstacles.push_back(o);
        }
    }

    // Fileira de moedas na faixa segura, pulando as que cairiam numa barreira.
    const size_t first_coin = coins.size();
    int count = RandomInt(4, 7);
    float row = (count - 1) * COIN_SPACING;
    float s0 = start + 1.0f + Random01() * (CHUNK_LENGTH - 2.0f - row);
    for (int i = 0; i < count; i++) {
        float s = s0 + i * COIN_SPACING;
        bool blocked = false;
        for (size_t k = first_new; k < obstacles.size(); k++) {
            const Obstacle& o = obstacles[k];
            if (o.lane == safe && s + COIN_RADIUS >= o.box.min.z - 0.5f &&
                s - COIN_RADIUS <= o.box.max.z + 0.5f)
                blocked = true;
        }
        if (!blocked) {
            Coin c = {safe, s, COIN_HEIGHT, false};
            coins.push_back(c);
        }
    }

    GeneratePowerUp(index, safe, first_new, first_coin);
}

float Game::PowerUpRandom01()
{
    return (float)(powerup_rng_() >> 8) * (1.0f / 16777216.0f);
}

// Power-up do chunk (Tarefa 5.2): chance POWERUP_CHUNK_CHANCE, tipo uniforme,
// na faixa segura. Os três sorteios são sempre consumidos (o gerador é
// próprio: a sequência de obstáculos e moedas não muda). A posição é o
// primeiro de 16 pontos igualmente espaçados no miolo do chunk (a partir do
// sorteado) que esteja a pelo menos POWERUP_CLEARANCE de qualquer obstáculo
// ou moeda da mesma faixa; se nenhum serve, o chunk fica sem power-up.
void Game::GeneratePowerUp(int index, int safe, size_t first_new, size_t first_coin)
{
    const float roll = PowerUpRandom01();
    const float kind = PowerUpRandom01();
    const float u = PowerUpRandom01();
    if (!powerups_enabled || roll >= POWERUP_CHUNK_CHANCE)
        return;

    const float start = index * CHUNK_LENGTH;
    const float lo = start + 2.0f, hi = start + CHUNK_LENGTH - 2.0f;
    const int tries = 16;
    for (int k = 0; k < tries; ++k) {
        const float f = std::fmod(u + (float)k / (float)tries, 1.0f);
        const float s = lo + (hi - lo) * f;
        bool free = true;
        for (size_t i = first_new; i < obstacles.size() && free; ++i) {
            const Obstacle& o = obstacles[i];
            if (o.lane == safe && s > o.box.min.z - POWERUP_CLEARANCE && s < o.box.max.z + POWERUP_CLEARANCE)
                free = false;
        }
        for (size_t i = first_coin; i < coins.size() && free; ++i)
            if (coins[i].lane == safe && std::fabs(coins[i].s - s) < POWERUP_CLEARANCE)
                free = false;
        if (!free)
            continue;
        PowerUp p = {(PowerUpType)std::min((int)(kind * (float)POWERUP_TYPE_COUNT), POWERUP_TYPE_COUNT - 1),
                     safe, s, false};
        powerups.push_back(p);
        return;
    }
}
