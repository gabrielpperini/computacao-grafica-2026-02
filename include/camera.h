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
const float CAMERA_BACK            = 6.0f;  // recuo atrás do jogador (ao longo da pista), fora do túnel
const float CAMERA_HEIGHT          = 3.5f;  // altura acima do chão, fora do túnel
const float CAMERA_TUNNEL_BACK     = 4.0f;  // dentro do túnel: mais perto ...
const float CAMERA_TUNNEL_HEIGHT   = 2.2f;  // ... e mais baixa, para não atravessar o teto
const float CAMERA_LOOK_AHEAD      = 4.0f;  // olha para o ponto da pista 4 m à frente do jogador ...
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
//
// Curvas (Tarefa 4.1): a câmera fica SOBRE a pista, no ponto s - recuo, e olha
// para o ponto da pista em s + CAMERA_LOOK_AHEAD (como um vagão que segue o
// trilho). Numa reta isso é idêntico a "atrás do jogador, olhando para a
// frente"; na curva o view vector é a corda entre esses dois pontos, que gira
// de forma contínua: começa a virar antes do jogador entrar na curva e não dá
// tranco nas junções reta/curva (onde a curvatura muda de uma vez). O heading
// não é suavizado à parte, então nenhuma suavização "briga" com a curva.
class ThirdPersonCamera {
public:
    ThirdPersonCamera();

    // Coloca a câmera direto no alvo, sem suavização (início/reinício da corrida).
    void Snap(const Track& track, float player_s, float player_x, bool in_tunnel);

    // Aproxima a câmera do alvo de forma suave, proporcional a dt.
    void Update(const Track& track, float player_s, float player_x, bool in_tunnel, float dt);

    CameraPose Pose() const;

private:
    void ComputePose(const Track& track, float player_s);

    float lateral_;     // deslocamento lateral atual (suavizado)
    float height_;      // altura atual (suavizada)
    float back_;        // recuo atual (suavizado)
    CameraPose pose_;
};

#endif // _CAMERA_H
