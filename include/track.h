#ifndef _TRACK_H
#define _TRACK_H

#include <random>
#include <vector>

#include <glm/vec4.hpp>

// Pista do Rail Rush: sequência de segmentos (reta/curva/túnel) com
// referencial local em qualquer distância s ao longo dela. Sem OpenGL.
//
// Convenções (ver plano): heading h em torno de Y;
//   forward = (-sin h, 0, -cos h),  right = (cos h, 0, -sin h).
// Com h = 0 o jogador corre para -Z do mundo. curvature = 1/R; positivo =
// curva à esquerda (h aumenta com a distância percorrida).

// Gerador da pista (Tarefa 4.1): alterna retas e curvas. A 1ª reta (largada)
// é mais longa, para não haver curva logo no começo da corrida. Os ângulos
// estão em radianos (45° e 90°). Trens, rampas e túneis só ficam em retas
// (o spawn de Game confere com IsStraightRange).
//
// Túneis (Tarefa 4.2): depois de uma reta que veio de uma curva, com
// probabilidade TRACK_TUNNEL_PROBABILITY o gerador põe um túnel (segmento reto
// com tunnel = true) seguido de outra reta comum, antes da próxima curva:
// reta, túnel, reta. Assim o portal é visto de frente, de longe, e a saída dá
// numa reta. Nunca na reta da largada. O comprimento é múltiplo de
// TRACK_TUNNEL_SECTION (o comprimento de um módulo da malha do túnel).
const float TRACK_CURVE_RADIUS              = 40.0f;  // m (|curvature| = 1/40)
const float TRACK_CURVE_MIN_ANGLE           = 0.78539816f; // rad (45°)
const float TRACK_CURVE_MAX_ANGLE           = 1.57079633f; // rad (90°)
const float TRACK_STRAIGHT_MIN_LENGTH       = 60.0f;  // m
const float TRACK_STRAIGHT_MAX_LENGTH       = 150.0f; // m
const float TRACK_FIRST_STRAIGHT_MIN_LENGTH = 100.0f; // m (reta da largada)
const float TRACK_TUNNEL_PROBABILITY        = 1.0f / 3.0f; // por reta elegível
const float TRACK_TUNNEL_SECTION            = 4.0f;   // m (módulo da malha do túnel)
const float TRACK_TUNNEL_MIN_LENGTH         = 40.0f;  // m (múltiplo de TRACK_TUNNEL_SECTION)
const float TRACK_TUNNEL_MAX_LENGTH         = 80.0f;  // m (idem)

struct TrackSegment {
    float s_start;          // distância (ao longo da pista) em que o segmento começa
    float length;           // m
    float curvature;        // 1/R; 0 = reta, > 0 = esquerda, < 0 = direita
    bool tunnel;
    glm::vec4 start_position; // ponto (w = 1) em que o segmento começa
    float start_heading;    // heading no início do segmento
};

struct TrackFrame {
    glm::vec4 position;  // w = 1
    glm::vec4 forward;   // w = 0
    glm::vec4 right;     // w = 0
    float heading;
};

class Track {
public:
    explicit Track(unsigned seed);

    // Anexa segmentos sorteados até cobrir s (o fim do último segmento passa
    // de s). Alterna reta (TRACK_STRAIGHT_*_LENGTH) e curva (raio
    // TRACK_CURVE_RADIUS, ângulo total entre TRACK_CURVE_MIN/MAX_ANGLE, para a
    // esquerda ou a direita), às vezes com um túnel no meio da reta (veja
    // acima), sempre na mesma sequência para a mesma semente.
    void EnsureGeneratedUntil(float s);

    // Descarta segmentos que terminam antes de s. Sempre mantém ao menos o
    // último segmento, e os mantidos preservam seu referencial (cada segmento
    // guarda posição/heading iniciais).
    void DiscardBefore(float s);

    // Anexa um segmento logo após o último. Usado pelo gerador e pelos testes
    // (para montar curvas/túneis diretamente).
    void AppendSegment(float length, float curvature, bool tunnel);

    // Referencial em s. Fora da faixa de segmentos mantidos, usa a fórmula do
    // primeiro (s antes dele) ou do último segmento (s depois dele)
    // estendida além do seu comprimento: extrapola sem falhar. Quem chama deve
    // usar EnsureGeneratedUntil antes para ter valores significativos à frente.
    TrackFrame FrameAt(float s) const;

    bool IsTunnelAt(float s) const;

    // true se algum segmento de túnel mantido cruza o intervalo [s0, s1].
    // Usado para tirar prédios e postes de perto dos túneis.
    bool TunnelOverlaps(float s0, float s1) const;

    // true se todo o intervalo [s0, s1] está em segmentos mantidos e retos
    // (curvature == 0); pode atravessar a emenda entre duas retas. Usado para
    // só pôr trens/rampas/túneis em retas.
    bool IsStraightRange(float s0, float s1) const;

    // Segmentos mantidos, em ordem de s (para testes e depuração).
    const std::vector<TrackSegment>& Segments() const { return segments_; }

    // Segmento que contém s; nullptr se s está fora dos segmentos mantidos.
    const TrackSegment* SegmentAt(float s) const;

    // Faixa de malha entre lateral [x0,x1] na altura y, de s0 a s1 em passos de
    // step. Gera 2 vértices por passo (ordem de triangle strip: x0 depois x1),
    // cada um com 4 floats em positions/normals (vec4) e 2 em uvs.
    // UV: u = (x - x0)/(x1 - x0) * u_repeat, v = s / v_tile.
    // Os vetores são acrescentados (append), não limpos.
    void BuildStrip(float s0, float s1, float step, float x0, float x1, float y,
                    float u_repeat, float v_tile,
                    std::vector<float>* positions, std::vector<float>* normals,
                    std::vector<float>* uvs) const;

private:
    static TrackFrame FrameInSegment(const TrackSegment& seg, float s);

    // Sorteia e anexa o próximo segmento (reta depois de curva e vice-versa,
    // às vezes reta, túnel, reta; veja o comentário do topo).
    void AppendGeneratedSegment();
    float Random01(); // [0, 1), portável (só usa a saída do mt19937)
    float RandomStraightLength(); // [TRACK_STRAIGHT_MIN_LENGTH, MAX); 1 sorteio

    std::vector<TrackSegment> segments_;
    std::mt19937 rng_;
    // A última reta veio de uma curva (pode ser seguida de um túnel). Falso
    // na reta da largada e na reta de saída de um túnel.
    bool tunnel_candidate_;
};

#endif // _TRACK_H
