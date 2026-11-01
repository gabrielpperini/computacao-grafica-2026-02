#ifndef _SCENERY_H
#define _SCENERY_H

#include <cstddef>
#include <vector>

#include <glm/vec3.hpp>

#include "game_config.h"
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
// terreno (desenhados com texturas diferentes). As faixas começam em
// player_s - DESPAWN_BEHIND arredondado para baixo na grade de SCENERY_STEP
// (vértices em s = k * SCENERY_STEP, fixos na pista: nas curvas o chão
// facetado não "desliza" de um quadro para o outro) e vão até
// player_s + SPAWN_AHEAD.
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
// muda de um quadro para o outro. Prédios que encostariam num túnel (ou nos
// seus portais, com folga TUNNEL_CLEAR_MARGIN) são omitidos.
void ComputeBuildings(const Track& track, float player_s, std::vector<BuildingInstance>* out);

// Um poste de luz: posição na pista (s, x lateral).
struct LampInstance {
    float s;
    float x;
};

// Limpa *out e lista os postes em s = k*LAMP_SPACING no trecho visível, dos
// dois lados, exceto ao longo dos túneis e junto aos portais (o poste em
// x = ±LAMP_OFFSET_X atravessaria a parede do túnel). 'behind' = quantos
// metros para trás do jogador a lista começa (padrão: o trecho visível; as
// luzes usam LIGHT_CANDIDATE_BEHIND, veja o fade das luzes abaixo).
void ComputeLamps(const Track& track, float player_s, std::vector<LampInstance>* out,
                  float behind = DESPAWN_BEHIND);

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

// ---------------------------------------------------------------------------
// Fade das luzes pontuais (Revisão final, item 5)
// ---------------------------------------------------------------------------
// Só as MAX_POINT_LIGHTS luzes mais próximas vão para o shader. Sem cuidado,
// uma luz "acende" de uma vez quando entra nesse conjunto (e apaga quando
// sai). Por isso cada luz escolhida tem um peso que multiplica a sua cor: 1
// longe do corte e caindo suavemente até 0 quando a distância dela chega à
// da 1ª luz que ficou de fora (o "raio de corte"; no máximo
// POINT_LIGHT_MAX_DIST). Na troca da 8ª pela 9ª as duas estão no corte, com
// peso 0. Uma luz que sai da lista por trás teria o mesmo problema; por
// isso as candidatas vão até LIGHT_CANDIDATE_BEHIND m para trás (bem além do
// trecho visível: lá atrás uma luz quase nunca está entre as 9 mais
// próximas) e, nos últimos LIGHT_EDGE_FADE_LENGTH m, a "distância efetiva"
// ganha LightEdgePenalty(), que cresce continuamente até LIGHT_EDGE_PENALTY:
// quando a luz sai da lista ela já está além do corte (peso 0).
const float POINT_LIGHT_FADE_WIDTH = 8.0f;  // m antes do corte em que o peso vai de 1 a 0
const float POINT_LIGHT_MAX_DIST   = 60.0f; // m: corte máximo (uma luz a 60 m quase não ilumina)
const float LIGHT_CANDIDATE_BEHIND = 60.0f; // m atrás do jogador em que ainda há luzes candidatas
const float LIGHT_EDGE_FADE_LENGTH = 25.0f; // m (antes da ponta de trás) em que a penalidade cresce
const float LIGHT_EDGE_PENALTY     = 60.0f; // m somados à distância na ponta de trás
// A pista precisa existir até onde há luzes candidatas (FrameAt delas):
// DESPAWN_BEHIND + TRACK_KEEP_MARGIN >= LIGHT_CANDIDATE_BEHIND (conferido em
// logic_tests, Test_PointLightWeight).

// Peso (0..1) de uma luz a 'dist' m do jogador com o corte em 'cutoff' m:
// smoothstep de 0 (dist >= cutoff) a 1 (dist <= cutoff - fade_width).
float PointLightWeight(float dist, float cutoff, float fade_width);

// Penalidade (m) somada à distância de uma luz em light_s: 0 até
// LIGHT_CANDIDATE_BEHIND - LIGHT_EDGE_FADE_LENGTH m atrás do jogador,
// crescendo linearmente até LIGHT_EDGE_PENALTY em LIGHT_CANDIDATE_BEHIND m
// atrás (onde a luz sai da lista de candidatas) e além.
float LightEdgePenalty(float light_s, float player_s);

// Escolhe as (no máximo) max_count luzes de menor distância efetiva
// (|points[i] - from| + extra_dist[i]; extra_dist vazio = 0) e o peso de cada
// uma (PointLightWeight com o corte = distância efetiva da 1ª que ficou de
// fora, limitada a POINT_LIGHT_MAX_DIST, e POINT_LIGHT_FADE_WIDTH). Limpa e
// preenche *out_indices (da mais perto para a mais longe) e *out_weights
// (mesma ordem). *scratch: memória de trabalho reaproveitada (sem alocar).
void PickPointLights(const std::vector<glm::vec3>& points, const std::vector<float>& extra_dist,
                     const glm::vec3& from, int max_count, std::vector<PointDistance>* scratch,
                     std::vector<int>* out_indices, std::vector<float>* out_weights);

// ---------------------------------------------------------------------------
// Túneis (Tarefa 4.2)
// ---------------------------------------------------------------------------
// Seção transversal interna (intradorso): paredes verticais em
// x = ±TUNNEL_INNER_HALF_WIDTH, de y = 0 a TUNNEL_SPRING_HEIGHT, e por cima uma
// abóbada semicircular de raio TUNNEL_INNER_HALF_WIDTH (topo a 7 m: trens de
// 3.2 m passam com folga em todas as faixas). Parede de espessura
// TUNNEL_WALL_THICKNESS. O túnel não tem piso próprio: o chão é o leito e o
// terreno de BuildGroundStrips (sem Z-fighting). As malhas vêm de
// tools/gen_models.py (data/procedural/tunnel_section.obj e
// tunnel_portal.obj), que usa ESTES MESMOS valores: alterar lá e aqui juntos.
const float TUNNEL_INNER_HALF_WIDTH  = 5.0f;  // m (cobre o leito, ±TRACK_BED_HALF_WIDTH)
const float TUNNEL_SPRING_HEIGHT     = 2.0f;  // m (altura das paredes retas)
const float TUNNEL_WALL_THICKNESS    = 0.6f;  // m
const float TUNNEL_SECTION_LENGTH    = TRACK_TUNNEL_SECTION; // m por módulo (instância)
// Portal (fachada de entrada/saída): parede de 2*HALF_WIDTH x HEIGHT x DEPTH
// com a abertura do perfil interno; fica fora do segmento (antes da entrada /
// depois da saída), encostado no 1º/último módulo.
const float TUNNEL_PORTAL_HALF_WIDTH = 7.5f;  // m
const float TUNNEL_PORTAL_HEIGHT     = 9.0f;  // m
const float TUNNEL_PORTAL_DEPTH      = 1.0f;  // m (ao longo da pista)
// Folga além dos portais sem prédios nem postes de rua.
const float TUNNEL_CLEAR_MARGIN      = 1.5f;  // m
// Lâmpadas do teto: uma a cada TUNNEL_LAMP_SPACING, a partir de meio
// espaçamento depois da entrada, no centro da pista. A luz pontual fica em
// TUNNEL_LAMP_HEIGHT (logo abaixo da luminária emissiva).
const float TUNNEL_LAMP_SPACING      = 10.0f; // m
const float TUNNEL_LAMP_HEIGHT       = 6.5f;  // m
// Escurecimento com a câmera dentro do túnel: o sol é multiplicado por
// TUNNEL_SUN_FACTOR e o ambiente por TUNNEL_AMBIENT_FACTOR, numa rampa linear
// de TUNNEL_DIM_TIME s (sem "piscar" na entrada/saída).
const float TUNNEL_DIM_TIME          = 0.5f;  // s
const float TUNNEL_SUN_FACTOR        = 0.2f;
const float TUNNEL_AMBIENT_FACTOR    = 0.6f;

// Altura do teto (intradorso) na lateral x; 0 fora de |x| <= TUNNEL_INNER_HALF_WIDTH.
float TunnelCeilingHeight(float x);

// Peça do túnel desenhada como instância: módulo de TUNNEL_SECTION_LENGTH ou
// portal (de entrada, voltado para quem chega; ou de saída, girado 180°).
const int TUNNEL_PIECE_SECTION = 0;
const int TUNNEL_PIECE_ENTRY   = 1;
const int TUNNEL_PIECE_EXIT    = 2;
struct TunnelPiece {
    float s;   // centro da peça ao longo da pista
    int kind;  // TUNNEL_PIECE_*
};

// Trecho [s0, s1] de um túnel.
struct TunnelSpan {
    float s0;
    float s1;
};

// Limpa *out e lista os túneis (segmentos com tunnel = true) que cruzam o
// trecho visível [player_s - DESPAWN_BEHIND, player_s + SPAWN_AHEAD] (contando
// os portais), em ordem de s.
void ComputeVisibleTunnels(const Track& track, float player_s, std::vector<TunnelSpan>* out);

// Limpa *out e lista as peças (módulos a cada TUNNEL_SECTION_LENGTH a partir
// da entrada, mais os dois portais) que cruzam o trecho visível.
void ComputeTunnelPieces(const Track& track, float player_s, std::vector<TunnelPiece>* out);

// Limpa *out_s e lista o s das lâmpadas de teto do trecho visível (a partir
// de 'behind' m atrás do jogador, como em ComputeLamps).
void ComputeTunnelLamps(const Track& track, float player_s, std::vector<float>* out_s,
                        float behind = DESPAWN_BEHIND);

// Um passo da rampa de escurecimento (0 = fora, 1 = câmera dentro do túnel):
// anda dt / TUNNEL_DIM_TIME em direção ao alvo, sem passar dele.
float StepTunnelDimming(float dimming, bool camera_in_tunnel, float dt);

#endif // _SCENERY_H
