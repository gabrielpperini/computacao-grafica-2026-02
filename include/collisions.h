#ifndef _COLLISIONS_H
#define _COLLISIONS_H

#include <glm/vec3.hpp>

// Testes de intersecção do jogo (ver src/collisions.cpp). Sem OpenGL.
// Tudo em coordenadas de pista: x = lateral, y = altura, z = s (distância
// ao longo da pista). Todos os intervalos são fechados: encostar conta.

// Caixa alinhada aos eixos. Invariante: min <= max em cada componente.
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

// Caixa a partir do centro e das semi-extensões. Semi-extensões negativas
// são tomadas em módulo, de modo que sempre vale min <= max.
AABB MakeAABB(glm::vec3 center, glm::vec3 half_extents);

// Interseção caixa-caixa (faces encostadas contam como interseção).
bool AABBIntersectsAABB(const AABB& a, const AABB& b);

// Esfera-caixa: ponto da caixa mais próximo do centro a distância <= raio.
bool SphereIntersectsAABB(glm::vec3 center, float radius, const AABB& box);

// Esfera-esfera (esferas tangentes contam como interseção).
bool SphereIntersectsSphere(glm::vec3 c1, float r1, glm::vec3 c2, float r2);

// Altura da rampa em (x, s). A rampa é a caixa `ramp`: sobe linearmente de
// ramp.min.y em s = ramp.min.z até ramp.max.y em s = ramp.max.z. Devolve false
// (sem tocar em *out_height) se (x, s) está fora da caixa.
bool RampHeightAt(const AABB& ramp, float x, float s, float* out_height);

// Jogador pousando sobre o teto de um obstáculo (ex.: trem): vindo de cima,
// ou seja, com a base anterior em ou acima do topo (menos a tolerância) e a
// base atual no topo ou abaixo dele (mais a tolerância), com sobreposição
// lateral e em s no quadro atual. Batida frontal (base anterior abaixo do
// topo) devolve false.
bool LandsOnTop(const AABB& player_prev, const AABB& player_now,
                const AABB& obstacle, float tolerance);

#endif
