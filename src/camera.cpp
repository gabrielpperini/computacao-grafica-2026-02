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
    : lateral_(0.0f), height_(CAMERA_HEIGHT), back_(CAMERA_BACK)
{
    frame_.position = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    frame_.forward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    frame_.right = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    frame_.heading = 0.0f;
    ComputePose();
}

void ThirdPersonCamera::Snap(const TrackFrame& player_frame, float player_x, bool in_tunnel)
{
    frame_ = player_frame;
    Target(player_x, in_tunnel, &lateral_, &height_, &back_);
    ComputePose();
}

void ThirdPersonCamera::Update(const TrackFrame& player_frame, float player_x, bool in_tunnel,
                               float dt)
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

    frame_ = player_frame;
    ComputePose();
}

CameraPose ThirdPersonCamera::Pose() const
{
    return pose_;
}

void ThirdPersonCamera::ComputePose()
{
    // Ponto do chão da pista atrás do qual a câmera fica (com 1/3 do lateral).
    const glm::vec4 base = frame_.position + frame_.right * lateral_;

    const glm::vec4 position = base - frame_.forward * back_ + kUp * height_;
    const glm::vec4 look_at = base + frame_.forward * CAMERA_LOOK_AHEAD + kUp * CAMERA_LOOK_HEIGHT;

    pose_.position = position;
    pose_.view_vector = look_at - position;
    pose_.up = kUp;
}
