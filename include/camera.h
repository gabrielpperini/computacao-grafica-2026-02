#ifndef _CAMERA_H
#define _CAMERA_H

#include <glm/vec4.hpp>

#include "track.h"

// Câmeras do Rail Rush. Sem OpenGL: devolvem só a pose (posição, view vector e
// up), que main.cpp transforma em matriz com Matrix_Camera_View().

struct CameraPose {
    glm::vec4 position;     // ponto c (w = 1)
    glm::vec4 view_vector;  // sentido para onde a câmera olha (w = 0)
    glm::vec4 up;           // vetor "up" (w = 0)
};

// Parâmetros da câmera em 3ª pessoa (m, no referencial da pista do jogador).
const float CAMERA_BACK            = 6.0f;  // recuo atrás do jogador, fora do túnel
const float CAMERA_HEIGHT          = 3.5f;  // altura acima do chão, fora do túnel
const float CAMERA_TUNNEL_BACK     = 4.0f;  // dentro do túnel: mais perto ...
const float CAMERA_TUNNEL_HEIGHT   = 2.2f;  // ... e mais baixa, para não atravessar o teto
const float CAMERA_LOOK_AHEAD      = 4.0f;  // olha para jogador + forward*4 ...
const float CAMERA_LOOK_HEIGHT     = 1.0f;  // ... + up*1
const float CAMERA_LATERAL_FACTOR  = 1.0f / 3.0f; // fração do deslocamento lateral seguida
const float CAMERA_SMOOTHING_RATE  = 6.0f;  // 1/s, em (1 - exp(-rate*dt))

// Câmera look-at em 3ª pessoa, atrás e acima do jogador.
//
// A suavização exponencial pos += (alvo - pos) * (1 - exp(-6*dt)) é aplicada ao
// deslocamento da câmera no referencial da pista do jogador (lateral, altura e
// recuo), e não à posição no mundo: assim o avanço ao longo da pista não fica
// atrasado proporcionalmente à velocidade (seriam ~5 m a 30 m/s) e o resultado
// é exatamente independente do dt para alvos constantes. Trocas de faixa e a
// entrada/saída do túnel são suaves; a câmera segue só 1/3 do deslocamento
// lateral do jogador (não "pula" de faixa). A altura do pulo não é seguida.
class ThirdPersonCamera {
public:
    ThirdPersonCamera();

    // Coloca a câmera direto no alvo, sem suavização (início/reinício da corrida).
    void Snap(const TrackFrame& player_frame, float player_x, bool in_tunnel);

    // Aproxima a câmera do alvo de forma suave, proporcional a dt.
    void Update(const TrackFrame& player_frame, float player_x, bool in_tunnel, float dt);

    CameraPose Pose() const;

private:
    void ComputePose();

    TrackFrame frame_;  // referencial do jogador no último Snap/Update
    float lateral_;     // deslocamento lateral atual (suavizado)
    float height_;      // altura atual (suavizada)
    float back_;        // recuo atual (suavizado)
    CameraPose pose_;
};

#endif // _CAMERA_H
