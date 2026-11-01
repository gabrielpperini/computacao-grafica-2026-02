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

// Chão: leito da pista (as 3 faixas, brita) e terreno dos dois lados, até
// além dos prédios (para não aparecer o céu "embaixo" do horizonte entre
// eles). As larguras são múltiplas dos ladrilhos das texturas.
const float TRACK_BED_HALF_WIDTH   = 4.5f;  // m
const float TERRAIN_HALF_WIDTH     = 22.5f; // m
const float GROUND_TEXTURE_TILE    = 4.5f;  // m por repetição da textura do terreno (U e V)
const float BED_TEXTURE_TILE       = 1.5f;  // m por repetição da textura da brita

// Dormentes: um por faixa a cada SLEEPER_SPACING, caixas de madeira apoiadas
// no chão (5 faces visíveis; a de baixo nunca aparece).
const float SLEEPER_SPACING        = 0.75f; // m entre dormentes consecutivos
const float SLEEPER_LENGTH         = 2.3f;  // m (lateral; < LANE_WIDTH, sobra um vão entre faixas)
const float SLEEPER_WIDTH          = 0.26f; // m (ao longo da pista)
const float SLEEPER_HEIGHT         = 0.06f; // m
const float SLEEPER_TEXTURE_TILE   = 1.2f;  // m por repetição da textura de madeira
const int   SLEEPER_FACES          = 5;     // topo, frente, trás e as duas pontas

// Trilhos: 2 por faixa, cada um uma "barra" sobre os dormentes: topo e as
// duas laterais (faixas ao longo da pista, sem Z-fighting com o chão).
const float RAIL_GAUGE             = 1.435f; // m entre os centros dos dois trilhos
const float RAIL_WIDTH             = 0.10f;  // m
const float RAIL_HEIGHT            = 0.18f;  // m: topo do trilho acima do chão
const float RAIL_TEXTURE_TILE      = 1.0f;   // m por repetição da textura

// Prédios: um a cada 12 m de cada lado, centrados em x = ±9, escolhidos entre
// BUILDING_VARIANT_COUNT modelos (main.cpp escala cada modelo de modo
// uniforme para a largura BUILDING_WIDTH; a altura vem do próprio modelo).
const float BUILDING_SPACING       = 12.0f;  // m entre prédios consecutivos
const float BUILDING_OFFSET_X      = 9.0f;   // m do centro da pista ao centro do prédio
const float BUILDING_WIDTH         = 6.0f;   // m (lateral)
const int   BUILDING_VARIANT_COUNT = 6;      // modelos de prédio diferentes
const int   BUILDING_TINT_COUNT    = 4;      // tons de pintura (multiplicam a textura)

// Postes de luz: um de cada lado a cada 20 m, com a haste entre o leito da
// pista e os prédios e o braço curvo apontando para a pista. A cabeça do poste
// (emissiva, a LAMP_HEIGHT do chão da pista) é também uma luz pontual da cena;
// main.cpp calcula a sua posição a partir do modelo (LampHeadPosition()).
const float LAMP_SPACING           = 20.0f;  // m entre postes consecutivos (por lado)
const float LAMP_OFFSET_X          = 5.5f;   // m do centro da pista ao poste
const float LAMP_HEIGHT            = 4.5f;   // m do chão ao centro da cabeça (= posição da luz)
const float LAMP_HEAD_SIZE         = 0.36f;  // m (lado do bloco emissivo da cabeça)
const float LAMP_HEAD_HEIGHT       = 0.16f;  // m (altura do bloco emissivo da cabeça)
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

const int GROUND_STRIP_COUNT = 3;  // leito (faixa 0) + terreno esquerdo + terreno direito
const int RAIL_STRIP_COUNT   = 18; // 3 faixas x 2 trilhos x (topo + 2 laterais)

// Limpa *out e gera as faixas de chão / de trilhos em torno de player_s. Em
// BuildGroundStrips a faixa 0 é o leito da pista (brita) e as 1 e 2 o
// terreno (desenhados com texturas diferentes).
void BuildGroundStrips(const Track& track, float player_s, StripBatch* out);
void BuildRailStrips(const Track& track, float player_s, StripBatch* out);

// Número máximo de vértices de BuildSleeperStrips (capacidade do buffer).
size_t MaxSleeperVertices();

// Limpa *out e gera os dormentes do trecho visível (s = k*SLEEPER_SPACING,
// em cada faixa): cada face é uma faixa de 4 vértices. As UV estão em metros
// (/SLEEPER_TEXTURE_TILE), com o veio da madeira ao longo do dormente.
void BuildSleeperStrips(const Track& track, float player_s, StripBatch* out);

// Um prédio: posição na pista (s, x lateral do centro), modelo e tom.
struct BuildingInstance {
    float s;
    float x;
    int variant; // [0, BUILDING_VARIANT_COUNT)
    int tint;    // [0, BUILDING_TINT_COUNT)
};

// Limpa *out e lista os prédios em s = k*BUILDING_SPACING no trecho visível,
// dos dois lados. Modelo e tom dependem só de (k, lado): o mesmo prédio não
// muda de um quadro para o outro.
void ComputeBuildings(float player_s, std::vector<BuildingInstance>* out);

// Um poste de luz: posição na pista (s, x lateral).
struct LampInstance {
    float s;
    float x;
};

// Limpa *out e lista os postes em s = k*LAMP_SPACING no trecho visível, dos
// dois lados.
void ComputeLamps(float player_s, std::vector<LampInstance>* out);

// Par (distância², índice) usado por PickNearestPoints.
struct PointDistance {
    float dist2;
    int index;
};

// Escolhe os (no máximo) max_count pontos mais próximos de 'from' pela
// distância euclidiana, do mais perto para o mais longe (empate: menor índice
// primeiro). Limpa *out_indices e o preenche com índices de 'points'; se houver
// menos de max_count pontos devolve todos. *scratch é memória de trabalho do
// chamador, reaproveitada entre chamadas (sem alocar a cada quadro). Usada
// para escolher as luzes pontuais que cabem no shader.
void PickNearestPoints(const std::vector<glm::vec3>& points, const glm::vec3& from,
                       int max_count, std::vector<PointDistance>* scratch,
                       std::vector<int>* out_indices);

#endif // _SCENERY_H
