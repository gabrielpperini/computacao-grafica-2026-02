#include "track.h"

#include <algorithm>
#include <cmath>

namespace {

// Abaixo disso a curvatura é tratada como zero (reta).
const float kMinCurvature = 1e-6f;

} // namespace

Track::Track(unsigned seed) : rng_(seed), tunnel_candidate_(false)
{
    // A pista nunca fica vazia: já nasce com a reta da largada (s = 0), mais
    // longa que as demais para a corrida não começar numa curva.
    const float len = TRACK_FIRST_STRAIGHT_MIN_LENGTH +
                      (TRACK_STRAIGHT_MAX_LENGTH - TRACK_FIRST_STRAIGHT_MIN_LENGTH) * Random01();
    AppendSegment(len, 0.0f, false);
}

float Track::Random01()
{
    // 24 bits da saída do mt19937 (sequência fixada pelo padrão), em vez de
    // std::uniform_real_distribution, cujo resultado varia entre bibliotecas:
    // a mesma semente gera a mesma pista em qualquer compilador.
    return (float)(rng_() >> 8) * (1.0f / 16777216.0f);
}

void Track::AppendGeneratedSegment()
{
    const TrackSegment& last = segments_.back();
    if (std::fabs(last.curvature) >= kMinCurvature) {
        // Depois de uma curva, uma reta (que pode ganhar um túnel em seguida).
        AppendSegment(RandomStraightLength(), 0.0f, false);
        tunnel_candidate_ = true;
        return;
    }
    if (last.tunnel) {
        // Depois de um túnel, a reta de saída (sem novo túnel depois dela).
        AppendSegment(RandomStraightLength(), 0.0f, false);
        tunnel_candidate_ = false;
        return;
    }
    // Depois de uma reta comum: túnel (só se a reta veio de uma curva; o
    // sorteio só acontece nesse caso) ou curva.
    const bool candidate = tunnel_candidate_;
    tunnel_candidate_ = false;
    if (candidate && Random01() < TRACK_TUNNEL_PROBABILITY) {
        // Comprimento em múltiplos de TRACK_TUNNEL_SECTION, de MIN a MAX.
        const int steps = (int)((TRACK_TUNNEL_MAX_LENGTH - TRACK_TUNNEL_MIN_LENGTH) /
                                TRACK_TUNNEL_SECTION + 0.5f);
        const int k = std::min(steps, (int)(Random01() * (float)(steps + 1)));
        AppendSegment(TRACK_TUNNEL_MIN_LENGTH + k * TRACK_TUNNEL_SECTION, 0.0f, true);
        return;
    }
    // Curva: ângulo total sorteado em [45°, 90°] e lado sorteado;
    // comprimento = ângulo * raio.
    const float angle = TRACK_CURVE_MIN_ANGLE +
                        (TRACK_CURVE_MAX_ANGLE - TRACK_CURVE_MIN_ANGLE) * Random01();
    const float sign = (Random01() < 0.5f) ? 1.0f : -1.0f;
    AppendSegment(angle * TRACK_CURVE_RADIUS, sign / TRACK_CURVE_RADIUS, false);
}

float Track::RandomStraightLength()
{
    return TRACK_STRAIGHT_MIN_LENGTH +
           (TRACK_STRAIGHT_MAX_LENGTH - TRACK_STRAIGHT_MIN_LENGTH) * Random01();
}

void Track::AppendSegment(float length, float curvature, bool tunnel)
{
    TrackSegment seg;
    seg.length = length;
    seg.curvature = curvature;
    seg.tunnel = tunnel;
    if (segments_.empty()) {
        seg.s_start = 0.0f;
        seg.start_position = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        seg.start_heading = 0.0f;
    } else {
        // O novo segmento começa exatamente onde o anterior termina.
        const TrackSegment& prev = segments_.back();
        TrackFrame end = FrameInSegment(prev, prev.s_start + prev.length);
        seg.s_start = prev.s_start + prev.length;
        seg.start_position = end.position;
        seg.start_heading = end.heading;
    }
    segments_.push_back(seg);
}

void Track::EnsureGeneratedUntil(float s)
{
    // NaN não entra no laço (comparação falsa), mas +infinito o tornaria
    // infinito (e esgotaria a memória): s não finito é ignorado.
    if (!std::isfinite(s))
        return;
    while (segments_.back().s_start + segments_.back().length <= s)
        AppendGeneratedSegment();
}

void Track::DiscardBefore(float s)
{
    // Remove os segmentos que terminam antes de s, mas mantém sempre o último.
    size_t drop = 0;
    while (drop + 1 < segments_.size() &&
           segments_[drop].s_start + segments_[drop].length < s)
        ++drop;
    if (drop > 0)
        segments_.erase(segments_.begin(), segments_.begin() + drop);
}

const TrackSegment* Track::SegmentAt(float s) const
{
    if (segments_.empty() || s < segments_.front().s_start)
        return nullptr;
    // Último segmento cujo s_start <= s.
    size_t lo = 0, hi = segments_.size();
    while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (segments_[mid].s_start <= s)
            lo = mid;
        else
            hi = mid;
    }
    const TrackSegment& seg = segments_[lo];
    if (s >= seg.s_start + seg.length)
        return nullptr;
    return &seg;
}

bool Track::IsTunnelAt(float s) const
{
    const TrackSegment* seg = SegmentAt(s);
    return seg != nullptr && seg->tunnel;
}

float Track::TunnelDistanceAhead(float s, float max_dist) const
{
    float best = -1.0f;
    for (size_t i = 0; i < segments_.size(); ++i) {
        const TrackSegment& seg = segments_[i];
        if (!seg.tunnel)
            continue;
        if (s >= seg.s_start && s <= seg.s_start + seg.length)
            return 0.0f;
        const float d = seg.s_start - s;
        if (d > 0.0f && d <= max_dist && (best < 0.0f || d < best))
            best = d;
    }
    return best;
}

bool Track::TunnelOverlaps(float s0, float s1) const
{
    for (size_t i = 0; i < segments_.size(); ++i) {
        const TrackSegment& seg = segments_[i];
        if (seg.tunnel && seg.s_start <= s1 && s0 <= seg.s_start + seg.length)
            return true;
    }
    return false;
}

bool Track::IsStraightRange(float s0, float s1) const
{
    if (s1 < s0)
        return false;
    const TrackSegment* seg = SegmentAt(s0);
    if (seg == nullptr)
        return false;
    // Percorre os segmentos de s0 até o que contém s1; todos têm de ser retos.
    const TrackSegment* last = &segments_.back();
    while (true) {
        if (std::fabs(seg->curvature) >= kMinCurvature)
            return false;
        if (s1 < seg->s_start + seg->length)
            return true;
        if (seg == last)
            return false; // s1 além do que está gerado
        ++seg;
    }
}

float Track::StraightRunStart(float s) const
{
    const TrackSegment* seg = SegmentAt(s);
    if (seg == nullptr || std::fabs(seg->curvature) >= kMinCurvature)
        return s;
    const TrackSegment* first = &segments_.front();
    while (seg > first && std::fabs((seg - 1)->curvature) < kMinCurvature)
        --seg;
    return seg->s_start;
}

TrackFrame Track::FrameInSegment(const TrackSegment& seg, float s)
{
    // u pode ser negativo ou maior que seg.length (extrapolação): a fórmula
    // fechada continua válida.
    const float u = s - seg.s_start;
    const float k = seg.curvature;
    const float h0 = seg.start_heading;
    const float h = h0 + k * u;

    TrackFrame f;
    f.heading = h;
    f.forward = glm::vec4(-std::sin(h), 0.0f, -std::cos(h), 0.0f);
    f.right = glm::vec4(std::cos(h), 0.0f, -std::sin(h), 0.0f);

    if (std::fabs(k) < kMinCurvature) {
        // Reta: p = p0 + u * forward(h0)
        f.position = seg.start_position +
                     u * glm::vec4(-std::sin(h0), 0.0f, -std::cos(h0), 0.0f);
    } else {
        // Curva (arco de raio 1/k), forma fechada:
        //   x = x0 + (cos h - cos h0)/k,  z = z0 - (sin h - sin h0)/k
        f.position = glm::vec4(seg.start_position.x + (std::cos(h) - std::cos(h0)) / k,
                               seg.start_position.y,
                               seg.start_position.z - (std::sin(h) - std::sin(h0)) / k,
                               1.0f);
    }
    return f;
}

TrackFrame Track::FrameAt(float s) const
{
    const TrackSegment* seg = SegmentAt(s);
    if (seg == nullptr)
        seg = (s < segments_.front().s_start) ? &segments_.front() : &segments_.back();
    return FrameInSegment(*seg, s);
}

void Track::BuildStrip(float s0, float s1, float step, float x0, float x1, float y,
                       float u_repeat, float v_tile,
                       std::vector<float>* positions, std::vector<float>* normals,
                       std::vector<float>* uvs, StripUvMode uv_mode) const
{
    if (step <= 0.0f || s1 < s0 || v_tile == 0.0f || x1 == x0)
        return;

    // Número de passos; o último par fica exatamente em s1.
    const int n = std::max(1, (int)std::ceil((s1 - s0) / step - 1e-4f));
    const size_t pairs = (size_t)n + 1;
    positions->reserve(positions->size() + pairs * 2 * 4);
    normals->reserve(normals->size() + pairs * 2 * 4);
    uvs->reserve(uvs->size() + pairs * 2 * 2);

    // Origens das UV, múltiplas de v_tile (a diferença para a UV "absoluta" é
    // um número inteiro de repetições): UV pequenas, sem perder precisão de
    // float com s ou coordenadas de mundo grandes.
    const float v_origin = std::floor(s0 / v_tile) * v_tile;
    const TrackFrame f0 = FrameAt(s0);
    const float wx_origin = std::floor(f0.position.x / v_tile) * v_tile;
    const float wz_origin = std::floor(f0.position.z / v_tile) * v_tile;
    const float xc = 0.5f * (x0 + x1);

    const float xs[2] = {x0, x1};
    for (int i = 0; i <= n; ++i) {
        const float s = std::min(s0 + i * step, s1);
        const TrackFrame f = FrameAt(s);
        // Comprimento de arco da paralela à pista em x = xc: o ponto é
        // centro(s) + right(s) * xc e d(right)/ds = curvatura * forward, então
        // |dp/ds| = 1 + curvatura * xc, cuja integral é s + xc * heading(s)
        // (a menos de uma constante): o lado de fora da curva é mais longo.
        const float v_along = ((s - v_origin) + xc * f.heading) / v_tile;
        for (int k = 0; k < 2; ++k) {
            const float x = xs[k];
            const glm::vec4 p = f.position + f.right * x + glm::vec4(0.0f, y, 0.0f, 0.0f);
            positions->push_back(p.x);
            positions->push_back(p.y);
            positions->push_back(p.z);
            positions->push_back(1.0f);
            normals->push_back(0.0f); // chão plano: normal para cima
            normals->push_back(1.0f);
            normals->push_back(0.0f);
            normals->push_back(0.0f);
            if (uv_mode == STRIP_UV_WORLD_XZ) {
                uvs->push_back((p.x - wx_origin) / v_tile);
                uvs->push_back((p.z - wz_origin) / v_tile);
            } else {
                uvs->push_back((x - x0) / (x1 - x0) * u_repeat);
                uvs->push_back(v_along);
            }
        }
    }
}
