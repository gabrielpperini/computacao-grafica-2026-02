#include "camera.h"

#include <cmath>

namespace {

const glm::vec4 kUp(0.0f, 1.0f, 0.0f, 0.0f);

// Alvo (lateral, altura, recuo) da câmera para o estado atual do jogador.
void Target(float player_x, bool in_tunnel, float* lateral, float* height, float* back)
{
    *lateral = player_x * CAMERA_LATERAL_FACTOR;
    *height = in_tunnel ? CAMERA_TUNNEL_HEIGHT : CAMERA_HEIGHT;
    *back = in_tunnel ? CAMERA_TUNNEL_BACK : CAMERA_BACK;
}

} // namespace

ThirdPersonCamera::ThirdPersonCamera()
    : lateral_(0.0f), height_(CAMERA_HEIGHT), back_(CAMERA_BACK), ground_(0.0f)
{
    // Pose de uma pista reta a partir da origem (até o primeiro Snap/Update).
    pose_.position = glm::vec4(0.0f, CAMERA_HEIGHT, CAMERA_BACK, 1.0f);
    pose_.view_vector = glm::vec4(0.0f, CAMERA_LOOK_HEIGHT - CAMERA_HEIGHT,
                                  -CAMERA_LOOK_AHEAD - CAMERA_BACK, 0.0f);
    pose_.up = kUp;
}

void ThirdPersonCamera::Snap(const Track& track, float player_s, float player_x, bool in_tunnel,
                             float ground_y)
{
    Target(player_x, in_tunnel, &lateral_, &height_, &back_);
    ground_ = ground_y;
    ComputePose(track, player_s);
}

void ThirdPersonCamera::Update(const Track& track, float player_s, float player_x, bool in_tunnel,
                               float dt, float ground_y)
{
    if (dt < 0.0f)
        dt = 0.0f;

    float lateral, height, back;
    Target(player_x, in_tunnel, &lateral, &height, &back);

    // Suavização exponencial: fração (1 - e^(-k dt)) da distância até o alvo.
    // Para alvo constante, n passos de dt equivalem a um passo de n*dt.
    const float a = 1.0f - std::exp(-CAMERA_SMOOTHING_RATE * dt);
    lateral_ += (lateral - lateral_) * a;
    height_ += (height - height_) * a;
    back_ += (back - back_) * a;
    ground_ += (ground_y - ground_) * a;

    ComputePose(track, player_s);
}

CameraPose ThirdPersonCamera::Pose() const
{
    return pose_;
}

void ThirdPersonCamera::ComputePose(const Track& track, float player_s)
{
    // Câmera sobre a pista, back_ atrás do jogador, e alvo sobre a pista,
    // CAMERA_LOOK_AHEAD à frente; os dois com o mesmo deslocamento lateral
    // (1/3 do do jogador), cada um no referencial do seu próprio ponto, e
    // ambos erguidos pela altura do chão sob o jogador (rampa/teto de trem).
    const TrackFrame behind = track.FrameAt(player_s - back_);
    const TrackFrame ahead = track.FrameAt(player_s + CAMERA_LOOK_AHEAD);

    const glm::vec4 position = behind.position + behind.right * lateral_ + kUp * (height_ + ground_);
    const glm::vec4 look_at = ahead.position + ahead.right * lateral_ + kUp * (CAMERA_LOOK_HEIGHT + ground_);

    pose_.position = position;
    pose_.view_vector = look_at - position;
    pose_.up = kUp;
}
