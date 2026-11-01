#include "track.h"

#include <algorithm>
#include <cmath>

namespace {

// Abaixo disso a curvatura é tratada como zero (reta).
const float kMinCurvature = 1e-6f;

} // namespace

Track::Track(unsigned seed) : rng_(seed)
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
    if (std::fabs(segments_.back().curvature) < kMinCurvature) {
        // Depois de uma reta, uma curva: ângulo total sorteado em [45°, 90°]
        // e lado sorteado; comprimento = ângulo * raio.
        const float angle = TRACK_CURVE_MIN_ANGLE +
                            (TRACK_CURVE_MAX_ANGLE - TRACK_CURVE_MIN_ANGLE) * Random01();
        const float sign = (Random01() < 0.5f) ? 1.0f : -1.0f;
        AppendSegment(angle * TRACK_CURVE_RADIUS, sign / TRACK_CURVE_RADIUS, false);
    } else {
        // Depois de uma curva, uma reta.
        const float len = TRACK_STRAIGHT_MIN_LENGTH +
                          (TRACK_STRAIGHT_MAX_LENGTH - TRACK_STRAIGHT_MIN_LENGTH) * Random01();
        AppendSegment(len, 0.0f, false);
    }
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
                       std::vector<float>* uvs) const
{
    if (step <= 0.0f || s1 < s0 || v_tile == 0.0f || x1 == x0)
        return;

    // Número de passos; o último par fica exatamente em s1.
    const int n = std::max(1, (int)std::ceil((s1 - s0) / step - 1e-4f));
    const size_t pairs = (size_t)n + 1;
    positions->reserve(positions->size() + pairs * 2 * 4);
    normals->reserve(normals->size() + pairs * 2 * 4);
    uvs->reserve(uvs->size() + pairs * 2 * 2);

    const float xs[2] = {x0, x1};
    for (int i = 0; i <= n; ++i) {
        const float s = std::min(s0 + i * step, s1);
        const TrackFrame f = FrameAt(s);
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
            uvs->push_back((x - x0) / (x1 - x0) * u_repeat);
            uvs->push_back(s / v_tile);
        }
    }
}
