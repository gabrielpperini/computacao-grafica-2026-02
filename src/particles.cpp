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

ParticleSystem::ParticleSystem(size_t capacity) : capacity_(capacity)
{
    // Toda a memória é reservada aqui: Emit só faz push_back até capacity_ e
    // Update só remove, então o vetor nunca realoca durante o jogo.
    particles_.reserve(capacity_);
}

void ParticleSystem::Emit(glm::vec4 world_pos, int count, glm::vec3 color, unsigned seed)
{
    std::mt19937 rng(seed);
    for (int i = 0; i < count && particles_.size() < capacity_; ++i) {
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
        ++i;
    }
}

void ParticleSystem::Clear()
{
    particles_.clear();
}
