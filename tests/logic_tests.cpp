// Testes unitários dos módulos de lógica (sem OpenGL/GLFW). Cada módulo novo
// adiciona suas funções Test_*() e as chama em main().
// Os asserts precisam estar sempre ativos, mesmo em builds com NDEBUG.
#undef NDEBUG
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/vec4.hpp>

#include "camera.h"
#include "collisions.h"
#include "game.h"
#include "game_config.h"
#include "scenery.h"
#include "track.h"

using glm::vec4;

static const float kEps = 1e-4f;

static bool near(const vec4& a, const vec4& b, float eps = kEps)
{
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps &&
           std::fabs(a.z - b.z) < eps && std::fabs(a.w - b.w) < eps;
}

static bool nearf(float a, float b, float eps = kEps)
{
    return std::fabs(a - b) < eps;
}

// ---------------------------------------------------------------- Track (1.1)

void Test_TrackStraightFrame()
{
    Track t(1);
    t.EnsureGeneratedUntil(200);
    TrackFrame f = t.FrameAt(10.0f);
    assert(near(f.position, vec4(0, 0, -10, 1)));
    assert(near(f.forward, vec4(0, 0, -1, 0)));
    assert(near(f.right, vec4(1, 0, 0, 0)));
    assert(nearf(f.heading, 0.0f));
}

void Test_TrackDiscardKeepsFrames()
{
    Track t(1);
    t.EnsureGeneratedUntil(500);
    t.DiscardBefore(300);
    assert(near(t.FrameAt(400).position, vec4(0, 0, -400, 1)));
    assert(t.SegmentAt(400) != nullptr);
}

void Test_StripVertexCount() // 0..10 com passo 1 -> 11 pares
{
    Track t(1);
    t.EnsureGeneratedUntil(50);
    std::vector<float> p, n, uv;
    t.BuildStrip(0, 10, 1, -4, 4, 0, 3, 4, &p, &n, &uv);
    assert(p.size() == 11 * 2 * 4 && uv.size() == 11 * 2 * 2);
    assert(n.size() == 11 * 2 * 4);
    // primeiro par: x = -4 e x = +4, z = 0; UV: u = 0 e 3, v = 0
    assert(nearf(p[0], -4) && nearf(p[4], 4) && nearf(p[2], 0) && nearf(p[3], 1));
    assert(nearf(uv[0], 0) && nearf(uv[2], 3) && nearf(uv[1], 0));
    // ultimo par: s = 10 -> z = -10, v = 10/4
    size_t last = 10 * 2;
    assert(nearf(p[last * 4 + 2], -10) && nearf(uv[last * 2 + 1], 2.5f));
    // normal aponta para cima
    assert(nearf(n[1], 1) && nearf(n[3], 0));
}

void Test_TrackFrameOutOfRange()
{
    Track t(1);
    t.EnsureGeneratedUntil(100);
    // alem do que foi gerado: extrapola (nao quebra)
    TrackFrame f = t.FrameAt(1e6f);
    assert(std::isfinite(f.position.z));
    t.DiscardBefore(70);
    TrackFrame g = t.FrameAt(-5.0f); // antes do primeiro segmento mantido
    assert(std::isfinite(g.position.z));
    assert(t.SegmentAt(-5.0f) == nullptr || t.SegmentAt(-5.0f)->s_start <= -5.0f);
}

void Test_TrackDeterministicSeed()
{
    Track a(7), b(7);
    a.EnsureGeneratedUntil(300);
    b.EnsureGeneratedUntil(300);
    assert(near(a.FrameAt(250).position, b.FrameAt(250).position));
}

// Guardas da formula fechada da curva (usada de verdade na Fase 4).
// O construtor ja gera o 1o segmento reto (0..60 m); AppendSegment anexa depois.
static const float kHalfPi = 1.57079632679f;

void Test_TrackCurveContinuity()
{
    const float R = 40.0f, k = 1.0f / R, L = kHalfPi * R;
    Track c(1);
    c.EnsureGeneratedUntil(10);
    const TrackSegment* first = c.SegmentAt(0);
    assert(first != nullptr);
    float j1 = first->s_start + first->length; // reta -> curva
    c.AppendSegment(L, k, false);
    c.AppendSegment(30.0f, 0.0f, false); // curva -> reta
    float j2 = j1 + L;
    float d = 1e-3f;
    TrackFrame a1 = c.FrameAt(j1 - d), b1 = c.FrameAt(j1 + d);
    assert(near(a1.position, b1.position, 5e-3f));
    assert(nearf(a1.heading, b1.heading, 1e-3f));
    TrackFrame a2 = c.FrameAt(j2 - d), b2 = c.FrameAt(j2 + d);
    assert(near(a2.position, b2.position, 5e-3f));
    assert(nearf(a2.heading, b2.heading, 1e-3f));
    // depois da curva a reta segue na direcao final da curva (-X)
    assert(near(c.FrameAt(j2 + 10.0f).forward, vec4(-1, 0, 0, 0), 1e-3f));
    assert(nearf(c.FrameAt(j2 + 10.0f).position.x, c.FrameAt(j2).position.x - 10.0f, 1e-3f));
}

void Test_TrackLeftCurveEndsHeadingLeft()
{
    const float R = 40.0f, k = 1.0f / R, L = kHalfPi * R;
    Track d(1);
    d.EnsureGeneratedUntil(10);
    const TrackSegment* first = d.SegmentAt(0);
    float s0 = first->s_start + first->length;
    d.AppendSegment(L, k, false);

    // curva a esquerda: ao fim, forward = (-1,0,0) e right = (0,0,-1)
    TrackFrame e = d.FrameAt(s0 + L);
    assert(near(e.forward, vec4(-1, 0, 0, 0), 1e-3f));
    assert(near(e.right, vec4(0, 0, -1, 0), 1e-3f));
    assert(nearf(e.heading, kHalfPi, 1e-3f));

    // centro do circulo fica a esquerda (-right) do inicio da curva; o ponto
    // medio (e qualquer outro) esta a distancia R dele
    TrackFrame st = d.FrameAt(s0);
    vec4 center = st.position - st.right * R;
    TrackFrame mid = d.FrameAt(s0 + L * 0.5f);
    vec4 dv = mid.position - center;
    assert(nearf(std::sqrt(dv.x * dv.x + dv.z * dv.z), R, 1e-3f));
    vec4 dv2 = e.position - center;
    assert(nearf(std::sqrt(dv2.x * dv2.x + dv2.z * dv2.z), R, 1e-3f));

    // curva a direita (curvatura negativa): simetrica, termina com forward (+1,0,0)
    Track r(1);
    r.EnsureGeneratedUntil(10);
    r.AppendSegment(L, -k, false);
    TrackFrame er = r.FrameAt(s0 + L);
    assert(near(er.forward, vec4(1, 0, 0, 0), 1e-3f));
    assert(nearf(er.heading, -kHalfPi, 1e-3f));
}

// ---------------------------------------------------------------- Game (1.2)

void Test_LaneClamp()
{
    Game g(1);
    g.HandleInput(Input::LEFT);
    g.HandleInput(Input::LEFT);
    assert(g.player.lane == -1);
    g.HandleInput(Input::RIGHT);
    g.HandleInput(Input::RIGHT);
    g.HandleInput(Input::RIGHT);
    assert(g.player.lane == 1);
}

void Test_LaneMoveTime() // 1 faixa em 1 s (LANE_WIDTH / LANE_SWITCH_SPEED < 1 s)
{
    Game g(1);
    g.HandleInput(Input::RIGHT);
    for (int i = 0; i < 60; i++) g.Update(1 / 60.f);
    assert(std::fabs(g.player.x - LANE_WIDTH) < 1e-4);
}

void Test_LaneNoOvershoot() // x nunca passa do alvo, nem com dt grande
{
    Game g(1);
    g.HandleInput(Input::LEFT);
    for (int i = 0; i < 30; i++) {
        g.Update(MAX_DT);
        assert(g.player.x >= -LANE_WIDTH);
    }
    assert(g.player.x == -LANE_WIDTH);
}

void Test_JumpLands()
{
    Game g(1);
    g.HandleInput(Input::JUMP);
    g.Update(0.01f);
    assert(g.player.y > 0);
    for (int i = 0; i < 200; i++) g.Update(0.01f);
    assert(g.player.y == 0 && g.player.grounded);
    assert(g.player.vy == 0);
}

void Test_NoDoubleJump()
{
    Game g(1);
    g.HandleInput(Input::JUMP);
    g.Update(0.1f);
    float vy = g.player.vy;
    g.HandleInput(Input::JUMP);
    assert(g.player.vy == vy);
}

void Test_SpeedCap()
{
    Game g(1);
    for (int i = 0; i < 100000; i++) g.Update(MAX_DT);
    assert(g.player.speed == PLAYER_MAX_SPEED);
}

void Test_SpeedRamp() // 5 s de jogo -> +5 * PLAYER_ACCEL
{
    Game g(1);
    for (int i = 0; i < 100; i++) g.Update(MAX_DT);
    assert(nearf(g.player.speed, PLAYER_START_SPEED + 5 * PLAYER_ACCEL, 1e-3f));
}

void Test_DtIndependent()
{
    Game a(1), b(1);
    for (int i = 0; i < 60; i++) a.Update(1 / 60.f);
    for (int i = 0; i < 240; i++) b.Update(1 / 240.f);
    assert(std::fabs(a.player.s - b.player.s) < 0.01f);
}

void Test_DtClamped() // dt gigante vale no máximo MAX_DT
{
    Game a(1), b(1);
    a.Update(5.0f);
    b.Update(MAX_DT);
    assert(a.player.s == b.player.s);
    a.Update(-1.0f); // dt negativo não anda para trás
    assert(a.player.s == b.player.s);
}

void Test_RollTimerExpires()
{
    Game g(1);
    g.HandleInput(Input::ROLL);
    assert(g.player.roll_timer == ROLL_DURATION);
    g.Update(ROLL_DURATION / 20);
    assert(g.player.roll_timer > 0);
    for (int i = 0; i < 20; i++) g.Update(0.05f);
    assert(g.player.roll_timer == 0);
}

void Test_JumpCancelsRoll()
{
    Game g(1);
    g.HandleInput(Input::ROLL);
    g.HandleInput(Input::JUMP);
    assert(g.player.roll_timer == 0);
    assert(g.player.vy > 0 && !g.player.grounded);
}

void Test_RollInAirDescendsFaster()
{
    Game g(1);
    g.HandleInput(Input::JUMP);
    g.Update(0.05f);
    assert(g.player.vy > 0);
    g.HandleInput(Input::ROLL);
    assert(g.player.vy == -JUMP_VELOCITY);
    assert(g.player.roll_timer == ROLL_DURATION);
    for (int i = 0; i < 100; i++) g.Update(0.01f);
    assert(g.player.grounded && g.player.y == 0);
}

void Test_InputIgnoredWhenNotPlaying()
{
    Game g(1);
    g.mode = GameMode::PAUSED;
    g.HandleInput(Input::LEFT);
    g.HandleInput(Input::JUMP);
    g.Update(0.05f);
    assert(g.player.lane == 0 && g.player.grounded);
    assert(g.player.s == 0 && g.elapsed == 0);
}

void Test_GameResetRestoresState()
{
    Game g(7), fresh(7);
    g.HandleInput(Input::RIGHT);
    g.HandleInput(Input::JUMP);
    for (int i = 0; i < 100; i++) g.Update(0.02f);
    g.mode = GameMode::GAME_OVER;
    g.Reset();
    assert(g.mode == GameMode::PLAYING);
    assert(g.elapsed == 0);
    assert(g.player.lane == 0 && g.player.x == 0 && g.player.y == 0);
    assert(g.player.s == 0 && g.player.speed == PLAYER_START_SPEED);
    assert(g.player.grounded && g.player.vy == 0 && g.player.roll_timer == 0);
    // pista regenerada com a mesma semente: mesmos frames que um jogo novo
    g.Update(0.05f);
    fresh.Update(0.05f);
    assert(near(g.track.FrameAt(100).position, fresh.track.FrameAt(100).position));
}

void Test_GameKeepsTrackAhead()
{
    Game g(1);
    for (int i = 0; i < 200; i++) g.Update(MAX_DT);
    assert(g.track.SegmentAt(g.player.s + SPAWN_AHEAD - 1.0f) != nullptr);
    assert(g.track.SegmentAt(g.player.s) != nullptr);
}

// ---------------------------------------------------------------- Câmera (1.3)

// Referencial de uma pista reta em s (heading 0: forward = -Z, right = +X).
static TrackFrame StraightFrame(float s)
{
    Track t(1);
    t.EnsureGeneratedUntil(s + 10.0f);
    return t.FrameAt(s);
}

void Test_CameraSnapEqualsTarget()
{
    ThirdPersonCamera cam;
    TrackFrame f = StraightFrame(10.0f); // jogador em (0, 0, -10)
    cam.Snap(f, LANE_WIDTH, false);
    CameraPose p = cam.Pose();
    // Só 1/3 do deslocamento lateral; 6 m atrás (+Z) e 3.5 m acima.
    float lat = LANE_WIDTH / 3.0f;
    assert(near(p.position, vec4(lat, CAMERA_HEIGHT, -10.0f + CAMERA_BACK, 1)));
    // Olha para jogador + forward*4 + up*1 (com o mesmo lateral).
    vec4 look(lat, CAMERA_LOOK_HEIGHT, -10.0f - CAMERA_LOOK_AHEAD, 1);
    assert(near(p.view_vector, look - p.position));
    assert(near(p.up, vec4(0, 1, 0, 0)));
}

void Test_CameraConverges() // troca de faixa: move aos poucos, sem passar do alvo
{
    ThirdPersonCamera cam;
    TrackFrame f = StraightFrame(0.0f);
    cam.Snap(f, 0.0f, false);
    float target = LANE_WIDTH / 3.0f;
    cam.Update(f, LANE_WIDTH, false, 1.0f / 60.0f);
    float x1 = cam.Pose().position.x;
    assert(x1 > 0.0f && x1 < 0.5f * target); // suave: não pula para o alvo
    float prev = x1;
    for (int i = 0; i < 120; i++) { // 2 s
        cam.Update(f, LANE_WIDTH, false, 1.0f / 60.0f);
        float x = cam.Pose().position.x;
        assert(x >= prev && x <= target + kEps); // monotônico, sem overshoot
        prev = x;
    }
    assert(nearf(prev, target, 1e-3f));
}

void Test_CameraDtIndependent() // 30 Hz e 120 Hz chegam ao mesmo lugar
{
    Track t(1);
    t.EnsureGeneratedUntil(100.0f);
    ThirdPersonCamera a, b;
    a.Snap(t.FrameAt(0.0f), 0.0f, false);
    b.Snap(t.FrameAt(0.0f), 0.0f, false);
    // 0.5 s: jogador anda a 12 m/s, troca para a faixa da direita e entra
    // num túnel (alvo de altura/recuo muda no meio do caminho).
    for (int i = 1; i <= 15; i++) {
        float s = 12.0f * i / 30.0f;
        a.Update(t.FrameAt(s), LANE_WIDTH, true, 1.0f / 30.0f);
    }
    for (int i = 1; i <= 60; i++) {
        float s = 12.0f * i / 120.0f;
        b.Update(t.FrameAt(s), LANE_WIDTH, true, 1.0f / 120.0f);
    }
    assert(near(a.Pose().position, b.Pose().position, 1e-3f));
    assert(near(a.Pose().view_vector, b.Pose().view_vector, 1e-3f));
    // E a câmera acompanha o avanço do jogador sem atraso (z ~ -6 + recuo).
    assert(a.Pose().position.z < -6.0f + CAMERA_BACK);
}

void Test_CameraTunnelLowerCloser()
{
    TrackFrame f = StraightFrame(20.0f);
    ThirdPersonCamera out, in;
    out.Snap(f, 0.0f, false);
    in.Snap(f, 0.0f, true);
    vec4 po = out.Pose().position, pi = in.Pose().position;
    assert(pi.y < po.y);                                     // mais baixa
    assert(std::fabs(pi.z - f.position.z) < std::fabs(po.z - f.position.z)); // mais perto
    assert(nearf(pi.y, CAMERA_TUNNEL_HEIGHT));
    assert(nearf(pi.z, f.position.z + CAMERA_TUNNEL_BACK));
    // A transição é suave: um passo de 1/60 s não chega ao alvo do túnel.
    out.Update(f, 0.0f, true, 1.0f / 60.0f);
    float y = out.Pose().position.y;
    assert(y < po.y && y > CAMERA_TUNNEL_HEIGHT + 0.5f);
}

// ---------------------------------------------------------------- Cenário (1.3)

void Test_SceneryFitsCapacity() // nunca passa da capacidade dos buffers da GPU
{
    Game g(1);
    StripBatch ground, rails;
    const float ss[] = {0.0f, 7.3f, 1234.567f, 98765.4f};
    for (float s : ss) {
        g.track.EnsureGeneratedUntil(s + SPAWN_AHEAD);
        BuildGroundStrips(g.track, s, &ground);
        BuildRailStrips(g.track, s, &rails);
        assert(ground.first.size() == (size_t)GROUND_STRIP_COUNT);
        assert(rails.first.size() == (size_t)RAIL_STRIP_COUNT);
        for (size_t i = 0; i < ground.count.size(); i++)
            assert((size_t)ground.count[i] <= MaxVerticesPerStrip());
        for (size_t i = 0; i < rails.count.size(); i++)
            assert((size_t)rails.count[i] <= MaxVerticesPerStrip());
        assert(ground.VertexCount() <= GROUND_STRIP_COUNT * MaxVerticesPerStrip());
        assert(rails.VertexCount() <= RAIL_STRIP_COUNT * MaxVerticesPerStrip());
        assert(ground.uvs.size() == ground.VertexCount() * 2);
        assert(rails.normals.size() == rails.VertexCount() * 4);
    }
}

void Test_RailsAboveGroundAndInsideBed()
{
    Track t(1);
    t.EnsureGeneratedUntil(200);
    StripBatch rails;
    BuildRailStrips(t, 50.0f, &rails);
    for (size_t v = 0; v < rails.VertexCount(); v++) {
        assert(nearf(rails.positions[4 * v + 1], RAIL_HEIGHT));
        assert(std::fabs(rails.positions[4 * v + 0]) < TRACK_BED_HALF_WIDTH);
    }
}

void Test_BuildingsCoverVisibleRange()
{
    std::vector<BuildingInstance> a, b;
    ComputeBuildings(100.0f, &a);
    assert(!a.empty());
    for (size_t i = 0; i < a.size(); i++) {
        assert(a[i].s >= 100.0f - DESPAWN_BEHIND && a[i].s <= 100.0f + SPAWN_AHEAD);
        assert(nearf(std::fabs(a[i].x), BUILDING_OFFSET_X));
        assert(a[i].height >= BUILDING_MIN_HEIGHT && a[i].height <= BUILDING_MIN_HEIGHT + 8.0f);
    }
    // Mesmo prédio (mesmo s e lado) mantém a altura quando o jogador anda.
    ComputeBuildings(130.0f, &b);
    int matched = 0;
    for (size_t i = 0; i < a.size(); i++)
        for (size_t j = 0; j < b.size(); j++)
            if (a[i].s == b[j].s && a[i].x == b[j].x) {
                assert(a[i].height == b[j].height);
                matched++;
            }
    assert(matched > 10);
}

// ------------------------------------------------------- Colisões (2.1)

static AABB Box(float x0, float y0, float z0, float x1, float y1, float z1)
{
    AABB b;
    b.min = glm::vec3(x0, y0, z0);
    b.max = glm::vec3(x1, y1, z1);
    return b;
}

void Test_MakeAABB()
{
    AABB b = MakeAABB(glm::vec3(1, 2, 3), glm::vec3(0.5f, 1.0f, 2.0f));
    assert(nearf(b.min.x, 0.5f) && nearf(b.min.y, 1.0f) && nearf(b.min.z, 1.0f));
    assert(nearf(b.max.x, 1.5f) && nearf(b.max.y, 3.0f) && nearf(b.max.z, 5.0f));
    // Semi-extensões negativas são tomadas em módulo (min <= max sempre).
    AABB n = MakeAABB(glm::vec3(0, 0, 0), glm::vec3(-1, -1, -1));
    assert(n.min.x <= n.max.x && n.min.y <= n.max.y && n.min.z <= n.max.z);
}

void Test_AABBTouchingIsHit() // faces encostadas contam como colisão
{
    AABB a = Box(0, 0, 0, 1, 1, 1);
    AABB b = Box(1, 0, 0, 2, 1, 1);
    assert(AABBIntersectsAABB(a, b));
    assert(AABBIntersectsAABB(b, a));
}

void Test_AABBSeparatedInS()
{
    AABB a = Box(0, 0, 0, 1, 1, 1);
    AABB b = Box(0, 0, 1.01f, 1, 1, 2);
    assert(!AABBIntersectsAABB(a, b));
    assert(!AABBIntersectsAABB(b, a));
}

void Test_AABBOverlap()
{
    AABB a = Box(0, 0, 0, 2, 2, 2);
    assert(AABBIntersectsAABB(a, Box(1, 1, 1, 3, 3, 3)));
    assert(AABBIntersectsAABB(a, Box(0.5f, 0.5f, 0.5f, 1, 1, 1))); // contido
    assert(!AABBIntersectsAABB(a, Box(0, 2.5f, 0, 2, 3, 2)));      // acima
    assert(!AABBIntersectsAABB(a, Box(2.5f, 0, 0, 3, 2, 2)));      // ao lado
}

void Test_SphereCornerMiss() // perto do canto, mas fora
{
    AABB box = Box(0, 0, 0, 1, 1, 1);
    // Distância ao canto (1,1,1) = sqrt(3)*0.5 ~ 0.866 > r = 0.8.
    assert(!SphereIntersectsAABB(glm::vec3(1.5f, 1.5f, 1.5f), 0.8f, box));
    assert(SphereIntersectsAABB(glm::vec3(1.5f, 1.5f, 1.5f), 0.9f, box));
    // Encostando exatamente na face: conta como colisão (<=).
    assert(SphereIntersectsAABB(glm::vec3(1.5f, 0.5f, 0.5f), 0.5f, box));
}

void Test_SphereInside()
{
    AABB box = Box(0, 0, 0, 4, 4, 4);
    assert(SphereIntersectsAABB(glm::vec3(2, 2, 2), 0.5f, box));
    assert(SphereIntersectsAABB(glm::vec3(2, 2, 2), 100.0f, box)); // caixa dentro da esfera
}

void Test_SphereIntersectsSphere()
{
    assert(SphereIntersectsSphere(glm::vec3(0, 0, 0), 1.0f, glm::vec3(1.5f, 0, 0), 1.0f));
    assert(SphereIntersectsSphere(glm::vec3(0, 0, 0), 1.0f, glm::vec3(2, 0, 0), 1.0f)); // tangentes
    assert(!SphereIntersectsSphere(glm::vec3(0, 0, 0), 1.0f, glm::vec3(2.1f, 0, 0), 1.0f));
    assert(!SphereIntersectsSphere(glm::vec3(0, 0, 0), 0.5f, glm::vec3(0, 3, 4), 1.0f));
}

void Test_RampMidHeight() // sobe de 0 a 2 em s = 0..4
{
    AABB ramp = Box(-1, 0, 0, 1, 2, 4);
    float h = -1.0f;
    assert(RampHeightAt(ramp, 0.0f, 2.0f, &h));
    assert(nearf(h, 1.0f));
}

void Test_RampEnds()
{
    AABB ramp = Box(-1, 0, 0, 1, 2, 4);
    float h = -1.0f;
    assert(RampHeightAt(ramp, 0.0f, 0.0f, &h));
    assert(nearf(h, 0.0f)); // início: min.y
    assert(RampHeightAt(ramp, 0.0f, 4.0f, &h));
    assert(nearf(h, 2.0f)); // fim: max.y
}

void Test_RampOutsideLane()
{
    AABB ramp = Box(-1, 0, 0, 1, 2, 4);
    float h = 123.0f;
    assert(!RampHeightAt(ramp, 1.5f, 2.0f, &h));
    assert(!RampHeightAt(ramp, -1.5f, 2.0f, &h));
    assert(!RampHeightAt(ramp, 0.0f, 4.5f, &h));  // além do fim
    assert(!RampHeightAt(ramp, 0.0f, -0.5f, &h)); // antes do começo
}

void Test_RampZeroLength() // sem divisão por zero
{
    AABB ramp = Box(-1, 0, 3, 1, 2, 3);
    float h = -1.0f;
    assert(RampHeightAt(ramp, 0.0f, 3.0f, &h));
    assert(std::isfinite(h));
}

void Test_LandsOnTopFromAbove()
{
    AABB train = Box(-1, 0, 10, 1, 3, 20);
    AABB prev  = Box(-0.4f, 3.2f, 14, 0.4f, 5.0f, 14.8f); // base acima do teto
    AABB now   = Box(-0.4f, 2.95f, 14.5f, 0.4f, 4.75f, 15.3f); // base atravessou o teto
    assert(LandsOnTop(prev, now, train, 0.1f));
}

void Test_LandsOnTopFrontalHitIsFalse()
{
    AABB train = Box(-1, 0, 10, 1, 3, 20);
    AABB prev  = Box(-0.4f, 0, 9, 0.4f, 1.8f, 9.8f);      // no chão, antes do trem
    AABB now   = Box(-0.4f, 0, 9.5f, 0.4f, 1.8f, 10.3f);  // encostou na frente
    assert(!LandsOnTop(prev, now, train, 0.1f));
}

void Test_LandsOnTopLaterallyApart()
{
    AABB train = Box(-1, 0, 10, 1, 3, 20);
    AABB prev  = Box(2.6f, 3.2f, 14, 3.4f, 5.0f, 14.8f);
    AABB now   = Box(2.6f, 2.95f, 14.5f, 3.4f, 4.75f, 15.3f);
    assert(!LandsOnTop(prev, now, train, 0.1f));
}

int main()
{
    Test_TrackStraightFrame();
    Test_TrackDiscardKeepsFrames();
    Test_StripVertexCount();
    Test_TrackFrameOutOfRange();
    Test_TrackDeterministicSeed();
    Test_TrackCurveContinuity();
    Test_TrackLeftCurveEndsHeadingLeft();
    Test_LaneClamp();
    Test_LaneMoveTime();
    Test_LaneNoOvershoot();
    Test_JumpLands();
    Test_NoDoubleJump();
    Test_SpeedCap();
    Test_SpeedRamp();
    Test_DtIndependent();
    Test_DtClamped();
    Test_RollTimerExpires();
    Test_JumpCancelsRoll();
    Test_RollInAirDescendsFaster();
    Test_InputIgnoredWhenNotPlaying();
    Test_GameResetRestoresState();
    Test_GameKeepsTrackAhead();
    Test_CameraSnapEqualsTarget();
    Test_CameraConverges();
    Test_CameraDtIndependent();
    Test_CameraTunnelLowerCloser();
    Test_SceneryFitsCapacity();
    Test_RailsAboveGroundAndInsideBed();
    Test_BuildingsCoverVisibleRange();
    printf("OK\n");
    return 0;
}
