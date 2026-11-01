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

    // Anexa segmentos até cobrir s (o fim do último segmento passa de s).
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

    std::vector<TrackSegment> segments_;
    std::mt19937 rng_;
};

#endif // _TRACK_H
