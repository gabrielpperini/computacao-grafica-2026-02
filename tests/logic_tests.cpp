// Testes unitários dos módulos de lógica (sem OpenGL/GLFW). Cada módulo novo
// adiciona suas funções Test_*() e as chama em main().
// Os asserts precisam estar sempre ativos, mesmo em builds com NDEBUG.
#undef NDEBUG
#include <algorithm>
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
// Estes testes só exercitam a física do jogador: spawn_enabled = false para
// que obstáculos gerados (Tarefa 2.2) não encerrem a corrida no meio.

void Test_LaneClamp()
{
    Game g(1);
    g.spawn_enabled = false;
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
    g.spawn_enabled = false;
    g.HandleInput(Input::RIGHT);
    for (int i = 0; i < 60; i++) g.Update(1 / 60.f);
    assert(std::fabs(g.player.x - LANE_WIDTH) < 1e-4);
}

void Test_LaneNoOvershoot() // x nunca passa do alvo, nem com dt grande
{
    Game g(1);
    g.spawn_enabled = false;
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
    g.spawn_enabled = false;
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
    g.spawn_enabled = false;
    g.HandleInput(Input::JUMP);
    g.Update(0.1f);
    float vy = g.player.vy;
    g.HandleInput(Input::JUMP);
    assert(g.player.vy == vy);
}

void Test_SpeedCap()
{
    Game g(1);
    g.spawn_enabled = false;
    for (int i = 0; i < 100000; i++) g.Update(MAX_DT);
    assert(g.player.speed == PLAYER_MAX_SPEED);
}

void Test_SpeedRamp() // 5 s de jogo -> +5 * PLAYER_ACCEL
{
    Game g(1);
    g.spawn_enabled = false;
    for (int i = 0; i < 100; i++) g.Update(MAX_DT);
    assert(nearf(g.player.speed, PLAYER_START_SPEED + 5 * PLAYER_ACCEL, 1e-3f));
}

void Test_DtIndependent()
{
    Game a(1), b(1);
    a.spawn_enabled = false;
    b.spawn_enabled = false;
    for (int i = 0; i < 60; i++) a.Update(1 / 60.f);
    for (int i = 0; i < 240; i++) b.Update(1 / 240.f);
    assert(std::fabs(a.player.s - b.player.s) < 0.01f);
}

void Test_DtClamped() // dt gigante vale no máximo MAX_DT
{
    Game a(1), b(1);
    a.spawn_enabled = false;
    b.spawn_enabled = false;
    a.Update(5.0f);
    b.Update(MAX_DT);
    assert(a.player.s == b.player.s);
    a.Update(-1.0f); // dt negativo não anda para trás
    assert(a.player.s == b.player.s);
}

void Test_RollTimerExpires()
{
    Game g(1);
    g.spawn_enabled = false;
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
    g.spawn_enabled = false;
    g.HandleInput(Input::ROLL);
    g.HandleInput(Input::JUMP);
    assert(g.player.roll_timer == 0);
    assert(g.player.vy > 0 && !g.player.grounded);
}

void Test_RollInAirDescendsFaster()
{
    Game g(1);
    g.spawn_enabled = false;
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
    g.spawn_enabled = false;
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
    g.spawn_enabled = false;
    fresh.spawn_enabled = false;
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
    g.spawn_enabled = false;
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

// ------------------------------------------- Obstáculos, moedas e game over (2.2)

// Jogo sem spawn automático (cenário montado à mão pelo teste).
static Game QuietGame()
{
    Game g(1);
    g.spawn_enabled = false;
    return g;
}

static void Run(Game& g, float seconds, float dt = 1.0f / 60.0f)
{
    int steps = (int)(seconds / dt + 0.5f);
    for (int i = 0; i < steps; i++) g.Update(dt);
}

static bool IsBlocking(ObstacleType t)
{
    return t == ObstacleType::SIGN || t == ObstacleType::TRAIN;
}

void Test_EveryChunkHasPassableLane()
{
    const int chunks = 500;
    const int samples = (int)(chunks * CHUNK_LENGTH);
    for (unsigned seed = 1; seed <= 20; seed++) {
        Game g(seed);
        g.GenerateChunksUntil(chunks * CHUNK_LENGTH);
        assert(g.obstacles.size() > (size_t)chunks); // gerou de fato
        // blocked[lane][i]: faixa obstruída (placa/trem) em s = i metros,
        // contando a profundidade do jogador.
        std::vector<char> blocked[3];
        std::vector<char> chunk_blocked[3];
        for (int l = 0; l < 3; l++) {
            blocked[l].assign(samples + 1, 0);
            chunk_blocked[l].assign(chunks, 0);
        }
        for (size_t k = 0; k < g.obstacles.size(); k++) {
            const Obstacle& o = g.obstacles[k];
            assert(o.lane >= -1 && o.lane <= 1);
            if (!IsBlocking(o.type))
                continue;
            int i0 = std::max(0, (int)std::ceil(o.box.min.z - PLAYER_HALF_DEPTH));
            int i1 = std::min(samples, (int)std::floor(o.box.max.z + PLAYER_HALF_DEPTH));
            for (int i = i0; i <= i1; i++) blocked[o.lane + 1][i] = 1;
            int c0 = std::max(0, (int)std::floor(o.box.min.z / CHUNK_LENGTH));
            int c1 = std::min(chunks - 1, (int)std::floor(o.box.max.z / CHUNK_LENGTH));
            for (int c = c0; c <= c1; c++) chunk_blocked[o.lane + 1][c] = 1;
        }
        // Em todo s (a cada 1 m) há uma faixa sem placa/trem...
        for (int i = 0; i <= samples; i++)
            assert(!blocked[0][i] || !blocked[1][i] || !blocked[2][i]);
        // ...e em todo chunk há uma faixa livre do começo ao fim dele (inclusive
        // de trens que venham do chunk anterior).
        for (int c = 0; c < chunks; c++)
            assert(!chunk_blocked[0][c] || !chunk_blocked[1][c] || !chunk_blocked[2][c]);
    }
}

// Entre dois chunks há um trecho sem NENHUM obstáculo, longo o bastante para
// atravessar duas faixas na velocidade máxima: o jogador sempre alcança a faixa
// livre do chunk seguinte, qualquer que seja a faixa em que está.
void Test_ChunkBordersAllowLaneChange()
{
    float two_lanes = 2.0f * LANE_WIDTH / LANE_SWITCH_SPEED * PLAYER_MAX_SPEED + 2.0f * PLAYER_HALF_DEPTH;
    assert(CHUNK_CLEAR_TAIL + CHUNK_CLEAR_HEAD >= two_lanes);
    for (unsigned seed = 1; seed <= 20; seed++) {
        Game g(seed);
        g.GenerateChunksUntil(200 * CHUNK_LENGTH);
        for (size_t k = 0; k < g.obstacles.size(); k++) {
            const AABB& b = g.obstacles[k].box;
            float c = std::floor(b.min.z / CHUNK_LENGTH) * CHUNK_LENGTH; // início do chunk
            assert(b.min.z >= c + CHUNK_CLEAR_HEAD - 1e-3f);
            assert(b.max.z <= c + CHUNK_LENGTH - CHUNK_CLEAR_TAIL + 1e-3f);
        }
    }
}

void Test_LowHighNeverTogetherInLane()
{
    for (unsigned seed = 1; seed <= 20; seed++) {
        Game g(seed);
        g.GenerateChunksUntil(500 * CHUNK_LENGTH);
        const std::vector<Obstacle>& o = g.obstacles;
        for (size_t i = 0; i < o.size(); i++) {
            if (o[i].type != ObstacleType::BARRIER_LOW)
                continue;
            for (size_t j = 0; j < o.size(); j++)
                if (o[j].type == ObstacleType::BARRIER_HIGH && o[j].lane == o[i].lane)
                    assert(std::fabs(o[j].s - o[i].s) >= LOW_HIGH_MIN_GAP);
        }
    }
}

void Test_FirstMetersObstacleFree()
{
    for (unsigned seed = 1; seed <= 20; seed++) {
        Game g(seed);
        g.GenerateChunksUntil(10 * CHUNK_LENGTH);
        for (size_t k = 0; k < g.obstacles.size(); k++)
            assert(g.obstacles[k].box.min.z >= OBSTACLE_FREE_START);
    }
}

// Obstáculos ficam dentro da própria faixa (não invadem a vizinha), apoiados
// no chão (ou, na barreira alta, a partir do vão), e sem rampas nesta fase.
void Test_ObstaclesInsideLane()
{
    Game g(3);
    g.GenerateChunksUntil(300 * CHUNK_LENGTH);
    for (size_t k = 0; k < g.obstacles.size(); k++) {
        const Obstacle& o = g.obstacles[k];
        assert(o.type != ObstacleType::RAMP);
        float cx = o.lane * LANE_WIDTH;
        assert(o.box.min.x >= cx - 0.5f * LANE_WIDTH + LANE_OBSTACLE_MARGIN - 1e-4f);
        assert(o.box.max.x <= cx + 0.5f * LANE_WIDTH - LANE_OBSTACLE_MARGIN + 1e-4f);
        assert(nearf(0.5f * (o.box.min.z + o.box.max.z), o.s));
        if (o.type == ObstacleType::BARRIER_HIGH)
            assert(nearf(o.box.min.y, BARRIER_HIGH_CLEARANCE));
        else
            assert(nearf(o.box.min.y, 0.0f));
    }
}

void Test_ObstacleExtentsSetter()
{
    Game g(1);
    g.SetObstacleExtents(ObstacleType::SIGN, glm::vec3(0.5f, 1.0f, 0.2f));
    Obstacle o = g.MakeObstacle(ObstacleType::SIGN, 1, 50.0f);
    assert(o.lane == 1 && o.s == 50.0f && o.velocity == 0.0f);
    assert(nearf(o.box.min.x, 2.0f) && nearf(o.box.max.x, 3.0f));
    assert(nearf(o.box.min.y, 0.0f) && nearf(o.box.max.y, 2.0f));
    assert(nearf(o.box.min.z, 49.8f) && nearf(o.box.max.z, 50.2f));
    // Largura maior que a faixa é limitada (não bloqueia a faixa vizinha).
    g.SetObstacleExtents(ObstacleType::TRAIN, glm::vec3(5.0f, 1.6f, 6.0f));
    Obstacle t = g.MakeObstacle(ObstacleType::TRAIN, 0, 100.0f);
    assert(nearf(t.box.max.x, 0.5f * LANE_WIDTH - LANE_OBSTACLE_MARGIN));
    // Barreira alta começa no vão livre.
    Obstacle h = g.MakeObstacle(ObstacleType::BARRIER_HIGH, -1, 70.0f);
    assert(nearf(h.box.min.y, BARRIER_HIGH_CLEARANCE));
}

void Test_PlayerBoxRollHeight()
{
    Game g = QuietGame();
    AABB b = g.PlayerBox();
    assert(nearf(b.min.y, 0.0f) && nearf(b.max.y, PLAYER_HEIGHT));
    assert(nearf(b.min.x, -PLAYER_HALF_WIDTH) && nearf(b.max.z, PLAYER_HALF_DEPTH));
    g.HandleInput(Input::ROLL);
    b = g.PlayerBox();
    assert(nearf(b.max.y, PLAYER_ROLL_HEIGHT));
}

void Test_HitBarrierGameOver()
{
    Game g = QuietGame();
    g.obstacles.push_back(g.MakeObstacle(ObstacleType::BARRIER_LOW, 0, 6.0f));
    Run(g, 2.0f);
    assert(g.mode == GameMode::GAME_OVER);
    assert(g.player.s < 6.0f); // parou ao bater, não atravessou
    float s = g.player.s, score = g.score;
    g.Update(0.05f); // em GAME_OVER nada anda
    assert(g.player.s == s && g.score == score);

    // Obstáculo na faixa vizinha: sem colisão "fantasma".
    Game h = QuietGame();
    h.obstacles.push_back(h.MakeObstacle(ObstacleType::SIGN, 1, 6.0f));
    h.obstacles.push_back(h.MakeObstacle(ObstacleType::TRAIN, -1, 12.0f));
    Run(h, 2.0f);
    assert(h.mode == GameMode::PLAYING && h.player.s > 20.0f);
}

void Test_JumpOverLowBarrier()
{
    Game g = QuietGame();
    g.obstacles.push_back(g.MakeObstacle(ObstacleType::BARRIER_LOW, 0, 4.0f));
    g.HandleInput(Input::JUMP);
    Run(g, 2.0f);
    assert(g.mode == GameMode::PLAYING && g.player.s > 20.0f);

    // Rolando não passa por baixo da barreira baixa.
    Game r = QuietGame();
    r.obstacles.push_back(r.MakeObstacle(ObstacleType::BARRIER_LOW, 0, 4.0f));
    r.HandleInput(Input::ROLL);
    Run(r, 2.0f);
    assert(r.mode == GameMode::GAME_OVER);
}

void Test_RollUnderHighBarrier()
{
    Game g = QuietGame();
    g.obstacles.push_back(g.MakeObstacle(ObstacleType::BARRIER_HIGH, 0, 5.0f));
    g.HandleInput(Input::ROLL);
    Run(g, 2.0f);
    assert(g.mode == GameMode::PLAYING && g.player.s > 20.0f);

    // Em pé ou pulando, bate.
    Game st = QuietGame();
    st.obstacles.push_back(st.MakeObstacle(ObstacleType::BARRIER_HIGH, 0, 5.0f));
    Run(st, 2.0f);
    assert(st.mode == GameMode::GAME_OVER);
    Game j = QuietGame();
    j.obstacles.push_back(j.MakeObstacle(ObstacleType::BARRIER_HIGH, 0, 5.0f));
    j.HandleInput(Input::JUMP);
    Run(j, 2.0f);
    assert(j.mode == GameMode::GAME_OVER);
}

// A barreira alta tem dois postes nas bordas (desenhados e com colisão), fora
// do caminho do jogador centrado na faixa.
void Test_HighBarrierPosts()
{
    Game g = QuietGame();
    Obstacle h = g.MakeObstacle(ObstacleType::BARRIER_HIGH, 0, 5.0f);
    AABB parts[3];
    int n = ObstacleParts(h, parts);
    assert(n == 3);
    assert(parts[0].min.y == h.box.min.y); // a viga é a própria caixa
    for (int i = 1; i < 3; i++) {
        assert(nearf(parts[i].min.y, 0.0f) && nearf(parts[i].max.y, BARRIER_HIGH_CLEARANCE));
        assert(std::fabs(parts[i].min.x) > PLAYER_HALF_WIDTH || std::fabs(parts[i].max.x) > PLAYER_HALF_WIDTH);
        assert(parts[i].min.x >= h.box.min.x && parts[i].max.x <= h.box.max.x);
        assert(!AABBIntersectsAABB(parts[i], MakeAABB(glm::vec3(0, 0.4f, 5.0f),
                                                      glm::vec3(PLAYER_HALF_WIDTH, 0.4f, PLAYER_HALF_DEPTH))));
    }
    Obstacle low = g.MakeObstacle(ObstacleType::BARRIER_LOW, 0, 5.0f);
    assert(ObstacleParts(low, parts) == 1);
}

void Test_SignAndTrainOnlyByDodging()
{
    const ObstacleType types[2] = {ObstacleType::SIGN, ObstacleType::TRAIN};
    for (int t = 0; t < 2; t++) {
        Game j = QuietGame();
        j.obstacles.push_back(j.MakeObstacle(types[t], 0, 10.0f));
        j.HandleInput(Input::JUMP);
        Run(j, 2.0f);
        assert(j.mode == GameMode::GAME_OVER);

        Game r = QuietGame();
        r.obstacles.push_back(r.MakeObstacle(types[t], 0, 10.0f));
        r.HandleInput(Input::ROLL);
        Run(r, 2.0f);
        assert(r.mode == GameMode::GAME_OVER);

        Game d = QuietGame();
        d.obstacles.push_back(d.MakeObstacle(types[t], 0, 10.0f));
        d.HandleInput(Input::RIGHT);
        Run(d, 3.0f);
        assert(d.mode == GameMode::PLAYING && d.player.s > 30.0f);
    }
}

// Na velocidade máxima com o maior dt (1.5 m por passo), uma barreira de
// 0.3 m que cairia entre a caixa do quadro anterior e a do atual não pode ser
// "pulada" pela simulação: a colisão usa a caixa varrida.
void Test_SweptBoxNoTunneling()
{
    Game g = QuietGame();
    g.SetObstacleExtents(ObstacleType::BARRIER_LOW, glm::vec3(1.0f, 0.45f, 0.15f));
    g.obstacles.push_back(g.MakeObstacle(ObstacleType::BARRIER_LOW, 0, 0.75f)); // z em [0.6, 0.9]
    g.player.speed = PLAYER_MAX_SPEED;
    AABB before = g.PlayerBox(); // z em [-0.4, 0.4]
    g.Update(MAX_DT);            // s = 1.5 -> z em [1.1, 1.9]
    assert(nearf(g.player.s, 1.5f));
    assert(before.max.z < 0.6f && g.PlayerBox().min.z > 0.9f); // nenhuma das duas toca
    assert(g.mode == GameMode::GAME_OVER);
}

void Test_CoinCollectedOnce()
{
    Game g = QuietGame();
    Coin c = {0, 3.0f, COIN_HEIGHT, false};
    Coin other = {1, 3.0f, COIN_HEIGHT, false}; // faixa vizinha: não pega
    g.coins.push_back(c);
    g.coins.push_back(other);
    Run(g, 1.0f);
    assert(g.coins_collected == 1);
    assert(g.coins[0].collected && !g.coins[1].collected);
    assert(nearf(g.score, g.player.s + 10.0f * g.multiplier, 1e-3f));
    Run(g, 1.0f);
    assert(g.coins_collected == 1);
    assert(g.mode == GameMode::PLAYING);
}

void Test_ScoreFollowsDistance()
{
    Game g = QuietGame();
    assert(g.score == 0 && g.coins_collected == 0 && g.multiplier == 1);
    Run(g, 3.0f);
    assert(nearf(g.score, g.player.s, 1e-3f));
}

void Test_DespawnBehind()
{
    Game g(3);
    g.Update(1.0f / 60.0f);
    assert(!g.obstacles.empty() && !g.coins.empty());
    float first_obstacle = g.obstacles.front().s;
    // "Teletransporta" o jogador; o passo seguinte descarta o que ficou mais
    // de DESPAWN_BEHIND para trás e gera até player.s + SPAWN_AHEAD.
    g.player.s = 600.0f;
    g.Update(1.0f / 60.0f);
    float s = g.player.s;
    assert(!g.obstacles.empty() && g.obstacles.front().s > first_obstacle);
    float farthest = 0.0f;
    for (size_t k = 0; k < g.obstacles.size(); k++) {
        assert(g.obstacles[k].box.max.z >= s - DESPAWN_BEHIND);
        farthest = std::max(farthest, g.obstacles[k].box.max.z);
    }
    for (size_t k = 0; k < g.coins.size(); k++)
        assert(g.coins[k].s >= s - DESPAWN_BEHIND);
    assert(farthest > s + SPAWN_AHEAD - 2.0f * CHUNK_LENGTH);
    assert(farthest <= s + SPAWN_AHEAD);
}

// O 1º chunk traz uma fileira de moedas na faixa inicial (sem obstáculos):
// correr sem tocar em nada já coleta moedas (usado pelo E2E "coins").
void Test_WelcomeCoins()
{
    Game g(42);
    Run(g, 3.0f);
    assert(g.mode == GameMode::PLAYING);
    assert(g.coins_collected == WELCOME_COIN_COUNT);
    assert(nearf(g.score, g.player.s + 10.0f * WELCOME_COIN_COUNT, 1e-2f));
}

void Test_ResetClearsRunAndRespawnsSame()
{
    Game g(5), fresh(5);
    g.Update(1.0f / 60.0f);
    fresh.Update(1.0f / 60.0f);
    Run(g, 30.0f); // anda até bater (sem entrada)
    assert(g.mode == GameMode::GAME_OVER);
    g.Reset();
    assert(g.mode == GameMode::PLAYING && g.score == 0 && g.coins_collected == 0);
    assert(g.multiplier == 1 && g.obstacles.empty() && g.coins.empty());
    assert(g.spawn_enabled);
    g.Update(1.0f / 60.0f);
    assert(g.obstacles.size() == fresh.obstacles.size() && g.coins.size() == fresh.coins.size());
    for (size_t k = 0; k < g.obstacles.size(); k++) {
        assert(g.obstacles[k].type == fresh.obstacles[k].type);
        assert(g.obstacles[k].lane == fresh.obstacles[k].lane);
        assert(g.obstacles[k].s == fresh.obstacles[k].s);
    }
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
    // Colisões (2.1)
    Test_MakeAABB();
    Test_AABBTouchingIsHit();
    Test_AABBSeparatedInS();
    Test_AABBOverlap();
    Test_SphereCornerMiss();
    Test_SphereInside();
    Test_SphereIntersectsSphere();
    Test_RampMidHeight();
    Test_RampEnds();
    Test_RampOutsideLane();
    Test_RampZeroLength();
    Test_LandsOnTopFromAbove();
    Test_LandsOnTopFrontalHitIsFalse();
    Test_LandsOnTopLaterallyApart();
    // Obstáculos, moedas e game over (2.2)
    Test_EveryChunkHasPassableLane();
    Test_ChunkBordersAllowLaneChange();
    Test_LowHighNeverTogetherInLane();
    Test_FirstMetersObstacleFree();
    Test_ObstaclesInsideLane();
    Test_ObstacleExtentsSetter();
    Test_PlayerBoxRollHeight();
    Test_HitBarrierGameOver();
    Test_JumpOverLowBarrier();
    Test_RollUnderHighBarrier();
    Test_HighBarrierPosts();
    Test_SignAndTrainOnlyByDodging();
    Test_SweptBoxNoTunneling();
    Test_CoinCollectedOnce();
    Test_ScoreFollowsDistance();
    Test_DespawnBehind();
    Test_WelcomeCoins();
    Test_ResetClearsRunAndRespawnsSame();
    printf("OK\n");
    return 0;
}
