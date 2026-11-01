#ifndef _SCENERY_H
#define _SCENERY_H

#include <cstddef>
#include <vector>

#include "track.h"

// Geometria do cenário derivada da pista: faixas (triangle strips) de chão e
// trilhos e a disposição dos prédios ao lado da pista. Sem OpenGL: main.cpp
// só envia o resultado para a GPU. Tudo é gerado no trecho visível
// [s - DESPAWN_BEHIND, s + SPAWN_AHEAD] em torno do jogador, que é exatamente
// o trecho que o Game mantém gerado.

// Passo (m) ao longo da pista entre pares de vértices das faixas.
const float SCENERY_STEP           = 1.0f;

// Chão: leito da pista (as 3 faixas) e terreno dos dois lados, até além dos
// prédios (para não aparecer o céu "embaixo" do horizonte entre eles). As
// larguras são múltiplas do ladrilho da textura, então U é contínuo nas emendas.
const float TRACK_BED_HALF_WIDTH   = 4.5f;  // m
const float TERRAIN_HALF_WIDTH     = 22.5f; // m
const float GROUND_TEXTURE_TILE    = 4.5f;  // m por repetição da textura (U e V)

// Trilhos: 2 por faixa, faixas finas um pouco acima do chão (sem Z-fighting).
const float RAIL_GAUGE             = 1.435f; // m entre os centros dos dois trilhos
const float RAIL_WIDTH             = 0.12f;  // m
const float RAIL_HEIGHT            = 0.03f;  // m acima do chão
const float RAIL_TEXTURE_TILE      = 1.0f;   // m por repetição da textura

// Prédios provisórios: caixas a cada 12 m, centradas em x = ±9.
const float BUILDING_SPACING       = 12.0f;  // m entre prédios consecutivos
const float BUILDING_OFFSET_X      = 9.0f;   // m do centro da pista ao centro do prédio
const float BUILDING_WIDTH         = 6.0f;   // m (lateral)
const float BUILDING_DEPTH         = 10.0f;  // m (ao longo da pista)
const float BUILDING_MIN_HEIGHT    = 8.0f;   // m; alturas em {8, 10, 12, 14, 16}

// Lote de faixas: vértices concatenados de várias faixas independentes, cada
// uma desenhada com glDrawArrays(GL_TRIANGLE_STRIP, first[i], count[i]).
struct StripBatch {
    std::vector<float> positions; // vec4 (w = 1), coordenadas de mundo
    std::vector<float> normals;   // vec4 (w = 0)
    std::vector<float> uvs;       // vec2
    std::vector<int> first;       // vértice inicial de cada faixa
    std::vector<int> count;       // número de vértices de cada faixa

    void Clear();
    size_t VertexCount() const { return positions.size() / 4; }
};

// Número máximo de vértices de UMA faixa do trecho visível (com folga para
// arredondamento). Capacidade dos buffers da GPU = num_faixas * isto.
size_t MaxVerticesPerStrip();

const int GROUND_STRIP_COUNT = 3; // leito + terreno esquerdo + terreno direito
const int RAIL_STRIP_COUNT   = 6; // 3 faixas x 2 trilhos

// Limpa *out e gera as faixas de chão / de trilhos em torno de player_s.
void BuildGroundStrips(const Track& track, float player_s, StripBatch* out);
void BuildRailStrips(const Track& track, float player_s, StripBatch* out);

// Um prédio: posição na pista (s, x lateral do centro) e dimensões em metros.
struct BuildingInstance {
    float s;
    float x;
    float width;   // lateral
    float height;
    float depth;   // ao longo da pista
};

// Limpa *out e lista os prédios em s = k*BUILDING_SPACING no trecho visível,
// dos dois lados. A altura depende só de (k, lado): o mesmo prédio mantém a
// altura de um quadro para o outro.
void ComputeBuildings(float player_s, std::vector<BuildingInstance>* out);

#endif // _SCENERY_H
