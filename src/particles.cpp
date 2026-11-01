#include "particles.h"

#include <algorithm>
#include <cmath>
#include <random>

// Sorteio em [0, 1) portável: usa só a saída bruta do mt19937 (as distribuições
// da biblioteca padrão dão resultados diferentes entre compiladores, e as
// explosões precisam ser reproduzíveis a partir da semente). 24 bits cabem
// exatamente num float.
static float Random01(std::mt19937& rng)
{
    return (float)(rng() >> 8) * (1.0f / 16777216.0f);
}

static float RandomRange(std::mt19937& rng, float lo, float hi)
{
    return lo + (hi - lo) * Random01(rng);
}

static bool ShorterLife(const Particle& a, const Particle& b)
{
    return a.life < b.life;
}

ParticleSystem::ParticleSystem(size_t capacity) : capacity_(capacity)
{
    // Toda a memória é reservada aqui: Emit só faz push_back até capacity_ e
    // Update só remove, então o vetor nunca realoca durante o jogo.
    particles_.reserve(capacity_);
}

void ParticleSystem::Emit(glm::vec4 world_pos, int count, glm::vec3 color, unsigned seed,
                          float floor_y)
{
    std::mt19937 rng(seed);
    // Mais que a capacidade não cabe: as excedentes só substituiriam outras
    // partículas da mesma explosão.
    count = std::min(count, (int)capacity_);
    if (count <= 0)
        return;
    // Pool cheio: as 'overflow' partículas de menor vida restante (as que iam
    // sumir primeiro) vão para o começo do vetor (nth_element: sem alocar; a
    // ordem de Alive() não importa) e são substituídas pelas novas. As novas
    // nunca substituem umas às outras.
    const size_t total = particles_.size() + (size_t)count;
    const size_t overflow = (total > capacity_) ? total - capacity_ : 0;
    if (overflow > 0 && overflow < particles_.size())
        std::nth_element(particles_.begin(), particles_.begin() + overflow, particles_.end(),
                         ShorterLife);
    for (int i = 0; i < count; ++i) {
        // Direção uniforme no hemisfério superior: cos(theta) = y uniforme em
        // [0, 1] e azimute uniforme (a densidade por área da esfera é uniforme
        // em y). O sorteio é feito sempre na mesma ordem para ser determinístico.
        const float y = Random01(rng);
        const float phi = 6.2831853f * Random01(rng);
        const float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float speed = RandomRange(rng, PARTICLE_MIN_SPEED, PARTICLE_MAX_SPEED);

        Particle p;
        p.position = glm::vec4(world_pos.x, world_pos.y, world_pos.z, 1.0f);
        p.velocity = glm::vec4(r * std::cos(phi) * speed, y * speed, r * std::sin(phi) * speed, 0.0f);
        p.max_life = RandomRange(rng, PARTICLE_MIN_LIFE, PARTICLE_MAX_LIFE);
        p.life = p.max_life;
        p.size = RandomRange(rng, PARTICLE_MIN_SIZE, PARTICLE_MAX_SIZE);
        p.color = color;
        p.floor_y = floor_y;
        if ((size_t)i < overflow)
            particles_[i] = p;
        else
            particles_.push_back(p);
    }
}

void ParticleSystem::Update(float dt)
{
    dt = std::max(dt, 0.0f);
    size_t i = 0;
    while (i < particles_.size()) {
        Particle& p = particles_[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            // Remoção O(1): a última ocupa a vaga (e é processada em seguida).
            // Não incrementa i. Sem apagar do meio do vetor nem realocar.
            p = particles_.back();
            particles_.pop_back();
            continue;
        }
        // Movimento uniformemente variado, exato para qualquer dt:
        // p += v dt + g dt^2 / 2 (g em -y);  v += g dt.
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt - 0.5f * PARTICLE_GRAVITY * dt * dt;
        p.position.z += p.velocity.z * dt;
        p.velocity.y -= PARTICLE_GRAVITY * dt;
        // Chão: o centro do sprite fica a meio tamanho acima dele (o quad não
        // atravessa o chão). Quem chega lá quica, perdendo velocidade.
        const float rest_y = p.floor_y + 0.5f * p.size;
        if (p.position.y < rest_y) {
            p.position.y = rest_y;
            if (p.velocity.y < 0.0f)
                p.velocity.y = -PARTICLE_BOUNCE * p.velocity.y;
            p.velocity.x *= PARTICLE_GROUND_FRICTION;
            p.velocity.z *= PARTICLE_GROUND_FRICTION;
        }
        ++i;
    }
}

void ParticleSystem::Clear()
{
    particles_.clear();
}
