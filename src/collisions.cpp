// Testes de intersecção do jogo. O enunciado exige que todos fiquem neste
// arquivo; ele centraliza as funções usadas pelas interações:
//   - obstáculo (trem, barreira): AABB jogador x AABB obstáculo -> game over
//     (AABBIntersectsAABB), exceto quando o jogador pousa no teto (LandsOnTop);
//   - moedas e power-ups: esfera do item x AABB do jogador -> coleta
//     (SphereIntersectsAABB);
//   - rampas e teto do trem: altura do chão sob o jogador (RampHeightAt) e
//     pouso em cima do trem (LandsOnTop), para o jogador poder andar nos trens;
//   - ímã: esfera de alcance do jogador x esfera da moeda -> a moeda é atraída
//     (SphereIntersectsSphere).
// Tudo em coordenadas de pista (x lateral, y altura, z = s), sem OpenGL.

#include "collisions.h"

#include <algorithm>
#include <cmath>

AABB MakeAABB(glm::vec3 center, glm::vec3 half_extents)
{
    glm::vec3 h(std::fabs(half_extents.x), std::fabs(half_extents.y),
                std::fabs(half_extents.z));
    AABB box;
    box.min = center - h;
    box.max = center + h;
    return box;
}

bool AABBIntersectsAABB(const AABB& a, const AABB& b)
{
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

bool SphereIntersectsAABB(glm::vec3 center, float radius, const AABB& box)
{
    // Ponto da caixa mais próximo do centro: o centro limitado à caixa.
    glm::vec3 closest(std::min(std::max(center.x, box.min.x), box.max.x),
                      std::min(std::max(center.y, box.min.y), box.max.y),
                      std::min(std::max(center.z, box.min.z), box.max.z));
    glm::vec3 d = center - closest;
    return d.x * d.x + d.y * d.y + d.z * d.z <= radius * radius;
}

bool SphereIntersectsSphere(glm::vec3 c1, float r1, glm::vec3 c2, float r2)
{
    glm::vec3 d = c1 - c2;
    float r = r1 + r2;
    return d.x * d.x + d.y * d.y + d.z * d.z <= r * r;
}

bool RampHeightAt(const AABB& ramp, float x, float s, float* out_height)
{
    if (x < ramp.min.x || x > ramp.max.x || s < ramp.min.z || s > ramp.max.z)
        return false;

    float length = ramp.max.z - ramp.min.z;
    // Rampa sem comprimento: evita divisão por zero (degrau na altura máxima).
    float t = length > 0.0f ? (s - ramp.min.z) / length : 1.0f;
    if (out_height)
        *out_height = ramp.min.y + t * (ramp.max.y - ramp.min.y);
    return true;
}

bool LandsOnTop(const AABB& player_prev, const AABB& player_now,
                const AABB& obstacle, float tolerance)
{
    // Sobreposição lateral (x) e ao longo da pista (z) no quadro atual.
    bool overlaps_xz = player_now.min.x <= obstacle.max.x &&
                       player_now.max.x >= obstacle.min.x &&
                       player_now.min.z <= obstacle.max.z &&
                       player_now.max.z >= obstacle.min.z;
    if (!overlaps_xz)
        return false;

    float roof = obstacle.max.y;
    // Vinha de cima (base anterior no teto ou acima) e agora a base está no
    // teto ou abaixo dele. Base anterior abaixo do teto = batida frontal.
    return player_prev.min.y >= roof - tolerance &&
           player_now.min.y <= roof + tolerance;
}
