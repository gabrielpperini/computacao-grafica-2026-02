#include "camera.h"

#include <algorithm>
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

// ---------------------------------------------------------------------------
// Câmera livre
// ---------------------------------------------------------------------------

namespace {

// Direção do olhar para um yaw/pitch (convenção da pista: yaw = 0 -> -Z).
glm::vec4 LookDirection(float yaw, float pitch)
{
    const float c = std::cos(pitch);
    return glm::vec4(-std::sin(yaw) * c, std::sin(pitch), -std::cos(yaw) * c, 0.0f);
}

} // namespace

FreeCamera::FreeCamera()
    : position_(0.0f, CAMERA_HEIGHT, CAMERA_BACK, 1.0f), yaw_(0.0f), pitch_(0.0f)
{
}

void FreeCamera::StartAt(const CameraPose& from)
{
    position_ = glm::vec4(from.position.x, from.position.y, from.position.z, 1.0f);

    const glm::vec3 v(from.view_vector.x, from.view_vector.y, from.view_vector.z);
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length < 1e-6f) {
        yaw_ = 0.0f;
        pitch_ = 0.0f;
        return;
    }
    float sin_pitch = v.y / length;
    sin_pitch = std::fmax(-1.0f, std::fmin(1.0f, sin_pitch));
    pitch_ = std::asin(sin_pitch);
    pitch_ = std::fmax(-FREE_CAMERA_MAX_PITCH, std::fmin(FREE_CAMERA_MAX_PITCH, pitch_));
    // Olhando (quase) reto para cima/baixo o yaw é indefinido: mantém o anterior.
    if (v.x * v.x + v.z * v.z > 1e-12f)
        yaw_ = std::atan2(-v.x, -v.z);  // inverso de (-sin yaw, -cos yaw)
}

void FreeCamera::Look(float dx_pixels, float dy_pixels)
{
    yaw_ -= dx_pixels * FREE_CAMERA_SENSITIVITY;  // mouse à direita: yaw diminui (right = (cos, 0, -sin))
    pitch_ -= dy_pixels * FREE_CAMERA_SENSITIVITY;
    pitch_ = std::fmax(-FREE_CAMERA_MAX_PITCH, std::fmin(FREE_CAMERA_MAX_PITCH, pitch_));
    // Mantém o yaw em (-pi, pi] para não perder precisão em sessões longas.
    const float two_pi = 6.28318531f;
    yaw_ = std::fmod(yaw_, two_pi);
    if (yaw_ > 3.14159265f) yaw_ -= two_pi;
    if (yaw_ < -3.14159265f) yaw_ += two_pi;
}

void FreeCamera::Move(glm::vec3 local_dir, float dt)
{
    const float length = std::sqrt(local_dir.x * local_dir.x + local_dir.y * local_dir.y +
                                   local_dir.z * local_dir.z);
    if (length < 1e-6f || dt <= 0.0f)
        return;
    local_dir /= length;

    const glm::vec4 forward = LookDirection(yaw_, pitch_);
    const glm::vec4 right(std::cos(yaw_), 0.0f, -std::sin(yaw_), 0.0f);
    const glm::vec4 delta = right * local_dir.x + kUp * local_dir.y + forward * local_dir.z;
    position_ += delta * (FREE_CAMERA_SPEED * dt);
}

void FreeCamera::Move(glm::vec3 local_dir, float dt, const FreeCameraBlockedFn& blocked)
{
    if (!blocked) {
        Move(local_dir, dt);
        return;
    }
    const float length = std::sqrt(local_dir.x * local_dir.x + local_dir.y * local_dir.y +
                                   local_dir.z * local_dir.z);
    if (length < 1e-6f || dt <= 0.0f)
        return;
    local_dir /= length;

    const glm::vec4 forward = LookDirection(yaw_, pitch_);
    const glm::vec4 right(std::cos(yaw_), 0.0f, -std::sin(yaw_), 0.0f);
    const glm::vec4 delta4 = (right * local_dir.x + kUp * local_dir.y + forward * local_dir.z) *
                             (FREE_CAMERA_SPEED * dt);
    const glm::vec3 delta(delta4.x, delta4.y, delta4.z);
    const float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    const int steps = std::max(1, (int)std::ceil(dist / FREE_CAMERA_COLLISION_STEP));
    const glm::vec3 step = delta / (float)steps;

    glm::vec3 p(position_.x, position_.y, position_.z);
    for (int i = 0; i < steps; ++i) {
        // Eixos horizontais primeiro: subir/descer encostado numa parede
        // continua deslizando.
        const int order[3] = {0, 2, 1};
        for (int a = 0; a < 3; ++a) {
            const int axis = order[a];
            if (step[axis] == 0.0f)
                continue;
            glm::vec3 q = p;
            q[axis] += step[axis];
            if (!blocked(q) || blocked(p))
                p = q;
        }
    }
    position_ = glm::vec4(p.x, p.y, p.z, 1.0f);
}

CameraPose FreeCamera::Pose() const
{
    CameraPose pose;
    pose.position = position_;
    pose.view_vector = LookDirection(yaw_, pitch_);
    pose.up = kUp;
    return pose;
}
