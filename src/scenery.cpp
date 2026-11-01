#include "scenery.h"

#include <algorithm>
#include <cmath>

#include "game_config.h"

namespace {

// Início das faixas do trecho visível: player_s - DESPAWN_BEHIND arredondado
// para baixo na grade de SCENERY_STEP. Com o início preso à grade, os vértices
// ficam sempre nos mesmos s (k * SCENERY_STEP): na curva o chão é facetado
// (segmentos de reta entre vértices do arco) e, se os vértices andassem junto
// com o jogador, as facetas "deslizariam" ao longo do arco a cada quadro.
float StripStart(float player_s)
{
    return std::floor((player_s - DESPAWN_BEHIND) / SCENERY_STEP) * SCENERY_STEP;
}

// Acrescenta uma faixa [x0,x1] na altura y ao lote, cobrindo o trecho visível.
// tile = metros por repetição da textura nas duas direções (sem esticar).
// uv_mode: STRIP_UV_WORLD_XZ no chão (brita/terreno não têm direção: mesma
// densidade em todo x, também nas curvas) e STRIP_UV_ALONG_TRACK nos trilhos
// (v = comprimento de arco do próprio trilho; veja StripUvMode em track.h).
void AppendStrip(const Track& track, float player_s, float x0, float x1, float y, float tile,
                 StripUvMode uv_mode, StripBatch* out)
{
    const int first = (int)out->VertexCount();
    track.BuildStrip(StripStart(player_s), player_s + SPAWN_AHEAD, SCENERY_STEP,
                     x0, x1, y, (x1 - x0) / tile, tile,
                     &out->positions, &out->normals, &out->uvs, uv_mode);
    const int count = (int)out->VertexCount() - first;
    if (count > 0) {
        out->first.push_back(first);
        out->count.push_back(count);
    }
}

// Acrescenta uma faixa VERTICAL (parede) na lateral x, de y0 a y1, cobrindo o
// trecho visível, com a normal para +right (outward_right) ou -right. Mesmos
// passos de Track::BuildStrip (mesmo número de vértices). UV em metros/tile:
// u na altura e v no comprimento de arco da parede (s + x * heading, como
// STRIP_UV_ALONG_TRACK), sem esticar nas curvas.
void AppendWallStrip(const Track& track, float player_s, float x, float y0, float y1,
                     bool outward_right, float tile, StripBatch* out)
{
    const float s0 = StripStart(player_s);
    const float s1 = player_s + SPAWN_AHEAD;
    const int n = std::max(1, (int)std::ceil((s1 - s0) / SCENERY_STEP - 1e-4f));
    const int first = (int)out->VertexCount();
    // Ordem de cada par: com normal +right, (topo, base); com -right, (base,
    // topo). Assim os triângulos da faixa ficam anti-horários vistos de fora.
    const float ys[2] = {outward_right ? y1 : y0, outward_right ? y0 : y1};
    const float sign = outward_right ? 1.0f : -1.0f;
    const float v_origin = std::floor(s0 / tile) * tile; // múltiplo de tile: v pequeno
    for (int i = 0; i <= n; ++i) {
        const float s = std::min(s0 + i * SCENERY_STEP, s1);
        const TrackFrame f = track.FrameAt(s);
        const float v = ((s - v_origin) + x * f.heading) / tile;
        for (int k = 0; k < 2; ++k) {
            const glm::vec4 p = f.position + f.right * x + glm::vec4(0.0f, ys[k], 0.0f, 0.0f);
            out->positions.push_back(p.x);
            out->positions.push_back(p.y);
            out->positions.push_back(p.z);
            out->positions.push_back(1.0f);
            out->normals.push_back(sign * f.right.x);
            out->normals.push_back(sign * f.right.y);
            out->normals.push_back(sign * f.right.z);
            out->normals.push_back(0.0f);
            out->uvs.push_back((ys[k] - y0) / tile);
            out->uvs.push_back(v);
        }
    }
    out->first.push_back(first);
    out->count.push_back((int)out->VertexCount() - first);
}

// Acrescenta uma face retangular (faixa de 4 vértices) dada pelos cantos na
// ordem de triangle strip vista de fora: base-esq, base-dir, topo-esq, topo-dir.
void AppendQuad(const glm::vec4 corners[4], const glm::vec4& normal, const float uv[8],
                StripBatch* out)
{
    out->first.push_back((int)out->VertexCount());
    out->count.push_back(4);
    for (int k = 0; k < 4; ++k) {
        out->positions.push_back(corners[k].x);
        out->positions.push_back(corners[k].y);
        out->positions.push_back(corners[k].z);
        out->positions.push_back(1.0f);
        out->normals.push_back(normal.x);
        out->normals.push_back(normal.y);
        out->normals.push_back(normal.z);
        out->normals.push_back(0.0f);
        out->uvs.push_back(uv[2 * k + 0]);
        out->uvs.push_back(uv[2 * k + 1]);
    }
}

// Canto de um dormente centrado em c: a = -1/+1 ao longo de right (ponta),
// b = -1/+1 ao longo de forward (trás/frente), y = altura.
glm::vec4 SleeperCorner(const glm::vec4& c, const TrackFrame& f, float a, float b, float y)
{
    return c + f.right * (a * 0.5f * SLEEPER_LENGTH) + f.forward * (b * 0.5f * SLEEPER_WIDTH)
             + glm::vec4(0.0f, y, 0.0f, 0.0f);
}

} // namespace

void StripBatch::Clear()
{
    positions.clear();
    normals.clear();
    uvs.clear();
    first.clear();
    count.clear();
}

size_t MaxVerticesPerStrip()
{
    // BuildStrip gera ceil(L/step) + 1 pares. Com o início preso à grade
    // (StripStart), L vai até DESPAWN_BEHIND + SPAWN_AHEAD + SCENERY_STEP:
    // +1 par por isso e +2 de folga para erros de arredondamento de float
    // com s grande.
    const float length = DESPAWN_BEHIND + SPAWN_AHEAD + SCENERY_STEP;
    const size_t pairs = (size_t)std::ceil(length / SCENERY_STEP) + 3;
    return 2 * pairs;
}

void BuildGroundStrips(const Track& track, float player_s, StripBatch* out)
{
    out->Clear();
    // Leito da pista e terreno: as bordas coincidem exatamente (mesmo cálculo),
    // então não há frestas; as larguras são múltiplas dos ladrilhos.
    AppendStrip(track, player_s, -TRACK_BED_HALF_WIDTH, TRACK_BED_HALF_WIDTH, 0.0f,
                BED_TEXTURE_TILE, STRIP_UV_WORLD_XZ, out);
    AppendStrip(track, player_s, -TERRAIN_HALF_WIDTH, -TRACK_BED_HALF_WIDTH, 0.0f,
                GROUND_TEXTURE_TILE, STRIP_UV_WORLD_XZ, out);
    AppendStrip(track, player_s, TRACK_BED_HALF_WIDTH, TERRAIN_HALF_WIDTH, 0.0f,
                GROUND_TEXTURE_TILE, STRIP_UV_WORLD_XZ, out);
}

void BuildRailStrips(const Track& track, float player_s, StripBatch* out)
{
    out->Clear();
    for (int lane = -1; lane <= 1; ++lane) {
        for (int side = -1; side <= 1; side += 2) {
            const float center = lane * LANE_WIDTH + side * 0.5f * RAIL_GAUGE;
            const float x0 = center - 0.5f * RAIL_WIDTH;
            const float x1 = center + 0.5f * RAIL_WIDTH;
            AppendStrip(track, player_s, x0, x1, RAIL_HEIGHT, RAIL_TEXTURE_TILE, STRIP_UV_ALONG_TRACK, out);
            AppendWallStrip(track, player_s, x0, SLEEPER_HEIGHT, RAIL_HEIGHT, false,
                            RAIL_TEXTURE_TILE, out);
            AppendWallStrip(track, player_s, x1, SLEEPER_HEIGHT, RAIL_HEIGHT, true,
                            RAIL_TEXTURE_TILE, out);
        }
    }
}

size_t MaxSleeperVertices()
{
    const float length = DESPAWN_BEHIND + SPAWN_AHEAD;
    const size_t per_lane = (size_t)std::ceil(length / SLEEPER_SPACING) + 2;
    return per_lane * 3 * SLEEPER_FACES * 4;
}

void BuildSleeperStrips(const Track& track, float player_s, StripBatch* out)
{
    out->Clear();
    const int k0 = (int)std::ceil((player_s - DESPAWN_BEHIND) / SLEEPER_SPACING);
    const int k1 = (int)std::floor((player_s + SPAWN_AHEAD) / SLEEPER_SPACING);
    const glm::vec4 up(0.0f, 1.0f, 0.0f, 0.0f);
    const float h = SLEEPER_HEIGHT;
    const float t = 1.0f / SLEEPER_TEXTURE_TILE;
    const float L = SLEEPER_LENGTH * t, W = SLEEPER_WIDTH * t, H = h * t;
    for (int k = k0; k <= k1; ++k) {
        const TrackFrame f = track.FrameAt(k * SLEEPER_SPACING);
        // Deslocamento de textura por dormente (hash de k): nem todos iguais.
        unsigned hsh = (unsigned)k * 2654435761u;
        hsh ^= hsh >> 13;
        const float du = (float)(hsh % 97u) / 97.0f;
        for (int lane = -1; lane <= 1; ++lane) {
            const glm::vec4 c = f.position + f.right * (lane * LANE_WIDTH);
            // Topo (visto de cima, com +forward para "cima" na tela). O veio
            // da madeira (V da textura) acompanha o comprimento do dormente.
            const glm::vec4 top[4] = {SleeperCorner(c, f, -1, -1, h), SleeperCorner(c, f, 1, -1, h),
                                      SleeperCorner(c, f, -1, 1, h), SleeperCorner(c, f, 1, 1, h)};
            const float uv_top[8] = {du, 0.0f, du, L, du + W, 0.0f, du + W, L};
            AppendQuad(top, up, uv_top, out);
            // Face de trás (voltada para o jogador, normal -forward) e da frente.
            const glm::vec4 back[4] = {SleeperCorner(c, f, -1, -1, 0), SleeperCorner(c, f, 1, -1, 0),
                                       SleeperCorner(c, f, -1, -1, h), SleeperCorner(c, f, 1, -1, h)};
            const glm::vec4 front[4] = {SleeperCorner(c, f, 1, 1, 0), SleeperCorner(c, f, -1, 1, 0),
                                        SleeperCorner(c, f, 1, 1, h), SleeperCorner(c, f, -1, 1, h)};
            const float uv_side[8] = {du, 0.0f, du, L, du + H, 0.0f, du + H, L};
            AppendQuad(back, -f.forward, uv_side, out);
            AppendQuad(front, f.forward, uv_side, out);
            // Pontas (normais -right e +right).
            const glm::vec4 left[4] = {SleeperCorner(c, f, -1, 1, 0), SleeperCorner(c, f, -1, -1, 0),
                                       SleeperCorner(c, f, -1, 1, h), SleeperCorner(c, f, -1, -1, h)};
            const glm::vec4 right[4] = {SleeperCorner(c, f, 1, -1, 0), SleeperCorner(c, f, 1, 1, 0),
                                        SleeperCorner(c, f, 1, -1, h), SleeperCorner(c, f, 1, 1, h)};
            const float uv_end[8] = {du, 0.0f, du + W, 0.0f, du, H, du + W, H};
            AppendQuad(left, -f.right, uv_end, out);
            AppendQuad(right, f.right, uv_end, out);
        }
    }
}

void ComputeBuildings(const Track& track, float player_s, std::vector<BuildingInstance>* out)
{
    out->clear();
    const int k0 = (int)std::ceil((player_s - DESPAWN_BEHIND) / BUILDING_SPACING);
    const int k1 = (int)std::floor((player_s + SPAWN_AHEAD) / BUILDING_SPACING);
    for (int k = k0; k <= k1; ++k) {
        for (int side = -1; side <= 1; side += 2) {
            // Hash inteiro de (k, lado) -> modelo e tom do prédio.
            // Aritmética unsigned: bem definida também para k negativo.
            unsigned h = (unsigned)(2 * k + (side > 0 ? 1 : 0)) * 2654435761u;
            h ^= h >> 15;
            BuildingInstance b;
            b.s = k * BUILDING_SPACING;
            // Nada de prédio encostado no túnel ou no portal.
            const float reach = 0.5f * BUILDING_WIDTH + TUNNEL_PORTAL_DEPTH + TUNNEL_CLEAR_MARGIN;
            if (track.TunnelOverlaps(b.s - reach, b.s + reach))
                continue;
            b.x = side * BUILDING_OFFSET_X;
            b.variant = (int)(h % (unsigned)BUILDING_VARIANT_COUNT);
            b.tint = (int)((h >> 8) % (unsigned)BUILDING_TINT_COUNT);
            out->push_back(b);
        }
    }
}

void ComputeLamps(const Track& track, float player_s, std::vector<LampInstance>* out, float behind)
{
    out->clear();
    const int k0 = (int)std::ceil((player_s - behind) / LAMP_SPACING);
    const int k1 = (int)std::floor((player_s + SPAWN_AHEAD) / LAMP_SPACING);
    const float reach = TUNNEL_PORTAL_DEPTH + TUNNEL_CLEAR_MARGIN;
    for (int k = k0; k <= k1; ++k) {
        // Dentro do túnel a iluminação é a do teto (ComputeTunnelLamps).
        if (track.TunnelOverlaps(k * LAMP_SPACING - reach, k * LAMP_SPACING + reach))
            continue;
        for (int side = -1; side <= 1; side += 2) {
            LampInstance l;
            l.s = k * LAMP_SPACING;
            l.x = side * LAMP_OFFSET_X;
            out->push_back(l);
        }
    }
}

namespace {

bool CloserFirst(const PointDistance& a, const PointDistance& b)
{
    if (a.dist2 != b.dist2)
        return a.dist2 < b.dist2;
    return a.index < b.index; // desempate estável
}

} // namespace

void PickNearestPoints(const std::vector<glm::vec3>& points, const glm::vec3& from,
                       int max_count, std::vector<PointDistance>* scratch,
                       std::vector<int>* out_indices)
{
    out_indices->clear();
    if (max_count <= 0)
        return;
    // resize não realoca depois que a capacidade já cobre points.size().
    std::vector<PointDistance>& all = *scratch;
    all.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        const glm::vec3 d = points[i] - from;
        all[i].dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
        all[i].index = (int)i;
    }
    std::sort(all.begin(), all.end(), CloserFirst);
    const size_t n = std::min(all.size(), (size_t)max_count);
    for (size_t i = 0; i < n; ++i)
        out_indices->push_back(all[i].index);
}

// ------------------------------------- Fade das luzes (Revisão final, item 5)

float PointLightWeight(float dist, float cutoff, float fade_width)
{
    if (!(fade_width > 0.0f))
        return dist < cutoff ? 1.0f : 0.0f;
    const float x = std::min(std::max((cutoff - dist) / fade_width, 0.0f), 1.0f);
    return x * x * (3.0f - 2.0f * x); // smoothstep: sem degrau nem quina
}

float LightEdgePenalty(float light_s, float player_s)
{
    const float ramp_start = LIGHT_CANDIDATE_BEHIND - LIGHT_EDGE_FADE_LENGTH; // m atrás do jogador
    const float u = (player_s - light_s - ramp_start) / LIGHT_EDGE_FADE_LENGTH;
    return std::min(std::max(u, 0.0f), 1.0f) * LIGHT_EDGE_PENALTY;
}

void PickPointLights(const std::vector<glm::vec3>& points, const std::vector<float>& extra_dist,
                     const glm::vec3& from, int max_count, std::vector<PointDistance>* scratch,
                     std::vector<int>* out_indices, std::vector<float>* out_weights)
{
    out_indices->clear();
    out_weights->clear();
    if (max_count <= 0)
        return;
    // Aqui o campo dist2 guarda a distância EFETIVA (não ao quadrado).
    std::vector<PointDistance>& all = *scratch;
    all.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        const glm::vec3 d = points[i] - from;
        const float extra = (i < extra_dist.size()) ? extra_dist[i] : 0.0f;
        all[i].dist2 = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z) + extra;
        all[i].index = (int)i;
    }
    std::sort(all.begin(), all.end(), CloserFirst);
    const size_t n = std::min(all.size(), (size_t)max_count);
    float cutoff = (all.size() > n) ? all[n].dist2 : POINT_LIGHT_MAX_DIST;
    cutoff = std::min(cutoff, POINT_LIGHT_MAX_DIST);
    for (size_t i = 0; i < n; ++i) {
        out_indices->push_back(all[i].index);
        out_weights->push_back(PointLightWeight(all[i].dist2, cutoff, POINT_LIGHT_FADE_WIDTH));
    }
}

// ---------------------------------------------------------------- Túneis (4.2)

float TunnelCeilingHeight(float x)
{
    const float r = TUNNEL_INNER_HALF_WIDTH;
    if (std::fabs(x) > r)
        return 0.0f;
    return TUNNEL_SPRING_HEIGHT + std::sqrt(r * r - x * x);
}

void ComputeVisibleTunnels(const Track& track, float player_s, std::vector<TunnelSpan>* out)
{
    out->clear();
    const float v0 = player_s - DESPAWN_BEHIND;
    const float v1 = player_s + SPAWN_AHEAD;
    const std::vector<TrackSegment>& segs = track.Segments();
    for (size_t i = 0; i < segs.size(); ++i) {
        const TrackSegment& g = segs[i];
        if (!g.tunnel)
            continue;
        TunnelSpan span = {g.s_start, g.s_start + g.length};
        // Os portais ficam fora do segmento: entram na conta da visibilidade.
        if (span.s1 + TUNNEL_PORTAL_DEPTH >= v0 && span.s0 - TUNNEL_PORTAL_DEPTH <= v1)
            out->push_back(span);
    }
}

void ComputeTunnelPieces(const Track& track, float player_s, std::vector<TunnelPiece>* out)
{
    out->clear();
    const float v0 = player_s - DESPAWN_BEHIND;
    const float v1 = player_s + SPAWN_AHEAD;
    const std::vector<TrackSegment>& segs = track.Segments();
    for (size_t i = 0; i < segs.size(); ++i) {
        if (!segs[i].tunnel)
            continue;
        const float s0 = segs[i].s_start;
        const float s1 = s0 + segs[i].length;
        // Portal de entrada em [s0 - DEPTH, s0].
        if (s0 >= v0 && s0 - TUNNEL_PORTAL_DEPTH <= v1) {
            TunnelPiece p = {s0 - 0.5f * TUNNEL_PORTAL_DEPTH, TUNNEL_PIECE_ENTRY};
            out->push_back(p);
        }
        // Módulos [s0 + k*L, s0 + (k+1)*L]; o comprimento do túnel é múltiplo
        // de L (gerador), então o último termina exatamente em s1.
        const int n = (int)((s1 - s0) / TUNNEL_SECTION_LENGTH + 0.5f);
        for (int k = 0; k < n; ++k) {
            const float a = s0 + k * TUNNEL_SECTION_LENGTH;
            if (a + TUNNEL_SECTION_LENGTH < v0 || a > v1)
                continue;
            TunnelPiece p = {a + 0.5f * TUNNEL_SECTION_LENGTH, TUNNEL_PIECE_SECTION};
            out->push_back(p);
        }
        // Portal de saída em [s1, s1 + DEPTH].
        if (s1 + TUNNEL_PORTAL_DEPTH >= v0 && s1 <= v1) {
            TunnelPiece p = {s1 + 0.5f * TUNNEL_PORTAL_DEPTH, TUNNEL_PIECE_EXIT};
            out->push_back(p);
        }
    }
}

void ComputeTunnelLamps(const Track& track, float player_s, std::vector<float>* out_s, float behind)
{
    out_s->clear();
    const float v0 = player_s - behind;
    const float v1 = player_s + SPAWN_AHEAD;
    const std::vector<TrackSegment>& segs = track.Segments();
    for (size_t i = 0; i < segs.size(); ++i) {
        if (!segs[i].tunnel)
            continue;
        const float s0 = segs[i].s_start;
        const float s1 = s0 + segs[i].length;
        for (int k = 0;; ++k) {
            const float s = s0 + 0.5f * TUNNEL_LAMP_SPACING + k * TUNNEL_LAMP_SPACING;
            if (s >= s1)
                break;
            if (s >= v0 && s <= v1)
                out_s->push_back(s);
        }
    }
}

float StepTunnelDimming(float dimming, bool camera_in_tunnel, float dt)
{
    if (dt <= 0.0f)
        return dimming;
    const float target = camera_in_tunnel ? 1.0f : 0.0f;
    const float step = dt / TUNNEL_DIM_TIME;
    if (dimming < target)
        return std::min(target, dimming + step);
    return std::max(target, dimming - step);
}
