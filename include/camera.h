#ifndef _CAMERA_H
#define _CAMERA_H

#include <glm/vec3.hpp>
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
// lateral do jogador (não "pula" de faixa). A altura do pulo não é seguida,
// mas a altura do CHÃO sob o jogador (ground_y: rampa ou teto de trem,
// Tarefa 4.4) é, também suavizada: em cima do trem a câmera sobe junto e
// olha para o jogador lá em cima, em vez de para a lateral do trem.
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
    // ground_y: altura do chão sob o jogador (Game::GroundHeight()).
    void Snap(const Track& track, float player_s, float player_x, bool in_tunnel,
              float ground_y = 0.0f);

    // Aproxima a câmera do alvo de forma suave, proporcional a dt.
    void Update(const Track& track, float player_s, float player_x, bool in_tunnel, float dt,
                float ground_y = 0.0f);

    CameraPose Pose() const;

    // Recuo e altura atuais (suavizados). main.cpp usa Back() para saber se a
    // própria câmera (em s - Back()) já está dentro do túnel; o E2E confere
    // que a altura diminui dentro dele.
    float Back() const { return back_; }
    float Height() const { return height_; }

private:
    void ComputePose(const Track& track, float player_s);

    float lateral_;     // deslocamento lateral atual (suavizado)
    float height_;      // altura atual (suavizada)
    float back_;        // recuo atual (suavizado)
    float ground_;      // altura do chão sob o jogador (suavizada)
    CameraPose pose_;
};

// Parâmetros da câmera livre (Tarefa 5.1).
const float FREE_CAMERA_SPEED       = 10.0f;        // m/s
const float FREE_CAMERA_SENSITIVITY = 0.003f;       // rad por pixel do mouse
const float FREE_CAMERA_MAX_PITCH   = 1.55334303f;  // 89 graus, em rad

// Câmera livre ("fly"), ativada com C com o jogo pausado: o jogador percorre
// o cenário olhando para onde quiser. Guarda posição, yaw e pitch; o view
// vector sai de yaw/pitch com a mesma convenção de heading da pista
// (forward = (-sin yaw, 0, -cos yaw)), de modo que yaw = 0 olha para -Z do
// mundo. O "up" é sempre o Y do mundo (sem rolagem), então o pitch é limitado
// a +-89 graus para o view vector nunca ficar paralelo a ele (o que tornaria
// a base da câmera, em Matrix_Camera_View, degenerada).
class FreeCamera {
public:
    FreeCamera();

    // Começa onde estava a câmera em 3ª pessoa: mesma posição e mesma direção
    // do olhar (yaw/pitch derivados do view vector da pose).
    void StartAt(const CameraPose& from);

    // Gira o olhar. dx > 0 (mouse para a direita) olha para a direita;
    // dy > 0 (mouse para baixo) olha para baixo. Em pixels, sem dt (o delta
    // do mouse já é o deslocamento do quadro).
    void Look(float dx_pixels, float dy_pixels);

    // Move local_dir = (direita, cima, frente) no referencial da câmera, a
    // FREE_CAMERA_SPEED m/s. "Frente" inclui o pitch (a câmera voa para onde
    // olha) e "cima" é o Y do mundo. A direção é normalizada (a diagonal não
    // é mais rápida); vetor nulo ou dt <= 0 não move.
    void Move(glm::vec3 local_dir, float dt);

    CameraPose Pose() const;

private:
    glm::vec4 position_;
    float yaw_;    // rad, em torno de Y
    float pitch_;  // rad, positivo = olha para cima
};

#endif // _CAMERA_H
