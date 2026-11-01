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
    // Com curvas a posição em s = 400 não é mais (0, 0, -400): o que importa é
    // que descartar o começo da pista não muda o referencial do que ficou.
    Track t(1);
    t.EnsureGeneratedUntil(500);
    TrackFrame before = t.FrameAt(400);
    t.DiscardBefore(300);
    TrackFrame after = t.FrameAt(400);
    assert(near(after.position, before.position) && nearf(after.heading, before.heading));
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

void Test_Curve90EndsPerpendicular()
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

// ------------------------------------------------ Gerador de curvas (4.1)

static const float kPi = 3.14159265f;

// Regras do gerador: 1º segmento reto >= 100 m (nada de curva logo na
// largada); depois alterna reta (60-150 m) e curva (|k| = 1/40, ângulo total
// de 45° a 90°, para os dois lados); determinístico pela semente.
void Test_TrackGeneratorRules()
{
    int left = 0, right = 0;
    for (unsigned seed = 1; seed <= 20; seed++) {
        Track t(seed);
        t.EnsureGeneratedUntil(20000.0f);
        const std::vector<TrackSegment>& segs = t.Segments();
        assert(segs.size() > 100);
        assert(segs[0].curvature == 0.0f && segs[0].s_start == 0.0f);
        assert(segs[0].length >= TRACK_FIRST_STRAIGHT_MIN_LENGTH - 1e-3f);
        assert(segs[0].length <= TRACK_STRAIGHT_MAX_LENGTH + 1e-3f);
        for (size_t i = 0; i < segs.size(); i++) {
            const TrackSegment& g = segs[i];
            if (i > 0) {
                // Alterna: nunca duas curvas seguidas, e duas retas seguidas
                // só com um túnel entre elas (reta, túnel, reta; Tarefa 4.2).
                const TrackSegment& p = segs[i - 1];
                if (g.curvature != 0.0f)
                    assert(p.curvature == 0.0f && !p.tunnel);
                else if (p.curvature == 0.0f)
                    assert(g.tunnel != p.tunnel);
                assert(nearf(g.s_start, p.s_start + p.length, 1e-2f));
            }
            if (g.tunnel) {
                // Comprimento do túnel testado em Test_TunnelOnlyOnStraight.
            } else if (g.curvature == 0.0f) {
                assert(g.length >= TRACK_STRAIGHT_MIN_LENGTH - 1e-3f);
                assert(g.length <= TRACK_STRAIGHT_MAX_LENGTH + 1e-3f);
            } else {
                assert(nearf(std::fabs(g.curvature), 1.0f / TRACK_CURVE_RADIUS, 1e-6f));
                float angle = g.length * std::fabs(g.curvature);
                assert(angle >= kPi / 4.0f - 1e-3f && angle <= kPi / 2.0f + 1e-3f);
                if (g.curvature > 0.0f) left++; else right++;
            }
        }
        // Mesma semente -> mesma pista.
        Track u(seed);
        u.EnsureGeneratedUntil(20000.0f);
        assert(u.Segments().size() == segs.size());
        for (size_t i = 0; i < segs.size(); i++)
            assert(u.Segments()[i].length == segs[i].length &&
                   u.Segments()[i].curvature == segs[i].curvature &&
                   u.Segments()[i].tunnel == segs[i].tunnel);
    }
    assert(left > 100 && right > 100); // curvas para os dois lados
}

// Em toda junção de segmentos da pista gerada, posição e heading são
// contínuos (sem degrau entre o fim de um segmento e o começo do outro).
void Test_CurveContinuity()
{
    for (unsigned seed = 1; seed <= 20; seed++) {
        Track t(seed);
        t.EnsureGeneratedUntil(5000.0f);
        const std::vector<TrackSegment>& segs = t.Segments();
        for (size_t i = 1; i < segs.size(); i++) {
            float j = segs[i].s_start;
            TrackFrame a = t.FrameAt(j - 1e-3f), b = t.FrameAt(j + 1e-3f);
            assert(near(a.position, b.position, 1e-2f));
            assert(nearf(a.heading, b.heading, 1e-3f));
            assert(near(a.forward, b.forward, 1e-3f));
        }
    }
}

// ------------------------------------------------------- Túneis (4.2)

// Túneis só em retas: segmento com tunnel = true tem curvatura 0, nunca é a
// reta da largada e fica entre duas retas comuns (reta, túnel, reta), de modo
// que o portal é visto de longe, de frente, e a saída também dá numa reta.
// Comprimento de 40 a 80 m, múltiplo do módulo do túnel (4 m). Aparecem em
// cerca de 1 de cada 3 retas.
void Test_TunnelOnlyOnStraight()
{
    int tunnels = 0, curves = 0;
    for (unsigned seed = 1; seed <= 20; seed++) {
        Track t(seed);
        t.EnsureGeneratedUntil(20000.0f);
        const std::vector<TrackSegment>& segs = t.Segments();
        assert(!segs[0].tunnel);
        for (size_t i = 0; i < segs.size(); i++) {
            const TrackSegment& g = segs[i];
            if (g.curvature != 0.0f) {
                assert(!g.tunnel);
                curves++;
                continue;
            }
            if (!g.tunnel)
                continue;
            tunnels++;
            assert(i > 0);
            assert(segs[i - 1].curvature == 0.0f && !segs[i - 1].tunnel);
            if (i + 1 < segs.size()) // (o último gerado ainda não tem sucessor)
                assert(segs[i + 1].curvature == 0.0f && !segs[i + 1].tunnel);
            assert(g.length >= TRACK_TUNNEL_MIN_LENGTH - 1e-3f);
            assert(g.length <= TRACK_TUNNEL_MAX_LENGTH + 1e-3f);
            float k = g.length / TRACK_TUNNEL_SECTION;
            assert(nearf(k, std::floor(k + 0.5f), 1e-3f));
            assert(t.IsStraightRange(g.s_start, g.s_start + g.length - 1e-3f));
        }
    }
    // ~1 túnel a cada 3 retas elegíveis (uma por curva).
    assert(tunnels > 0);
    float ratio = (float)tunnels / (float)curves;
    assert(ratio > 0.2f && ratio < 0.5f);
}

// IsTunnelAt: verdadeiro dentro do segmento de túnel, falso fora (inclusive
// logo antes do portal de entrada e logo depois da saída). TunnelOverlaps
// detecta qualquer intersecção do intervalo [s0, s1] com um túnel.
void Test_IsTunnelAtBounds()
{
    Track t(1);
    float a = t.Segments()[0].length; // fim da reta da largada
    t.AppendSegment(60.0f, 0.0f, true);
    t.AppendSegment(100.0f, 0.0f, false);
    float b = a + 60.0f;
    assert(!t.IsTunnelAt(0.0f) && !t.IsTunnelAt(a - 0.01f));
    assert(t.IsTunnelAt(a + 0.01f) && t.IsTunnelAt(a + 30.0f) && t.IsTunnelAt(b - 0.01f));
    assert(!t.IsTunnelAt(b + 0.01f) && !t.IsTunnelAt(b + 50.0f));
    assert(!t.TunnelOverlaps(0.0f, a - 1.0f));
    assert(t.TunnelOverlaps(a - 1.0f, a + 1.0f));
    assert(t.TunnelOverlaps(a - 10.0f, b + 10.0f));
    assert(t.TunnelOverlaps(b - 1.0f, b + 1.0f));
    assert(!t.TunnelOverlaps(b + 1.0f, b + 90.0f));
    // A mesma semente continua gerando a mesma pista com túneis.
    Track u(7), v(7);
    u.EnsureGeneratedUntil(5000.0f);
    v.EnsureGeneratedUntil(2000.0f);
    v.EnsureGeneratedUntil(5000.0f);
    assert(u.Segments().size() == v.Segments().size());
    for (size_t i = 0; i < u.Segments().size(); i++)
        assert(u.Segments()[i].tunnel == v.Segments()[i].tunnel &&
               u.Segments()[i].length == v.Segments()[i].length);
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

// Pista reta bem longa (heading 0: forward = -Z, right = +X, s = -Z): o 1º
// segmento gerado já é reto e AppendSegment acrescenta mais 1 km de reta.
// Sem EnsureGeneratedUntil depois, o gerador não chega a pôr curvas.
static Track StraightTrack()
{
    Track t(1);
    t.AppendSegment(1000.0f, 0.0f, false);
    return t;
}

void Test_CameraSnapEqualsTarget()
{
    ThirdPersonCamera cam;
    Track t = StraightTrack(); // jogador em s = 10: (0, 0, -10)
    cam.Snap(t, 10.0f, LANE_WIDTH, false);
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
    Track t = StraightTrack();
    cam.Snap(t, 0.0f, 0.0f, false);
    float target = LANE_WIDTH / 3.0f;
    cam.Update(t, 0.0f, LANE_WIDTH, false, 1.0f / 60.0f);
    float x1 = cam.Pose().position.x;
    assert(x1 > 0.0f && x1 < 0.5f * target); // suave: não pula para o alvo
    float prev = x1;
    for (int i = 0; i < 120; i++) { // 2 s
        cam.Update(t, 0.0f, LANE_WIDTH, false, 1.0f / 60.0f);
        float x = cam.Pose().position.x;
        assert(x >= prev && x <= target + kEps); // monotônico, sem overshoot
        prev = x;
    }
    assert(nearf(prev, target, 1e-3f));
}

void Test_CameraDtIndependent() // 30 Hz e 120 Hz chegam ao mesmo lugar
{
    Track t = StraightTrack();
    ThirdPersonCamera a, b;
    a.Snap(t, 0.0f, 0.0f, false);
    b.Snap(t, 0.0f, 0.0f, false);
    // 0.5 s: jogador anda a 12 m/s, troca para a faixa da direita e entra
    // num túnel (alvo de altura/recuo muda no meio do caminho).
    for (int i = 1; i <= 15; i++) {
        float s = 12.0f * i / 30.0f;
        a.Update(t, s, LANE_WIDTH, true, 1.0f / 30.0f);
    }
    for (int i = 1; i <= 60; i++) {
        float s = 12.0f * i / 120.0f;
        b.Update(t, s, LANE_WIDTH, true, 1.0f / 120.0f);
    }
    assert(near(a.Pose().position, b.Pose().position, 1e-3f));
    assert(near(a.Pose().view_vector, b.Pose().view_vector, 1e-3f));
    // E a câmera acompanha o avanço do jogador sem atraso (z ~ -6 + recuo).
    assert(a.Pose().position.z < -6.0f + CAMERA_BACK);
}

void Test_CameraTunnelLowerCloser()
{
    Track t = StraightTrack();
    TrackFrame f = t.FrameAt(20.0f);
    ThirdPersonCamera out, in;
    out.Snap(t, 20.0f, 0.0f, false);
    in.Snap(t, 20.0f, 0.0f, true);
    vec4 po = out.Pose().position, pi = in.Pose().position;
    assert(pi.y < po.y);                                     // mais baixa
    assert(std::fabs(pi.z - f.position.z) < std::fabs(po.z - f.position.z)); // mais perto
    assert(nearf(pi.y, CAMERA_TUNNEL_HEIGHT));
    assert(nearf(pi.z, f.position.z + CAMERA_TUNNEL_BACK));
    // Acessores (main.cpp usa Back() para saber se a CÂMERA está no túnel).
    assert(nearf(in.Height(), CAMERA_TUNNEL_HEIGHT) && nearf(in.Back(), CAMERA_TUNNEL_BACK));
    assert(nearf(out.Height(), CAMERA_HEIGHT) && nearf(out.Back(), CAMERA_BACK));
    // A transição é suave: um passo de 1/60 s não chega ao alvo do túnel.
    out.Update(t, 20.0f, 0.0f, true, 1.0f / 60.0f);
    float y = out.Pose().position.y;
    assert(y < po.y && y > CAMERA_TUNNEL_HEIGHT + 0.5f);
}

// Na curva a câmera acompanha a pista: fica sobre a própria pista CAMERA_BACK
// atrás do jogador e olha para o ponto da pista CAMERA_LOOK_AHEAD à frente
// (e não para a tangente), de modo que o enquadramento "entra" na curva.
// Nas junções reta/curva a pose varia continuamente (sem tranco).
void Test_CameraFollowsCurve()
{
    const float R = TRACK_CURVE_RADIUS;
    Track t(1);
    const float s0 = t.Segments()[0].length; // início da curva
    t.AppendSegment(0.5f * kPi * R, 1.0f / R, false); // 90° à esquerda
    t.AppendSegment(100.0f, 0.0f, false);
    const float s_mid = s0 + 0.25f * kPi * R;

    ThirdPersonCamera cam;
    cam.Snap(t, s_mid, 0.0f, false);
    CameraPose p = cam.Pose();
    TrackFrame behind = t.FrameAt(s_mid - CAMERA_BACK);
    TrackFrame ahead = t.FrameAt(s_mid + CAMERA_LOOK_AHEAD);
    vec4 up(0, 1, 0, 0);
    assert(near(p.position, behind.position + up * CAMERA_HEIGHT, 1e-3f));
    vec4 look = ahead.position + up * CAMERA_LOOK_HEIGHT;
    assert(near(p.view_vector, look - p.position, 1e-3f));

    // O jogador continua bem enquadrado: no plano horizontal, o ângulo entre
    // o view vector e a direção câmera->jogador é pequeno (< 5°).
    vec4 player = t.FrameAt(s_mid).position;
    vec4 to_player = player - p.position;
    float a1 = std::atan2(-p.view_vector.x, -p.view_vector.z);
    float a2 = std::atan2(-to_player.x, -to_player.z);
    assert(std::fabs(a1 - a2) < 5.0f * kPi / 180.0f);

    // Continuidade ao atravessar as junções (passos de 1 cm): a pose não salta.
    ThirdPersonCamera c2;
    c2.Snap(t, s0 - 20.0f, 0.0f, false);
    CameraPose prev = c2.Pose();
    for (float s = s0 - 20.0f; s < s0 + 0.5f * kPi * R + 20.0f; s += 0.01f) {
        c2.Update(t, s, 0.0f, false, 1.0f / 60.0f);
        CameraPose q = c2.Pose();
        assert(near(q.position, prev.position, 0.02f));
        assert(near(q.view_vector, prev.view_vector, 0.02f));
        prev = q;
    }
}

// ---------------------------------------------------------------- Cenário (1.3)

void Test_SceneryFitsCapacity() // nunca passa da capacidade dos buffers da GPU
{
    Game g(1);
    StripBatch ground, rails;
    // Valores fracionários de s: com o início das faixas preso à grade de
    // SCENERY_STEP, o número de pares varia com a parte fracionária.
    const float ss[] = {0.0f, 7.3f, 20.999f, 1234.567f, 5000.5f, 98765.4f, 98765.99f};
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
    Track t = StraightTrack(); // reta: x de mundo = lateral
    StripBatch rails;
    BuildRailStrips(t, 50.0f, &rails);
    int top = 0;
    for (size_t v = 0; v < rails.VertexCount(); v++) {
        // Trilho = barra apoiada nos dormentes: topo em RAIL_HEIGHT, laterais
        // descendo até o topo dos dormentes (nunca até o chão: sem Z-fighting).
        float y = rails.positions[4 * v + 1];
        assert(y > SLEEPER_HEIGHT - 1e-4f && y < RAIL_HEIGHT + 1e-4f);
        if (nearf(y, RAIL_HEIGHT) && nearf(rails.normals[4 * v + 1], 1.0f))
            top++;
        assert(std::fabs(rails.positions[4 * v + 0]) < TRACK_BED_HALF_WIDTH);
        // Normal unitária (topo: +Y; laterais: horizontais).
        float nx = rails.normals[4 * v + 0], ny = rails.normals[4 * v + 1], nz = rails.normals[4 * v + 2];
        assert(nearf(nx * nx + ny * ny + nz * nz, 1.0f));
    }
    assert(top > 0 && top < (int)rails.VertexCount());
}

void Test_SleepersUnderRailsFitCapacity()
{
    Game g(3);
    StripBatch sleepers;
    const float ss[] = {0.0f, 7.3f, 1234.567f, 98765.4f};
    for (float s : ss) {
        g.track.EnsureGeneratedUntil(s + SPAWN_AHEAD);
        BuildSleeperStrips(g.track, s, &sleepers);
        assert(!sleepers.first.empty());
        assert(sleepers.VertexCount() <= MaxSleeperVertices());
        assert(sleepers.first.size() % (3 * SLEEPER_FACES) == 0);
        for (size_t i = 0; i < sleepers.count.size(); i++)
            assert(sleepers.count[i] == 4);
        for (size_t v = 0; v < sleepers.VertexCount(); v++) {
            float y = sleepers.positions[4 * v + 1];
            assert(y > -1e-4f && y < SLEEPER_HEIGHT + 1e-4f && SLEEPER_HEIGHT < RAIL_HEIGHT);
        }
        assert(sleepers.uvs.size() == sleepers.VertexCount() * 2);
    }
    // Pista reta (s = -Z): o dormente cobre os dois trilhos da faixa e não
    // invade a faixa vizinha.
    Track t = StraightTrack();
    BuildSleeperStrips(t, 30.0f, &sleepers);
    float max_x = 0.0f;
    for (size_t v = 0; v < sleepers.VertexCount(); v++)
        max_x = std::max(max_x, std::fabs(sleepers.positions[4 * v + 0]));
    assert(nearf(max_x, LANE_WIDTH + 0.5f * SLEEPER_LENGTH));
    assert(0.5f * SLEEPER_LENGTH > 0.5f * RAIL_GAUGE + RAIL_WIDTH);
    assert(SLEEPER_LENGTH < LANE_WIDTH);
}

void Test_BuildingsCoverVisibleRange()
{
    std::vector<BuildingInstance> a, b;
    ComputeBuildings(StraightTrack(), 100.0f, &a);
    assert(!a.empty());
    for (size_t i = 0; i < a.size(); i++) {
        assert(a[i].s >= 100.0f - DESPAWN_BEHIND && a[i].s <= 100.0f + SPAWN_AHEAD);
        assert(nearf(std::fabs(a[i].x), BUILDING_OFFSET_X));
        assert(a[i].variant >= 0 && a[i].variant < BUILDING_VARIANT_COUNT);
        assert(a[i].tint >= 0 && a[i].tint < BUILDING_TINT_COUNT);
    }
    // Mesmo prédio (mesmo s e lado) mantém modelo e tom quando o jogador anda.
    ComputeBuildings(StraightTrack(), 130.0f, &b);
    int matched = 0;
    std::vector<int> seen(BUILDING_VARIANT_COUNT, 0);
    for (size_t i = 0; i < a.size(); i++) {
        seen[a[i].variant]++;
        for (size_t j = 0; j < b.size(); j++)
            if (a[i].s == b[j].s && a[i].x == b[j].x) {
                assert(a[i].variant == b[j].variant && a[i].tint == b[j].tint);
                matched++;
            }
    }
    assert(matched > 10);
    // Variedade: todos os modelos aparecem no trecho visível.
    for (int v = 0; v < BUILDING_VARIANT_COUNT; v++)
        assert(seen[v] > 0);
}

// ------------------------------------------- Cenário em curvas (4.1)

// As faixas começam num múltiplo de SCENERY_STEP (grade fixa em s): na curva
// os vértices não "deslizam" ao longo do arco de um quadro para o outro (o
// chão facetado ficaria tremendo). Só o último par (no horizonte) acompanha
// o jogador.
void Test_StripsSnapToGrid()
{
    Track t(2);
    t.EnsureGeneratedUntil(3000.0f);
    StripBatch a, b;
    BuildGroundStrips(t, 1000.3f, &a);
    BuildGroundStrips(t, 1000.75f, &b);
    // Mesmo início de grade (980) nos dois quadros: os pares coincidem.
    TrackFrame f = t.FrameAt(980.0f);
    vec4 p0 = f.position - f.right * TRACK_BED_HALF_WIDTH;
    assert(nearf(a.positions[0], p0.x, 1e-3f) && nearf(a.positions[2], p0.z, 1e-3f));
    for (int v = 0; v < 2 * 100; v++)
        for (int c = 0; c < 4; c++)
            assert(a.positions[4 * v + c] == b.positions[4 * v + c]);
    // O trecho visível inteiro continua coberto: começa até player_s - DESPAWN_BEHIND.
    StripBatch r;
    BuildRailStrips(t, 1000.3f, &r);
    vec4 q = t.FrameAt(980.0f).position;
    float dx = r.positions[0] - q.x, dz = r.positions[2] - q.z;
    assert(std::sqrt(dx * dx + dz * dz) < LANE_WIDTH + RAIL_GAUGE); // 1º vértice perto de s = 980
}

// Raio mínimo: a borda interna do terreno (TERRAIN_HALF_WIDTH) não pode
// passar do centro da curva (a faixa se dobraria sobre si mesma), com folga.
void Test_CurveRadiusClearsTerrain()
{
    assert(TRACK_CURVE_RADIUS > TERRAIN_HALF_WIDTH + 10.0f);
    // E na pista gerada: no meio de qualquer curva, a borda interna do
    // terreno fica a R - TERRAIN_HALF_WIDTH do centro, do lado de dentro.
    Track t(4);
    t.EnsureGeneratedUntil(3000.0f);
    const std::vector<TrackSegment>& segs = t.Segments();
    int curves = 0;
    for (size_t i = 0; i < segs.size(); i++) {
        const TrackSegment& g = segs[i];
        if (g.curvature == 0.0f)
            continue;
        curves++;
        float R = 1.0f / std::fabs(g.curvature);
        float side = g.curvature > 0.0f ? -1.0f : 1.0f; // lado de dentro (centro)
        TrackFrame st = t.FrameAt(g.s_start);
        vec4 center = st.position + st.right * (side * R);
        TrackFrame mid = t.FrameAt(g.s_start + 0.5f * g.length);
        vec4 inner = mid.position + mid.right * (side * TERRAIN_HALF_WIDTH);
        vec4 d = inner - center;
        assert(nearf(std::sqrt(d.x * d.x + d.z * d.z), R - TERRAIN_HALF_WIDTH, 1e-2f));
    }
    assert(curves > 5);
}

// Prédios do lado de dentro da curva ficam mais juntos (raio menor); mesmo
// assim dois prédios vizinhos do mesmo lado não se encostam: centros a mais
// que a diagonal do quadrado BUILDING_WIDTH x BUILDING_WIDTH (condição
// suficiente para dois quadrados quaisquer não se cruzarem).
void Test_InnerCurveBuildingsDontOverlap()
{
    Track t(6);
    t.EnsureGeneratedUntil(3000.0f);
    std::vector<BuildingInstance> bs;
    float min_dist = 1e9f;
    for (float ps = 0.0f; ps < 2800.0f; ps += 150.0f) {
        ComputeBuildings(t, ps, &bs);
        for (size_t i = 0; i < bs.size(); i++)
            for (size_t j = i + 1; j < bs.size(); j++) {
                if (bs[i].x != bs[j].x)
                    continue;
                TrackFrame fi = t.FrameAt(bs[i].s), fj = t.FrameAt(bs[j].s);
                vec4 ci = fi.position + fi.right * bs[i].x;
                vec4 cj = fj.position + fj.right * bs[j].x;
                vec4 d = ci - cj;
                min_dist = std::min(min_dist, std::sqrt(d.x * d.x + d.z * d.z));
            }
    }
    assert(min_dist > BUILDING_WIDTH * std::sqrt(2.0f));
    assert(min_dist < BUILDING_SPACING - 0.5f); // houve prédio do lado de dentro de curva
}

// ------------------------------------------------- Luzes pontuais (3.1)

void Test_LampsCoverVisibleRange()
{
    std::vector<LampInstance> lamps;
    ComputeLamps(StraightTrack(), 100.0f, &lamps);
    assert(!lamps.empty());
    int left = 0, right = 0;
    for (size_t i = 0; i < lamps.size(); i++) {
        assert(lamps[i].s >= 100.0f - DESPAWN_BEHIND && lamps[i].s <= 100.0f + SPAWN_AHEAD);
        assert(nearf(std::fmod(lamps[i].s, LAMP_SPACING), 0.0f));
        assert(nearf(std::fabs(lamps[i].x), LAMP_OFFSET_X));
        // Entre o leito da pista e os prédios (nunca dentro de um prédio).
        assert(std::fabs(lamps[i].x) > TRACK_BED_HALF_WIDTH);
        assert(std::fabs(lamps[i].x) + 0.5f * LAMP_HEAD_SIZE < BUILDING_OFFSET_X - 0.5f * BUILDING_WIDTH);
        if (lamps[i].x < 0) left++; else right++;
    }
    assert(left == right);
}

void Test_PickNearestPoints()
{
    using glm::vec3;
    std::vector<vec3> pts;
    pts.push_back(vec3(10, 0, 0)); // 0: dist 10
    pts.push_back(vec3(1, 0, 0));  // 1: dist 1
    pts.push_back(vec3(0, 5, 0));  // 2: dist 5
    pts.push_back(vec3(0, 0, -3)); // 3: dist 3
    pts.push_back(vec3(-2, 0, 0)); // 4: dist 2
    std::vector<int> idx;
    std::vector<PointDistance> scratch;

    PickNearestPoints(pts, vec3(0, 0, 0), 3, &scratch, &idx);
    assert(idx.size() == 3);
    assert(idx[0] == 1 && idx[1] == 4 && idx[2] == 3);

    // A origem de referência importa (distância em 3D, não só em s).
    PickNearestPoints(pts, vec3(10, 0, 0), 2, &scratch, &idx);
    assert(idx.size() == 2 && idx[0] == 0 && idx[1] == 1);

    // Menos pontos que o máximo: devolve todos, sem repetir, e limpa a saída anterior.
    PickNearestPoints(pts, vec3(0, 0, 0), 8, &scratch, &idx);
    assert(idx.size() == 5);
    assert(idx[4] == 0);
    std::vector<int> seen(5, 0);
    for (size_t i = 0; i < idx.size(); i++) seen[idx[i]]++;
    for (int i = 0; i < 5; i++) assert(seen[i] == 1);

    // Vazio e max_count <= 0.
    PickNearestPoints(std::vector<vec3>(), vec3(0, 0, 0), 8, &scratch, &idx);
    assert(idx.empty());
    PickNearestPoints(pts, vec3(0, 0, 0), 0, &scratch, &idx);
    assert(idx.empty());

    // Empate de distância: estável (menor índice primeiro).
    std::vector<vec3> tie;
    tie.push_back(vec3(2, 0, 0));
    tie.push_back(vec3(-2, 0, 0));
    tie.push_back(vec3(0, 2, 0));
    PickNearestPoints(tie, vec3(0, 0, 0), 2, &scratch, &idx);
    assert(idx.size() == 2 && idx[0] == 0 && idx[1] == 1);
}

void Test_PickNearestLampsAroundPlayer()
{
    // Com os postes reais, as MAX_POINT_LIGHTS luzes escolhidas ficam todas
    // perto do jogador, em ordem crescente de distância.
    std::vector<LampInstance> lamps;
    const float ps = 100.0f;
    ComputeLamps(StraightTrack(), ps, &lamps);
    std::vector<glm::vec3> pos;
    for (size_t i = 0; i < lamps.size(); i++)
        pos.push_back(glm::vec3(lamps[i].x, LAMP_HEIGHT, -lamps[i].s)); // pista reta: s = -Z
    std::vector<int> idx;
    std::vector<PointDistance> scratch;
    PickNearestPoints(pos, glm::vec3(0, 0, -ps), MAX_POINT_LIGHTS, &scratch, &idx);
    // O buffer de trabalho é reaproveitado: a 2ª chamada não realoca.
    const PointDistance* before = scratch.data();
    PickNearestPoints(pos, glm::vec3(0, 0, -ps), MAX_POINT_LIGHTS, &scratch, &idx);
    assert(scratch.data() == before);
    assert((int)idx.size() == MAX_POINT_LIGHTS);
    float prev = 0.0f;
    for (size_t i = 0; i < idx.size(); i++) {
        glm::vec3 d = pos[idx[i]] - glm::vec3(0, 0, -ps);
        float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        assert(dist >= prev);
        assert(dist < 0.5f * (DESPAWN_BEHIND + SPAWN_AHEAD));
        prev = dist;
    }
}

// ------------------------------------------- Cenário dos túneis (4.2)

// Pista com um túnel de 60 m em [*ts, *te]: largada, reta de 50 m, túnel,
// reta de 300 m.
static Track TunnelTrack(float* ts, float* te)
{
    Track t(1);
    t.AppendSegment(50.0f, 0.0f, false);
    *ts = t.Segments().back().s_start + 50.0f;
    t.AppendSegment(60.0f, 0.0f, true);
    *te = *ts + 60.0f;
    t.AppendSegment(300.0f, 0.0f, false);
    return t;
}

// Prédios e postes de rua não aparecem ao longo do túnel nem colados nos
// portais (o poste em x = ±5.5 atravessaria a parede; o prédio, o portal),
// mas continuam antes e depois dele.
void Test_SceneryAvoidsTunnels()
{
    float ts, te;
    Track t = TunnelTrack(&ts, &te);
    const float lo = ts - TUNNEL_PORTAL_DEPTH - TUNNEL_CLEAR_MARGIN;
    const float hi = te + TUNNEL_PORTAL_DEPTH + TUNNEL_CLEAR_MARGIN;
    std::vector<BuildingInstance> bs;
    std::vector<LampInstance> ls;
    int before = 0, after = 0, lamps_before = 0, lamps_after = 0;
    for (float ps = 0.0f; ps <= te + 100.0f; ps += 5.0f) {
        ComputeBuildings(t, ps, &bs);
        for (size_t i = 0; i < bs.size(); i++) {
            float b0 = bs[i].s - 0.5f * BUILDING_WIDTH, b1 = bs[i].s + 0.5f * BUILDING_WIDTH;
            assert(b1 < lo || b0 > hi);
            if (b1 < lo && b1 > lo - BUILDING_SPACING) before++;
            if (b0 > hi && b0 < hi + BUILDING_SPACING) after++;
        }
        ComputeLamps(t, ps, &ls);
        for (size_t i = 0; i < ls.size(); i++) {
            assert(ls[i].s < lo || ls[i].s > hi);
            if (ls[i].s < lo && ls[i].s > lo - LAMP_SPACING) lamps_before++;
            if (ls[i].s > hi && ls[i].s < hi + LAMP_SPACING) lamps_after++;
        }
    }
    // Logo antes e logo depois do túnel o cenário continua.
    assert(before > 0 && after > 0 && lamps_before > 0 && lamps_after > 0);
}

// Módulos do túnel (TUNNEL_SECTION_LENGTH cada) cobrem o segmento de ponta a
// ponta, sem sobra nem buraco; um portal antes da entrada e outro depois da
// saída; lâmpadas de teto a cada TUNNEL_LAMP_SPACING, todas dentro do túnel.
void Test_TunnelPiecesAndLamps()
{
    float ts, te;
    Track t = TunnelTrack(&ts, &te);
    std::vector<TunnelPiece> pieces;
    ComputeTunnelPieces(t, ts + 10.0f, &pieces); // túnel inteiro no trecho visível
    int sections = 0, entries = 0, exits = 0;
    float smin = 1e9f, smax = -1e9f;
    for (size_t i = 0; i < pieces.size(); i++) {
        const TunnelPiece& p = pieces[i];
        if (p.kind == TUNNEL_PIECE_SECTION) {
            sections++;
            smin = std::min(smin, p.s);
            smax = std::max(smax, p.s);
            float k = (p.s - ts - 0.5f * TUNNEL_SECTION_LENGTH) / TUNNEL_SECTION_LENGTH;
            assert(nearf(k, std::floor(k + 0.5f), 1e-3f));
        } else if (p.kind == TUNNEL_PIECE_ENTRY) {
            entries++;
            assert(nearf(p.s, ts - 0.5f * TUNNEL_PORTAL_DEPTH, 1e-3f));
        } else {
            assert(p.kind == TUNNEL_PIECE_EXIT);
            exits++;
            assert(nearf(p.s, te + 0.5f * TUNNEL_PORTAL_DEPTH, 1e-3f));
        }
    }
    assert(sections == (int)(60.0f / TUNNEL_SECTION_LENGTH + 0.5f));
    assert(entries == 1 && exits == 1);
    assert(nearf(smin - 0.5f * TUNNEL_SECTION_LENGTH, ts, 1e-3f));
    assert(nearf(smax + 0.5f * TUNNEL_SECTION_LENGTH, te, 1e-3f));

    std::vector<float> lamps;
    ComputeTunnelLamps(t, ts + 10.0f, &lamps);
    assert(lamps.size() == 6); // 5, 15, ..., 55 m depois da entrada
    for (size_t i = 0; i < lamps.size(); i++) {
        assert(lamps[i] > ts && lamps[i] < te);
        assert(nearf(lamps[i], ts + 0.5f * TUNNEL_LAMP_SPACING + i * TUNNEL_LAMP_SPACING, 1e-3f));
    }

    // Só o que cruza o trecho visível: longe antes do túnel, nada; com o
    // horizonte no meio do túnel, só o começo dele.
    ComputeTunnelPieces(t, ts - SPAWN_AHEAD - 10.0f, &pieces);
    ComputeTunnelLamps(t, ts - SPAWN_AHEAD - 10.0f, &lamps);
    assert(pieces.empty() && lamps.empty());
    const float ps = ts + 30.0f - SPAWN_AHEAD;
    ComputeTunnelPieces(t, ps, &pieces);
    assert(!pieces.empty());
    for (size_t i = 0; i < pieces.size(); i++)
        assert(pieces[i].s <= ps + SPAWN_AHEAD + TUNNEL_SECTION_LENGTH && pieces[i].kind != TUNNEL_PIECE_EXIT);
}

// Regra R9: o vão interno do túnel comporta as 3 faixas, os trens (3.2 m de
// altura, inclusive nas faixas externas), a câmera (dentro e durante a
// transição) e as lâmpadas do teto; o portal é maior que o tubo.
void Test_TunnelClearance()
{
    const float train_w = OBSTACLE_SIZE[(int)ObstacleType::TRAIN][0];
    const float train_h = OBSTACLE_SIZE[(int)ObstacleType::TRAIN][1];
    const float x_edge = LANE_WIDTH + 0.5f * train_w; // lateral do trem na faixa externa
    assert(TunnelCeilingHeight(0.0f) >= 5.5f);
    assert(TunnelCeilingHeight(x_edge) > train_h + 0.5f);
    assert(TUNNEL_INNER_HALF_WIDTH > x_edge + 0.5f);          // parede longe do trem
    assert(TUNNEL_INNER_HALF_WIDTH > TRACK_BED_HALF_WIDTH);   // leito inteiro dentro
    // Teto monotônico: mais baixo junto às paredes, nunca acima do centro.
    float prev = TunnelCeilingHeight(0.0f);
    for (float x = 0.25f; x <= TUNNEL_INNER_HALF_WIDTH; x += 0.25f) {
        float h = TunnelCeilingHeight(x);
        assert(h <= prev + 1e-4f && h >= TUNNEL_SPRING_HEIGHT - 1e-4f);
        assert(nearf(TunnelCeilingHeight(-x), h));
        prev = h;
    }
    // Câmera (desloca no máximo 1/3 da faixa externa): nem a altura de fora
    // (durante a transição) chega perto do teto.
    const float cam_x = LANE_WIDTH * CAMERA_LATERAL_FACTOR;
    assert(TunnelCeilingHeight(cam_x) > CAMERA_HEIGHT + 1.0f);
    assert(CAMERA_TUNNEL_HEIGHT < CAMERA_HEIGHT);
    // Lâmpadas do teto: abaixo do teto e bem acima dos trens.
    assert(TUNNEL_LAMP_HEIGHT < TunnelCeilingHeight(0.0f));
    assert(TUNNEL_LAMP_HEIGHT > train_h + 1.0f);
    // Portal envolve o tubo (cobre a borda da parede).
    assert(TUNNEL_PORTAL_HALF_WIDTH > TUNNEL_INNER_HALF_WIDTH + TUNNEL_WALL_THICKNESS);
    assert(TUNNEL_PORTAL_HEIGHT > TunnelCeilingHeight(0.0f) + TUNNEL_WALL_THICKNESS);
    // Os módulos têm o mesmo comprimento que a "grade" do gerador de túneis.
    assert(TUNNEL_SECTION_LENGTH == TRACK_TUNNEL_SECTION);
}

// Escurecimento do sol com a câmera dentro do túnel: rampa linear de
// TUNNEL_DIM_TIME s nos dois sentidos, sem saltos (cada passo muda no máximo
// dt / TUNNEL_DIM_TIME), presa em [0, 1] e independente do dt.
void Test_TunnelDimmingSmooth()
{
    float d = 0.0f;
    const float dt = 1.0f / 60.0f;
    float t = 0.0f;
    while (d < 1.0f) {
        float nd = StepTunnelDimming(d, true, dt);
        assert(nd >= d && nd - d <= dt / TUNNEL_DIM_TIME + 1e-5f);
        d = nd;
        t += dt;
        assert(t < TUNNEL_DIM_TIME + 2.0f * dt);
    }
    assert(t > TUNNEL_DIM_TIME - 2.0f * dt);
    assert(StepTunnelDimming(1.0f, true, 1.0f) == 1.0f);
    // Saída: volta a 0 pelo mesmo caminho.
    float half = StepTunnelDimming(1.0f, false, 0.5f * TUNNEL_DIM_TIME);
    assert(nearf(half, 0.5f, 1e-4f));
    assert(StepTunnelDimming(half, false, 10.0f) == 0.0f);
    // 30 Hz e 120 Hz chegam ao mesmo valor.
    float a = 0.0f, b = 0.0f;
    for (int i = 0; i < 9; i++) a = StepTunnelDimming(a, true, 1.0f / 30.0f);
    for (int i = 0; i < 36; i++) b = StepTunnelDimming(b, true, 1.0f / 120.0f);
    assert(nearf(a, b, 1e-4f));
    // dt negativo não anda para trás.
    assert(StepTunnelDimming(0.3f, true, -1.0f) == 0.3f);
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

// Fonte única das medidas: as extensões padrão do Game são OBSTACLE_SIZE / 2
// (nenhuma é cortada pelos limites de faixa/chunk, senão o modelo desenhado
// na medida da tabela ficaria maior que a caixa de colisão), e a caixa de cada
// tipo tem exatamente essas medidas.
void Test_ObstacleSizeTableIsSourceOfTruth()
{
    Game g = QuietGame();
    for (int t = 0; t < OBSTACLE_TYPE_COUNT; t++) {
        glm::vec3 h = g.ObstacleExtents((ObstacleType)t);
        assert(nearf(2.0f * h.x, OBSTACLE_SIZE[t][0]));
        assert(nearf(2.0f * h.y, OBSTACLE_SIZE[t][1]));
        assert(nearf(2.0f * h.z, OBSTACLE_SIZE[t][2]));
        Obstacle o = g.MakeObstacle((ObstacleType)t, 1, 100.0f);
        assert(nearf(o.box.max.x - o.box.min.x, OBSTACLE_SIZE[t][0]));
        assert(nearf(o.box.max.y - o.box.min.y, OBSTACLE_SIZE[t][1]));
        assert(nearf(o.box.max.z - o.box.min.z, OBSTACLE_SIZE[t][2]));
    }
    // Barreira alta: vão livre de 1.1 m e topo fora do alcance do pulo.
    Obstacle hb = g.MakeObstacle(ObstacleType::BARRIER_HIGH, 0, 50.0f);
    assert(nearf(hb.box.min.y, BARRIER_HIGH_CLEARANCE));
    assert(hb.box.max.y > JUMP_VELOCITY * JUMP_VELOCITY / (2.0f * GRAVITY)); // pés não passam do topo
    assert(PLAYER_ROLL_HEIGHT < BARRIER_HIGH_CLEARANCE);
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

// ------------------------------------------- Obstáculos e curvas (4.1)

// Seeds 1..20: todo TRAIN/RAMP está inteiro (de box.min.z a box.max.z) em
// segmentos retos. Barreiras, placas e moedas podem cair nas curvas (e caem).
void Test_NoTrainOnCurve()
{
    int trains = 0, in_curve = 0;
    for (unsigned seed = 1; seed <= 20; seed++) {
        Game g(seed);
        g.GenerateChunksUntil(300 * CHUNK_LENGTH);
        const std::vector<TrackSegment>& segs = g.track.Segments();
        for (size_t k = 0; k < g.obstacles.size(); k++) {
            const Obstacle& o = g.obstacles[k];
            const TrackSegment* a = g.track.SegmentAt(o.box.min.z);
            const TrackSegment* b = g.track.SegmentAt(o.box.max.z);
            assert(a != nullptr && b != nullptr);
            if (o.type == ObstacleType::TRAIN || o.type == ObstacleType::RAMP) {
                trains++;
                // Todos os segmentos de a até b (inclusive) são retos.
                for (const TrackSegment* q = a; q <= b; q++)
                    assert(q->curvature == 0.0f);
                assert(a >= &segs.front() && b <= &segs.back());
            } else if (a->curvature != 0.0f) {
                in_curve++;
            }
        }
    }
    assert(trains > 500);   // ainda há muitos trens (nas retas)
    assert(in_curve > 100); // e há obstáculos nas curvas
}

// Trocando de faixa no meio de uma curva, o jogador anda em arcos
// concêntricos: a distância ao centro da curva é R + x (curva à esquerda, o
// centro fica do lado -right) ou R - x (à direita), com x sempre em
// [-LANE_WIDTH, LANE_WIDTH].
void Test_LanesConcentricInCurve()
{
    for (int dir = -1; dir <= 1; dir += 2) {
        Game g(1);
        g.spawn_enabled = false;
        g.Reset();
        // Pista feita à mão: reta inicial + curva de 90° + reta.
        g.track = Track(1);
        const float s0 = g.track.Segments()[0].length;
        const float k = dir / TRACK_CURVE_RADIUS;
        const float L = 0.5f * kPi * TRACK_CURVE_RADIUS;
        g.track.AppendSegment(L, k, false);
        g.track.AppendSegment(200.0f, 0.0f, false);
        TrackFrame st = g.track.FrameAt(s0);
        vec4 center = st.position - st.right * (1.0f / k);

        // Corre até entrar na curva.
        while (g.player.s < s0 + 5.0f) g.Update(1.0f / 60.0f);
        g.HandleInput(Input::RIGHT);
        int steps_in_curve = 0;
        bool changed_back = false;
        while (g.player.s < s0 + L - 1.0f) {
            g.Update(1.0f / 60.0f);
            if (!changed_back && g.player.s > s0 + 0.5f * L) {
                g.HandleInput(Input::LEFT);
                g.HandleInput(Input::LEFT);
                changed_back = true;
            }
            const Player& p = g.player;
            assert(g.mode == GameMode::PLAYING);
            assert(p.x >= -LANE_WIDTH - 1e-4f && p.x <= LANE_WIDTH + 1e-4f);
            TrackFrame f = g.track.FrameAt(p.s);
            vec4 w = f.position + f.right * p.x;
            vec4 d = w - center;
            float expected = 1.0f / std::fabs(k) + (k > 0.0f ? p.x : -p.x);
            assert(nearf(std::sqrt(d.x * d.x + d.z * d.z), expected, 2e-2f));
            steps_in_curve++;
        }
        assert(steps_in_curve > 60);
        assert(g.player.lane == -1 && nearf(g.player.x, -LANE_WIDTH)); // trocou mesmo
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
    Test_Curve90EndsPerpendicular();
    Test_TrackGeneratorRules();
    Test_CurveContinuity();
    Test_TunnelOnlyOnStraight();
    Test_IsTunnelAtBounds();
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
    Test_CameraFollowsCurve();
    Test_SceneryFitsCapacity();
    Test_RailsAboveGroundAndInsideBed();
    Test_BuildingsCoverVisibleRange();
    Test_SleepersUnderRailsFitCapacity();
    Test_StripsSnapToGrid();
    Test_CurveRadiusClearsTerrain();
    Test_InnerCurveBuildingsDontOverlap();
    // Luzes pontuais (3.1)
    Test_LampsCoverVisibleRange();
    Test_PickNearestPoints();
    Test_PickNearestLampsAroundPlayer();
    // Túneis (4.2)
    Test_SceneryAvoidsTunnels();
    Test_TunnelPiecesAndLamps();
    Test_TunnelClearance();
    Test_TunnelDimmingSmooth();
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
    Test_ObstacleSizeTableIsSourceOfTruth();
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
    Test_NoTrainOnCurve();
    Test_LanesConcentricInCurve();
    printf("OK\n");
    return 0;
}
