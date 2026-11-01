#ifndef _PARTICLES_H
#define _PARTICLES_H

#include <cstddef>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

// Sistema de partículas da coleta de moedas e power-ups (Tarefa 6.1; funcionalidade
// extra do SPEC). Sem OpenGL: pode ser testado em tests/logic_tests.cpp. O desenho
// (billboards aditivos) fica em main.cpp.
//
// Cada partícula nasce no ponto da coleta, com posição, direção, velocidade e tempo
// de vida próprios, se move sob gravidade e some ao fim da vida. Todo o movimento é
// multiplicado por dt (e integrado de forma exata para aceleração constante, então o
// resultado não depende do tamanho do passo).

struct Particle {
    glm::vec4 position;  // coordenadas de mundo (w = 1)
    glm::vec4 velocity;  // m/s (w = 0)
    float life;          // tempo de vida restante (s)
    float max_life;      // tempo de vida total (s)
    float size;          // lado do quad no nascimento (m); o desenho o encolhe com life/max_life
    glm::vec3 color;     // cor RGB (a mesma ao longo da vida; o alfa decai no desenho)
    float floor_y;       // altura do chão sob a explosão (y de mundo); PARTICLE_NO_FLOOR = sem chão
};

// Parâmetros de emissão e física.
const size_t PARTICLE_CAPACITY      = 512;
const float  PARTICLE_GRAVITY       = 6.0f;   // m/s²
const float  PARTICLE_MIN_SPEED     = 2.0f;   // m/s
const float  PARTICLE_MAX_SPEED     = 6.0f;
const float  PARTICLE_MIN_LIFE      = 0.4f;   // s
const float  PARTICLE_MAX_LIFE      = 0.9f;
const float  PARTICLE_MIN_SIZE      = 0.20f;  // m
const float  PARTICLE_MAX_SIZE      = 0.45f;
// Chão (Tarefa 7.1b): ao chegar a meio tamanho do chão (o sprite não é cortado
// por ele), a partícula quica: v.y troca de sinal e é multiplicada por
// PARTICLE_BOUNCE, e a velocidade horizontal por PARTICLE_GROUND_FRICTION.
const float  PARTICLE_BOUNCE          = 0.35f;
const float  PARTICLE_GROUND_FRICTION = 0.6f;
const float  PARTICLE_NO_FLOOR        = -1e30f; // floor_y padrão: cai sem chão

class ParticleSystem {
public:
    // Reserva todo o armazenamento agora: nenhuma alocação depois (Emit e Update
    // nunca realocam o vetor).
    explicit ParticleSystem(size_t capacity = PARTICLE_CAPACITY);

    // Cria até 'count' partículas em world_pos, com direção aleatória no
    // hemisfério superior (y >= 0), velocidade em [PARTICLE_MIN_SPEED,
    // PARTICLE_MAX_SPEED], vida em [PARTICLE_MIN_LIFE, PARTICLE_MAX_LIFE] e
    // tamanho em [PARTICLE_MIN_SIZE, PARTICLE_MAX_SIZE]. O gerador (mt19937) é
    // semeado com 'seed': mesma semente, mesma explosão. floor_y é a altura do
    // chão sob a explosão (as partículas quicam nele; veja Update). Com o pool
    // cheio cada partícula nova SUBSTITUI a de menor vida restante (a que ia
    // sumir primeiro): a explosão da coleta sempre aparece, e Alive().size()
    // nunca passa da capacidade (count > capacidade: só 'capacidade' nascem).
    void Emit(glm::vec4 world_pos, int count, glm::vec3 color, unsigned seed,
              float floor_y = PARTICLE_NO_FLOOR);

    // Avança dt segundos (dt < 0 vira 0): gravidade PARTICLE_GRAVITY em -y,
    // posição integrada com dt, quique no chão (o centro nunca fica abaixo de
    // floor_y + size/2) e remove (troca com a última) as de vida <= 0.
    void Update(float dt);

    // Remove todas as partículas (reinício da corrida).
    void Clear();

    // Partículas vivas (a ordem não é a de nascimento).
    const std::vector<Particle>& Alive() const { return particles_; }

    size_t Capacity() const { return capacity_; }

private:
    size_t capacity_;
    std::vector<Particle> particles_;
};

#endif // _PARTICLES_H
