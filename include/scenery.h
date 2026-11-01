#ifndef _SCENERY_H
#define _SCENERY_H

#include <cstddef>
#include <vector>

#include <glm/vec3.hpp>

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

// Postes de luz: um de cada lado a cada 20 m, entre o leito da pista e os
// prédios. A cabeça do poste (emissiva) é também uma luz pontual da cena.
const float LAMP_SPACING           = 20.0f;  // m entre postes consecutivos (por lado)
const float LAMP_OFFSET_X          = 5.5f;   // m do centro da pista ao poste
const float LAMP_HEIGHT            = 4.5f;   // m do chão ao centro da cabeça (= posição da luz)
const float LAMP_POLE_WIDTH        = 0.2f;   // m (seção quadrada da haste)
const float LAMP_HEAD_SIZE         = 0.5f;   // m (lado da cabeça)
const float LAMP_HEAD_HEIGHT       = 0.3f;   // m (altura da cabeça)
const int   MAX_POINT_LIGHTS       = 8;      // luzes pontuais enviadas ao shader

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

// Um poste de luz: posição na pista (s, x lateral).
struct LampInstance {
    float s;
    float x;
};

// Limpa *out e lista os postes em s = k*LAMP_SPACING no trecho visível, dos
// dois lados.
void ComputeLamps(float player_s, std::vector<LampInstance>* out);

// Escolhe os (no máximo) max_count pontos mais próximos de 'from' pela
// distância euclidiana, do mais perto para o mais longe (empate: menor índice
// primeiro). Limpa *out_indices e o preenche com índices de 'points'; se houver
// menos de max_count pontos devolve todos. Usada para escolher as luzes
// pontuais que cabem no shader.
void PickNearestPoints(const std::vector<glm::vec3>& points, const glm::vec3& from,
                       int max_count, std::vector<int>* out_indices);

#endif // _SCENERY_H
