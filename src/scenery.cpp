#include "scenery.h"

#include <algorithm>
#include <cmath>

#include "game_config.h"

namespace {

// Acrescenta uma faixa [x0,x1] na altura y ao lote, cobrindo o trecho visível.
// tile = metros por repetição da textura nas duas direções (sem esticar).
void AppendStrip(const Track& track, float player_s, float x0, float x1, float y, float tile,
                 StripBatch* out)
{
    const int first = (int)out->VertexCount();
    track.BuildStrip(player_s - DESPAWN_BEHIND, player_s + SPAWN_AHEAD, SCENERY_STEP,
                     x0, x1, y, (x1 - x0) / tile, tile,
                     &out->positions, &out->normals, &out->uvs);
    const int count = (int)out->VertexCount() - first;
    if (count > 0) {
        out->first.push_back(first);
        out->count.push_back(count);
    }
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
    // BuildStrip gera ceil(L/step) + 1 pares; +2 pares de folga para erros de
    // arredondamento de float com s grande.
    const float length = DESPAWN_BEHIND + SPAWN_AHEAD;
    const size_t pairs = (size_t)std::ceil(length / SCENERY_STEP) + 3;
    return 2 * pairs;
}

void BuildGroundStrips(const Track& track, float player_s, StripBatch* out)
{
    out->Clear();
    // Leito da pista e terreno: as bordas coincidem exatamente (mesmo cálculo),
    // então não há frestas; U contínuo porque as larguras são múltiplas do ladrilho.
    AppendStrip(track, player_s, -TRACK_BED_HALF_WIDTH, TRACK_BED_HALF_WIDTH, 0.0f,
                GROUND_TEXTURE_TILE, out);
    AppendStrip(track, player_s, -TERRAIN_HALF_WIDTH, -TRACK_BED_HALF_WIDTH, 0.0f,
                GROUND_TEXTURE_TILE, out);
    AppendStrip(track, player_s, TRACK_BED_HALF_WIDTH, TERRAIN_HALF_WIDTH, 0.0f,
                GROUND_TEXTURE_TILE, out);
}

void BuildRailStrips(const Track& track, float player_s, StripBatch* out)
{
    out->Clear();
    for (int lane = -1; lane <= 1; ++lane) {
        for (int side = -1; side <= 1; side += 2) {
            const float center = lane * LANE_WIDTH + side * 0.5f * RAIL_GAUGE;
            AppendStrip(track, player_s, center - 0.5f * RAIL_WIDTH, center + 0.5f * RAIL_WIDTH,
                        RAIL_HEIGHT, RAIL_TEXTURE_TILE, out);
        }
    }
}

void ComputeBuildings(float player_s, std::vector<BuildingInstance>* out)
{
    out->clear();
    const int k0 = (int)std::ceil((player_s - DESPAWN_BEHIND) / BUILDING_SPACING);
    const int k1 = (int)std::floor((player_s + SPAWN_AHEAD) / BUILDING_SPACING);
    for (int k = k0; k <= k1; ++k) {
        for (int side = -1; side <= 1; side += 2) {
            // Hash inteiro de (k, lado) -> altura em {8, 10, 12, 14, 16} m.
            // Aritmética unsigned: bem definida também para k negativo.
            unsigned h = (unsigned)(2 * k + (side > 0 ? 1 : 0)) * 2654435761u;
            h ^= h >> 15;
            BuildingInstance b;
            b.s = k * BUILDING_SPACING;
            b.x = side * BUILDING_OFFSET_X;
            b.width = BUILDING_WIDTH;
            b.height = BUILDING_MIN_HEIGHT + 2.0f * (float)(h % 5u);
            b.depth = BUILDING_DEPTH;
            out->push_back(b);
        }
    }
}

void ComputeLamps(float player_s, std::vector<LampInstance>* out)
{
    out->clear();
    const int k0 = (int)std::ceil((player_s - DESPAWN_BEHIND) / LAMP_SPACING);
    const int k1 = (int)std::floor((player_s + SPAWN_AHEAD) / LAMP_SPACING);
    for (int k = k0; k <= k1; ++k) {
        for (int side = -1; side <= 1; side += 2) {
            LampInstance l;
            l.s = k * LAMP_SPACING;
            l.x = side * LAMP_OFFSET_X;
            out->push_back(l);
        }
    }
}

namespace {

struct IndexedDistance {
    float dist2;
    int index;
};

bool CloserFirst(const IndexedDistance& a, const IndexedDistance& b)
{
    if (a.dist2 != b.dist2)
        return a.dist2 < b.dist2;
    return a.index < b.index; // desempate estável
}

} // namespace

void PickNearestPoints(const std::vector<glm::vec3>& points, const glm::vec3& from,
                       int max_count, std::vector<int>* out_indices)
{
    out_indices->clear();
    if (max_count <= 0)
        return;
    std::vector<IndexedDistance> all(points.size());
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
