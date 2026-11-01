// Testes de intersecção do jogo. O enunciado exige que todos fiquem neste
// arquivo; ele centraliza as funções usadas pelas interações:
//   - obstáculo (trem, barreira): AABB jogador x AABB obstáculo -> game over
//     (AABBIntersectsAABB), exceto quando o jogador pousa no teto (LandsOnTop);
//     as duas caixas são varridas no passo (SweepAABB, SweepAABBAlongZ), para
//     que nenhuma "atravesse" a outra entre dois quadros;
//   - moedas e power-ups: esfera do item x AABB do jogador -> coleta
//     (SphereIntersectsAABB);
//   - rampas e teto do trem: altura da rampa sob a pegada do jogador
//     (RampHeightUnder/RampHeightAt) e pouso em cima do trem (LandsOnTop,
//     com FootprintsOverlap), para o jogador subir e andar nos trens;
//   - teto do túnel: quanto a cabeça do jogador passou dele
//     (CeilingPenetration) e o teto efetivo na aproximação da boca
//     (TunnelApproachCeiling);
//   - câmera livre: ponto de mundo x volume interior do túnel
//     (PointInsideTunnel), para escurecer o sol/céu só dentro dele;
//   - ímã: esfera de alcance do jogador x esfera da moeda -> a moeda é atraída
//     (SphereIntersectsSphere).
//   - em pé sobre o teto do trem (StandsOnTop): o trem não mata quem está em
//     cima dele, e o teto vira o chão da câmera;
//   - trem móvel x obstáculo à frente na mesma faixa (StopBehindAlongZ): onde
//     ele para;
//   - spawn: moeda/power-up/moeda do porco x obstáculo ao longo de s
//     (SpanTouchesBoxAlongZ, PointNearBoxAlongZ), para nada nascer dentro ou
//     colado num obstáculo.
// Tudo em coordenadas de pista (x lateral, y altura, z = s), sem OpenGL.

#include "collisions.h"

#include <algorithm>
#include <cmath>

#include <glm/common.hpp>

#include "scenery.h" // TunnelCeilingHeight, TUNNEL_INNER_HALF_WIDTH

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

AABB SweepAABB(const AABB& before, const AABB& after)
{
    AABB box;
    box.min = glm::min(before.min, after.min);
    box.max = glm::max(before.max, after.max);
    return box;
}

AABB SweepAABBAlongZ(const AABB& now, float dz)
{
    AABB before = now;
    before.min.z -= dz;
    before.max.z -= dz;
    return SweepAABB(before, now);
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

bool RampHeightUnder(const AABB& ramp, const AABB& box, float* out_height)
{
    if (!FootprintsOverlap(box, ramp))
        return false;
    // Ponto da pegada mais alto na rampa: a frente (max.z) limitada à rampa;
    // em x qualquer ponto comum serve (a rampa não varia com x).
    float x = std::min(std::max(0.5f * (box.min.x + box.max.x), ramp.min.x), ramp.max.x);
    float s = std::min(std::max(box.max.z, ramp.min.z), ramp.max.z);
    return RampHeightAt(ramp, x, s, out_height);
}

bool RampSideBlocks(const AABB& ramp, const AABB& from, const AABB& to, float step_height,
                    float* out_x)
{
    float h;
    if (FootprintsOverlap(from, ramp) || !RampHeightUnder(ramp, to, &h))
        return false; // já estava sobre a rampa (ou entrou pela frente), ou não chegou nela
    if (h <= to.min.y + step_height)
        return false; // degrau pequeno (começo baixo da rampa, ou chegando de cima)
    // Para encostado na lateral, por fora (1 mm de folga: intervalos fechados).
    const float half = 0.5f * (to.max.x - to.min.x);
    const float gap = 1e-3f;
    const bool from_left = from.min.x + from.max.x < ramp.min.x + ramp.max.x;
    if (out_x)
        *out_x = from_left ? ramp.min.x - half - gap : ramp.max.x + half + gap;
    return true;
}

bool FootprintsOverlap(const AABB& a, const AABB& b)
{
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

namespace {

// Fração t em [0, 1] do quadro em que o intervalo [lo, hi] (indo de prev a
// now, linearmente) passa a tocar [o_lo, o_hi]. Supõe que no fim (t = 1) ele
// toca. 0 se já tocava no começo.
float EntryFraction(float prev_lo, float prev_hi, float now_lo, float now_hi,
                    float o_lo, float o_hi)
{
    if (prev_hi < o_lo) // vinha de antes (menor coordenada)
        return (o_lo - prev_hi) / (now_hi - prev_hi);
    if (prev_lo > o_hi) // vinha de depois
        return (prev_lo - o_hi) / (prev_lo - now_lo);
    return 0.0f;
}

} // namespace

bool LandsOnTop(const AABB& player_prev, const AABB& player_now,
                const AABB& obstacle, float tolerance)
{
    // Pegada atual sobre o obstáculo e base atual no teto ou abaixo dele.
    if (!FootprintsOverlap(player_now, obstacle))
        return false;
    const float roof = obstacle.max.y;
    if (player_now.min.y > roof + tolerance)
        return false;

    // Vinha de cima no quadro anterior?
    if (player_prev.min.y >= roof - tolerance)
        return true;

    // Ou estava na altura do teto no instante em que a pegada começou a tocar
    // a do obstáculo (o último eixo, x ou z, a passar a sobrepor), com a base
    // interpolada linearmente entre os quadros. Já tocava antes -> t = 0 ->
    // base anterior (abaixo do teto: batida).
    float tx = EntryFraction(player_prev.min.x, player_prev.max.x, player_now.min.x,
                             player_now.max.x, obstacle.min.x, obstacle.max.x);
    float tz = EntryFraction(player_prev.min.z, player_prev.max.z, player_now.min.z,
                             player_now.max.z, obstacle.min.z, obstacle.max.z);
    float t = std::min(std::max(std::max(tx, tz), 0.0f), 1.0f);
    float base = player_prev.min.y + t * (player_now.min.y - player_prev.min.y);
    return base >= roof - tolerance;
}

bool StandsOnTop(const AABB& player, const AABB& obstacle, float tolerance)
{
    return FootprintsOverlap(player, obstacle) && player.min.y >= obstacle.max.y - tolerance;
}

bool StopBehindAlongZ(const AABB& mover, const AABB& other, float gap, float* stop_z)
{
    const bool same_lane = mover.min.x <= other.max.x && mover.max.x >= other.min.x;
    if (!same_lane || other.max.z > mover.min.z + 1e-3f)
        return false;
    if (stop_z)
        *stop_z = other.max.z + gap;
    return true;
}

bool SpanTouchesBoxAlongZ(float s0, float s1, const AABB& box, float margin)
{
    return s1 >= box.min.z - margin && s0 <= box.max.z + margin;
}

bool PointNearBoxAlongZ(float s, const AABB& box, float margin)
{
    return s > box.min.z - margin && s < box.max.z + margin;
}

float CeilingPenetration(const AABB& box, float ceiling)
{
    return std::max(box.max.y - ceiling, 0.0f);
}

float TunnelApproachCeiling(float ceiling, float distance, float slope)
{
    return ceiling + slope * std::max(distance, 0.0f);
}

bool PointInsideTunnelSegment(const TrackSegment& seg, glm::vec3 point)
{
    if (!seg.tunnel)
        return false;
    // Túnel é reto: eixo = start_position + u * forward(start_heading).
    const float h = seg.start_heading;
    const float dx = point.x - seg.start_position.x;
    const float dz = point.z - seg.start_position.z;
    const float u = dx * (-std::sin(h)) + dz * (-std::cos(h));   // ao longo do eixo
    const float lat = dx * std::cos(h) + dz * (-std::sin(h));    // deslocamento lateral (direita +)
    const float y = point.y - seg.start_position.y;
    if (u < 0.0f || u > seg.length)
        return false;
    if (std::fabs(lat) > TUNNEL_INNER_HALF_WIDTH)
        return false;
    return y > 0.0f && y < TunnelCeilingHeight(lat);
}

bool PointInsideTunnel(const Track& track, glm::vec3 point)
{
    const std::vector<TrackSegment>& segs = track.Segments();
    for (size_t i = 0; i < segs.size(); ++i)
        if (PointInsideTunnelSegment(segs[i], point))
            return true;
    return false;
}
