#include "game.h"

#include <algorithm>
#include <cmath>

#include <glm/common.hpp>

namespace {

// Mistura a semente do jogo para o gerador do spawn, de modo que a sequência
// não seja a mesma do gerador da pista (que usa a semente pura).
const unsigned kSpawnSeedMix = 0x9E3779B9u;
// Idem para o sorteio de quais trens são móveis: gerador separado, para que
// ele não altere a sequência de tipos/posições dos obstáculos (rng_).
const unsigned kMovingSeedMix = 0x85EBCA6Bu;

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

Game::Game(unsigned seed)
    : mode(GameMode::PLAYING), track(seed), elapsed(0.0f), coins_collected(0),
      score(0.0f), multiplier(1), spawn_enabled(true), seed_(seed),
      rng_(seed ^ kSpawnSeedMix), next_chunk_(0), safe_lane_(0),
      moving_rng_(seed ^ kMovingSeedMix)
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
    coins_collected = 0;
    score = 0.0f;
    multiplier = 1;
    next_chunk_ = 0;
    safe_lane_ = 0;
    safe_lanes_.clear();
    obstacle_dz_.clear();

    rng_.seed(seed_ ^ kSpawnSeedMix);
    moving_rng_.seed(seed_ ^ kMovingSeedMix);
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

    // Trens móveis andam neste passo (a caixa deles anda junto).
    MoveObstacles(dt);

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
    AABB parts[3];
    for (size_t i = 0; i < obstacles.size() && mode == GameMode::PLAYING; ++i) {
        int n = ObstacleParts(obstacles[i], parts);
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

    // Moedas: esfera da moeda x caixa do jogador -> coletada (uma única vez).
    for (size_t i = 0; i < coins.size(); ++i) {
        Coin& c = coins[i];
        if (c.collected)
            continue;
        glm::vec3 center(c.lane * LANE_WIDTH, c.y, c.s);
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
                               [behind](const Coin& c) { return c.s < behind; }),
                coins.end());
}

AABB Game::PlayerBox() const
{
    float h = (player.roll_timer > 0.0f) ? PLAYER_ROLL_HEIGHT : PLAYER_HEIGHT;
    AABB b;
    b.min = glm::vec3(player.x - PLAYER_HALF_WIDTH, player.y, player.s - PLAYER_HALF_DEPTH);
    b.max = glm::vec3(player.x + PLAYER_HALF_WIDTH, player.y + h, player.s + PLAYER_HALF_DEPTH);
    return b;
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
//    placas); barreiras, placas e moedas podem ficar nas curvas.
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

        // Trens (e rampas, Tarefa 4.4) só em retas: um trem de 13 m numa curva
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
            obstacles.push_back(o);
        }
    }

    // Fileira de moedas na faixa segura, pulando as que cairiam numa barreira.
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
}
