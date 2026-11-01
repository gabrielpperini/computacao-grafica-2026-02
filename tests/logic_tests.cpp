// Testes unitários dos módulos de lógica (sem OpenGL/GLFW). Cada módulo novo
// adiciona suas funções Test_*() e as chama em main().
// Os asserts precisam estar sempre ativos, mesmo em builds com NDEBUG.
#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include "bezier.h"
#include "camera.h"
#include "collisions.h"
#include "game.h"
#include "game_config.h"
#include "particles.h"
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

    // Valores (Tarefa 7.1a): numa pista montada à mão com duas retas a mais, a
    // extrapolação além do último segmento e antes do primeiro mantido segue a
    // reta do segmento (posição = início + forward * distância, heading igual).
    Track u(1);
    u.AppendSegment(50.0f, 0.0f, false);
    u.AppendSegment(60.0f, 0.0f, false);
    const TrackSegment last = u.Segments().back();
    const float end = last.s_start + last.length;
    const float hl = last.start_heading;
    const vec4 fwd(-std::sin(hl), 0.0f, -std::cos(hl), 0.0f);
    const float far_s[3] = {end + 0.5f, end + 40.0f, 1e6f};
    for (int i = 0; i < 3; ++i) {
        TrackFrame e = u.FrameAt(far_s[i]);
        vec4 expect = last.start_position + fwd * (far_s[i] - last.s_start);
        assert(near(e.position, expect, 1e-2f + 1e-6f * far_s[i]));
        assert(near(e.forward, fwd) && nearf(e.heading, hl));
    }
    u.DiscardBefore(last.s_start + 1.0f); // fica só o último segmento (reta)
    assert(u.Segments().size() == 1);
    TrackFrame b = u.FrameAt(last.s_start - 5.0f);
    assert(near(b.position, last.start_position - fwd * 5.0f, 1e-3f));
    assert(nearf(b.heading, hl));
    // A extrapolação é contínua com o interior do segmento.
    assert(near(u.FrameAt(last.s_start).position, last.start_position));
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

// dt inválido (Tarefa 7.1a): NaN, infinito e negativo nunca contaminam o estado.
void Test_DtSanitized()
{
    const float nan = std::nanf("");
    const float inf = INFINITY;
    assert(ClampFrameDt(nan) == 0.0f);
    assert(ClampFrameDt(-1.0f) == 0.0f);
    assert(ClampFrameDt(-inf) == 0.0f);
    assert(ClampFrameDt(0.0f) == 0.0f);
    assert(ClampFrameDt(0.01f) == 0.01f);
    assert(ClampFrameDt(5.0f) == MAX_DT);
    assert(ClampFrameDt(inf) == MAX_DT);

    Game g(1);
    g.spawn_enabled = false;
    g.Update(0.016f);
    const Player p = g.player;
    const float elapsed = g.elapsed, score = g.score;
    g.Update(nan);
    g.Update(-inf);
    g.Update(-0.5f);
    assert(g.player.s == p.s && g.player.x == p.x && g.player.y == p.y && g.player.speed == p.speed);
    assert(g.elapsed == elapsed && g.score == score);
    assert(std::isfinite(g.player.s) && std::isfinite(g.elapsed));
    g.Update(inf); // infinito vale MAX_DT
    assert(std::isfinite(g.player.s) && g.player.s > p.s && g.player.s - p.s < PLAYER_MAX_SPEED * MAX_DT + 1e-3f);

    // Com NaN no meio de uma corrida com spawn, a pista e o resto continuam finitos.
    Game h(3);
    for (int i = 0; i < 120; ++i)
        h.Update(i % 7 == 0 ? nan : 0.016f);
    assert(std::isfinite(h.player.s) && std::isfinite(h.score) && std::isfinite(h.player.y));
}

void Test_TrackIgnoresNonFiniteS()
{
    Track t(1);
    t.EnsureGeneratedUntil(100);
    const size_t n = t.Segments().size();
    t.EnsureGeneratedUntil(std::nanf(""));  // não gera nada (nem trava)
    t.EnsureGeneratedUntil(INFINITY);       // idem: sem laço infinito
    assert(t.Segments().size() == n);
    t.EnsureGeneratedUntil(300);            // continua funcionando depois
    assert(t.Segments().size() > n);

    // Game: um s não finito do jogador também não trava o gerador do spawn.
    Game g(1);
    g.GenerateChunksUntil(std::nanf(""));
    g.GenerateChunksUntil(INFINITY);
    assert(g.obstacles.empty());
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
    // dt = 0 (pausa, Tarefa 7.1a) e dt inválido congelam a rampa.
    assert(StepTunnelDimming(0.5f, true, 0.0f) == 0.5f);
    assert(StepTunnelDimming(0.5f, false, 0.0f) == 0.5f);
    assert(StepTunnelDimming(0.5f, true, -1.0f) == 0.5f);
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

// Caixa varrida (Tarefa 7.1a): a união da caixa antes e depois do passo.
void Test_SweepAABB()
{
    AABB before = Box(0, 0, 0, 1, 1, 1);
    AABB after  = Box(2, 0.5f, -1, 3, 1.5f, 0);
    AABB s = SweepAABB(before, after);
    assert(nearf(s.min.x, 0) && nearf(s.min.y, 0) && nearf(s.min.z, -1));
    assert(nearf(s.max.x, 3) && nearf(s.max.y, 1.5f) && nearf(s.max.z, 1));
    // Simétrica e idempotente.
    AABB r = SweepAABB(after, before);
    assert(nearf(r.min.z, s.min.z) && nearf(r.max.x, s.max.x));
    AABB same = SweepAABB(before, before);
    assert(nearf(same.min.x, 0) && nearf(same.max.z, 1));
    // Intersecta tudo o que qualquer uma das duas caixas intersecta.
    assert(AABBIntersectsAABB(s, Box(0.2f, 0.2f, 0.2f, 0.4f, 0.4f, 0.4f)));
    assert(AABBIntersectsAABB(s, Box(2.2f, 0.7f, -0.8f, 2.4f, 0.9f, -0.6f)));
}

void Test_SweepAABBAlongZ()
{
    // Obstáculo que andou dz < 0 neste passo (trem vindo): a caixa varrida vai
    // da posição antiga (max.z - dz) à atual.
    AABB now = Box(-1, 0, 10, 1, 3, 23);
    AABB s = SweepAABBAlongZ(now, -1.9f);
    assert(nearf(s.min.z, 10) && nearf(s.max.z, 23 + 1.9f));
    assert(nearf(s.min.x, -1) && nearf(s.max.x, 1) && nearf(s.min.y, 0) && nearf(s.max.y, 3));
    // dz > 0: a posição antiga ficava mais atrás (min.z - dz).
    s = SweepAABBAlongZ(now, 2.0f);
    assert(nearf(s.min.z, 10 - 2.0f) && nearf(s.max.z, 23));
    // Parado: a própria caixa.
    s = SweepAABBAlongZ(now, 0.0f);
    assert(nearf(s.min.z, 10) && nearf(s.max.z, 23));
    // Pega o jogador que o trem atravessou num passo: caixa fina "atrás" da
    // posição atual do trem, na altura do jogador.
    assert(AABBIntersectsAABB(SweepAABBAlongZ(now, -1.9f), Box(-0.4f, 0, 23.5f, 0.4f, 1.8f, 24.5f)));
    assert(!AABBIntersectsAABB(now, Box(-0.4f, 0, 23.5f, 0.4f, 1.8f, 24.5f)));
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

// ------------------------------------------------ Rampas e tetos de trem (4.4)

// Rampa sob a "pegada" (projeção xz) do jogador: a altura é a do ponto mais
// alto da rampa sob a pegada, isto é, na frente dela (a rampa sobe com s).
void Test_RampHeightUnderFootprint()
{
    AABB ramp = Box(-1.1f, 0, 10, 1.1f, 3.2f, 18); // sobe 0.4 m por metro
    float h = -1.0f;
    // Pegada [13.6, 14.4]: altura na frente (s = 14.4).
    assert(RampHeightUnder(ramp, Box(-0.4f, 0, 13.6f, 0.4f, 1.8f, 14.4f), &h));
    assert(nearf(h, 0.4f * 4.4f));
    // Frente já além do fim da rampa: altura máxima (3.2).
    assert(RampHeightUnder(ramp, Box(-0.4f, 0, 17.5f, 0.4f, 1.8f, 18.3f), &h));
    assert(nearf(h, 3.2f));
    // Só a ponta de trás da pegada antes do começo: conta a frente.
    assert(RampHeightUnder(ramp, Box(-0.4f, 0, 9.5f, 0.4f, 1.8f, 10.3f), &h));
    assert(nearf(h, 0.4f * 0.3f));
    // Pegada só encostando de lado (troca de faixa): ainda está sobre a rampa.
    assert(RampHeightUnder(ramp, Box(1.1f, 0, 13.6f, 1.9f, 1.8f, 14.4f), &h));
    // Fora em x ou em s: false e *out_height intacto.
    h = -7.0f;
    assert(!RampHeightUnder(ramp, Box(1.2f, 0, 13.6f, 2.0f, 1.8f, 14.4f), &h));
    assert(!RampHeightUnder(ramp, Box(-0.4f, 0, 18.1f, 0.4f, 1.8f, 18.9f), &h));
    assert(!RampHeightUnder(ramp, Box(-0.4f, 0, 9.0f, 0.4f, 1.8f, 9.8f), &h));
    assert(h == -7.0f);
}

// Queda rápida (dt grande / rolamento no ar): num único quadro a base passa
// de 0.2 m acima do teto para 0.85 m abaixo dele. Ainda é pouso, não batida.
void Test_LandsOnTopBigFall()
{
    AABB train = Box(-1.1f, 0, 10, 1.1f, 3.2f, 23);
    AABB prev  = Box(-0.4f, 3.4f, 15, 0.4f, 5.2f, 15.8f);
    AABB now   = Box(-0.4f, 2.35f, 16.5f, 0.4f, 4.15f, 17.3f);
    assert(LandsOnTop(prev, now, train, 0.25f));
}

// Limites da tolerância (intervalos fechados: na fronteira conta).
void Test_LandsOnTopToleranceBoundary()
{
    AABB train = Box(-1.1f, 0, 10, 1.1f, 3.0f, 23); // teto em 3.0 (exato em float)
    const float tol = 0.25f;
    // Base anterior exatamente em teto - tol (sobre o trem nos dois quadros).
    assert(LandsOnTop(Box(-0.4f, 2.75f, 15, 0.4f, 4.55f, 15.8f),
                      Box(-0.4f, 2.75f, 15.5f, 0.4f, 4.55f, 16.3f), train, tol));
    // Um pouco abaixo disso: batida (não sobe no teto).
    assert(!LandsOnTop(Box(-0.4f, 2.7f, 15, 0.4f, 4.5f, 15.8f),
                       Box(-0.4f, 2.7f, 15.5f, 0.4f, 4.5f, 16.3f), train, tol));
    // Base atual exatamente em teto + tol: pousou.
    assert(LandsOnTop(Box(-0.4f, 3.5f, 15, 0.4f, 5.3f, 15.8f),
                      Box(-0.4f, 3.25f, 15.5f, 0.4f, 5.05f, 16.3f), train, tol));
    // Ainda acima de teto + tol: ainda no ar.
    assert(!LandsOnTop(Box(-0.4f, 3.5f, 15, 0.4f, 5.3f, 15.8f),
                       Box(-0.4f, 3.375f, 15.5f, 0.4f, 5.175f, 16.3f), train, tol));
}

// Entrando na pegada do trem NESTE quadro, vale a altura da base no instante
// em que a frente (ou a lateral) do jogador cruza a borda do trem (interpolada
// entre os dois quadros). Subindo pela rampa colada à frente do trem, a base
// chega ao teto exatamente ali: é pouso, mesmo que no quadro anterior ela
// estivesse mais baixa (a rampa sobe 0.6 m por passo a 30 m/s com dt = 0.05).
void Test_LandsOnTopClimbingIn()
{
    AABB train = Box(-1.1f, 0, 20, 1.1f, 3.2f, 33);
    // Rampa: base 2.9 com a frente 0.75 m antes do trem; agora 3.2 e 0.75 m dentro.
    AABB prev = Box(-0.4f, 2.9f, 18.45f, 0.4f, 4.7f, 19.25f);
    AABB now  = Box(-0.4f, 3.2f, 19.95f, 0.4f, 5.0f, 20.75f);
    assert(LandsOnTop(prev, now, train, 0.25f));
    // Lateral: troca de faixa de um teto para o teto vizinho (mesma altura).
    AABB side_prev = Box(-2.9f, 3.2f, 25, -2.1f, 5.0f, 25.8f);
    AABB side_now  = Box(-1.5f, 3.2f, 25.5f, -0.7f, 5.0f, 26.3f);
    assert(LandsOnTop(side_prev, side_now, train, 0.25f));
    // A mesma troca de faixa a partir do chão: batida na lateral.
    AABB low_prev = Box(-2.9f, 0, 25, -2.1f, 1.8f, 25.8f);
    AABB low_now  = Box(-1.5f, 0, 25.5f, -0.7f, 1.8f, 26.3f);
    assert(!LandsOnTop(low_prev, low_now, train, 0.25f));
}

// Pulando contra a frente do trem: entra na pegada dele com a base abaixo do
// teto (no quadro anterior e no cruzamento): batida. Já quem vinha de cima
// (base anterior acima do teto) e cruza a borda da frente caindo pousa,
// mesmo que no quadro atual a base tenha passado um pouco do teto.
void Test_LandsOnTopEntryBelowRoof()
{
    AABB train = Box(-1.1f, 0, 20, 1.1f, 3.2f, 33);
    // Base anterior bem abaixo do teto e entrando: batida frontal.
    assert(!LandsOnTop(Box(-0.4f, 1.0f, 18.5f, 0.4f, 2.8f, 19.3f),
                       Box(-0.4f, 1.2f, 19.5f, 0.4f, 3.0f, 20.3f), train, 0.25f));
    // Base anterior acima do teto, entrando um pouco abaixo dele: pousa.
    assert(LandsOnTop(Box(-0.4f, 3.4f, 18.5f, 0.4f, 5.2f, 19.3f),
                      Box(-0.4f, 3.0f, 19.5f, 0.4f, 4.8f, 20.3f), train, 0.25f));
}

// Geometria do bloqueio lateral da rampa (degrau máximo 0.4 m).
void Test_RampSideBlocks()
{
    AABB ramp = Box(1.4f, 0, 10, 3.6f, 3.2f, 18); // sobe 0.4 m por metro em s
    float x = 0.0f;
    // Entrando pela esquerda, pegada em s = [14, 14.8] (altura 1.92 > 0.4).
    assert(RampSideBlocks(ramp, Box(0.4f, 0, 14, 1.2f, 1.8f, 14.8f),
                          Box(0.8f, 0, 14, 1.6f, 1.8f, 14.8f), 0.4f, &x));
    assert(x + 0.4f < 1.4f && x > 0.99f); // pegada para logo antes da lateral
    // No começo baixo (altura 0.32 <= 0.4): é só um degrau, passa.
    assert(!RampSideBlocks(ramp, Box(0.4f, 0, 9.2f, 1.2f, 1.8f, 10.8f),
                           Box(0.8f, 0, 9.2f, 1.6f, 1.8f, 10.8f), 0.4f, &x));
    // Já no alto (pulo): degrau pequeno, passa.
    assert(!RampSideBlocks(ramp, Box(0.4f, 1.6f, 14, 1.2f, 3.4f, 14.8f),
                           Box(0.8f, 1.6f, 14, 1.6f, 3.4f, 14.8f), 0.4f, &x));
    // Entrada pela frente (já na faixa x da rampa): não é lateral.
    assert(!RampSideBlocks(ramp, Box(2.1f, 0, 11.0f, 2.9f, 1.8f, 11.8f),
                           Box(2.1f, 0, 11.0f, 2.9f, 1.8f, 11.8f), 0.4f, &x));
    // Pela direita: para do lado direito.
    AABB ramp2 = Box(-1.1f, 0, 10, 1.1f, 3.2f, 18);
    assert(RampSideBlocks(ramp2, Box(1.2f, 0, 14, 2.0f, 1.8f, 14.8f),
                          Box(0.9f, 0, 14, 1.7f, 1.8f, 14.8f), 0.4f, &x));
    assert(x - 0.4f > 1.1f && x < 1.51f);
    // Longe da rampa: nada.
    assert(!RampSideBlocks(ramp, Box(-0.4f, 0, 14, 0.4f, 1.8f, 14.8f),
                           Box(-0.2f, 0, 14, 0.6f, 1.8f, 14.8f), 0.4f, &x));
}

// Teto (do túnel): quanto a caixa passou dele.
void Test_CeilingPenetration()
{
    AABB b = Box(-0.4f, 4.0f, 0, 0.4f, 5.8f, 0.8f);
    assert(nearf(CeilingPenetration(b, 6.0f), 0.0f));
    assert(nearf(CeilingPenetration(b, 5.8f), 0.0f)); // encostado não penetra
    assert(nearf(CeilingPenetration(b, 5.5f), 0.3f));
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

// Avança 1/60 s por quadro enquanto 'cond()' for verdadeira, com no máximo
// max_steps quadros (Tarefa 7.1a): se o comportamento esperado regredir, o teste
// falha com assert em vez de travar para sempre.
template <typename Cond>
static void RunWhile(Game& g, Cond cond, int max_steps = 60 * 120)
{
    int steps = 0;
    while (cond()) {
        assert(steps++ < max_steps && "RunWhile: condição nunca deixou de valer");
        g.Update(1.0f / 60.0f);
    }
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
            if (g.obstacles[k].type == ObstacleType::RAMP) {
                // Rampa (Tarefa 4.4): colada à frente de um trem, entra na
                // emenda livre do começo do chunk, mas nunca mata (troca de
                // faixa por cima dela continua possível) e não sai do chunk.
                float c = std::floor(b.max.z / CHUNK_LENGTH) * CHUNK_LENGTH;
                assert(b.min.z >= c - 1e-3f);
                assert(b.max.z <= c + CHUNK_LENGTH - CHUNK_CLEAR_TAIL + 1e-3f);
                continue;
            }
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
// no chão (ou, na barreira alta, a partir do vão). As regras das rampas estão
// em Test_RampSpawnRules (4.4).
void Test_ObstaclesInsideLane()
{
    Game g(3);
    g.GenerateChunksUntil(300 * CHUNK_LENGTH);
    for (size_t k = 0; k < g.obstacles.size(); k++) {
        const Obstacle& o = g.obstacles[k];
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

// A caixa do jogador acompanha o modelo (Tarefa 7.1a): o personagem tem ~1.07 m
// de largura com os braços (tronco 0.53 m) e ~0.53 m de profundidade (cabeça)
// depois da escala para PLAYER_HEIGHT; a caixa não pode morder além disso, ou
// o jogador morre antes de o modelo encostar no trem.
void Test_PlayerBoxMatchesModel()
{
    assert(PLAYER_HALF_DEPTH >= 0.26f && PLAYER_HALF_DEPTH <= 0.30f);
    assert(PLAYER_HALF_WIDTH >= 0.26f && PLAYER_HALF_WIDTH <= 0.54f);

    // Trem parado de frente: a morte acontece quando o NARIZ da caixa chega à
    // frente do trem (no máximo um passo depois), não 13 cm antes.
    Game g = QuietGame();
    Obstacle train = g.MakeObstacle(ObstacleType::TRAIN, 0, 26.5f);
    g.obstacles.push_back(train);
    const float dt = 1.0f / 240.0f;
    for (int i = 0; i < 2400 && g.mode == GameMode::PLAYING; ++i)
        g.Update(dt);
    assert(g.mode == GameMode::GAME_OVER);
    const float nose = g.player.s + PLAYER_HALF_DEPTH;
    assert(nose >= train.box.min.z - 1e-3f && nose <= train.box.min.z + 0.06f);
}

// O passo que vira GAME_OVER não coleta nada nem soma pontos (Tarefa 7.1a): a
// moeda e o power-up que a caixa varrida tocaria no mesmo passo da batida, e a
// distância desse passo, ficam de fora.
void Test_GameOverFrameNoCollectNoScore()
{
    Game g = QuietGame();
    g.player.s = 5.0f;
    g.obstacles.push_back(g.MakeObstacle(ObstacleType::BARRIER_LOW, 0, 6.0f)); // min.z = 5.85
    Coin c = {0, 6.3f, COIN_HEIGHT, false};   // dentro da caixa varrida, atrás da barreira
    g.coins.push_back(c);
    PowerUp p = {PowerUpType::MULTIPLIER, 0, 6.3f, false};
    g.powerups.push_back(p);
    const float score = g.score;
    g.Update(MAX_DT); // nariz de 5.27 até ~5.87: bate (5.85) e alcançaria a moeda (5.8)
    assert(g.mode == GameMode::GAME_OVER);
    assert(g.coins_collected == 0 && !g.coins[0].collected);
    assert(!g.powerups[0].collected && !g.PowerUpActive(PowerUpType::MULTIPLIER));
    assert(g.score == score);
    assert(g.events.empty());

    // Sanidade: sem a barreira, o mesmo passo coleta a moeda e soma distância.
    Game h = QuietGame();
    h.player.s = 5.0f;
    h.coins.push_back(c);
    h.Update(MAX_DT);
    assert(h.mode == GameMode::PLAYING && h.coins_collected == 1 && h.score > 10.0f);
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
        assert(parts[i].max.x < -PLAYER_HALF_WIDTH || parts[i].min.x > PLAYER_HALF_WIDTH);
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
    AABB before = g.PlayerBox(); // z em [-0.27, 0.27]
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

// ------------------------------------------------------ Trens em movimento (4.3)

// Jogo sem spawn automático e com uma reta longa (600 m) logo depois da reta
// da largada, para posicionar trens à mão em qualquer s até ~700 m.
static Game StraightGame()
{
    Game g = QuietGame();
    g.track = Track(1);
    g.track.AppendSegment(600.0f, 0.0f, false);
    return g;
}

void Test_StraightRunStart()
{
    Track t(1);
    const float L0 = t.Segments()[0].length;
    t.AppendSegment(60.0f, 1.0f / TRACK_CURVE_RADIUS, false);  // curva
    t.AppendSegment(80.0f, 0.0f, false);                       // reta
    t.AppendSegment(40.0f, 0.0f, true);                        // túnel (também reto)
    t.AppendSegment(70.0f, 0.0f, false);                       // reta
    const float a = L0 + 60.0f;
    assert(nearf(t.StraightRunStart(10.0f), 0.0f));
    assert(nearf(t.StraightRunStart(a + 1.0f), a));
    assert(nearf(t.StraightRunStart(a + 80.0f + 10.0f), a));   // atravessa reta + túnel
    assert(nearf(t.StraightRunStart(a + 120.0f + 10.0f), a));
    assert(nearf(t.StraightRunStart(L0 + 30.0f), L0 + 30.0f)); // em curva: o próprio s
}

void Test_MovingTrainAdvances()
{
    Game g = StraightGame();
    Obstacle t = g.MakeMovingTrain(1, 200.0f, -MOVING_TRAIN_SPEED);
    assert(nearf(t.velocity, -MOVING_TRAIN_SPEED));
    assert(t.min_s <= t.box.min.z);
    g.obstacles.push_back(t);
    Run(g, 1.0f);
    const Obstacle& o = g.obstacles[0];
    assert(nearf(o.s, 200.0f - MOVING_TRAIN_SPEED, 1e-3f));
    // A caixa acompanha o s (centro e comprimento preservados).
    assert(nearf(0.5f * (o.box.min.z + o.box.max.z), o.s, 1e-3f));
    assert(nearf(o.box.max.z - o.box.min.z, OBSTACLE_SIZE[(int)ObstacleType::TRAIN][2], 1e-3f));
    assert(g.mode == GameMode::PLAYING);

    // O deslocamento depende do tempo, não do número de quadros.
    Game h = StraightGame();
    h.obstacles.push_back(h.MakeMovingTrain(1, 200.0f, -MOVING_TRAIN_SPEED));
    Run(h, 1.0f, 1.0f / 20.0f);
    assert(nearf(h.obstacles[0].s, o.s, 1e-3f));

    // dt gigante vale no máximo MAX_DT (o trem também).
    Game k = StraightGame();
    k.obstacles.push_back(k.MakeMovingTrain(1, 200.0f, -MOVING_TRAIN_SPEED));
    k.Update(10.0f);
    assert(nearf(k.obstacles[0].s, 200.0f - MOVING_TRAIN_SPEED * MAX_DT, 1e-4f));

    // Trem parado (velocity 0) não anda.
    Game z = StraightGame();
    z.obstacles.push_back(z.MakeObstacle(ObstacleType::TRAIN, 1, 200.0f));
    Run(z, 1.0f);
    assert(nearf(z.obstacles[0].s, 200.0f));
}

// O trem para quando o seu nariz chegaria ao começo da reta onde nasceu (aqui
// há uma curva logo antes) e fica parado lá.
void Test_MovingTrainStopsAtSegmentEnd()
{
    Game g = QuietGame();
    g.track = Track(1);
    const float L0 = g.track.Segments()[0].length;
    g.track.AppendSegment(60.0f, 1.0f / TRACK_CURVE_RADIUS, false);
    g.track.AppendSegment(120.0f, 0.0f, false);
    const float run_start = L0 + 60.0f;
    Obstacle t = g.MakeMovingTrain(1, run_start + 100.0f, -MOVING_TRAIN_SPEED);
    assert(t.velocity < 0.0f);
    assert(nearf(t.min_s, run_start));
    g.obstacles.push_back(t);
    bool stopped = false;
    for (int i = 0; i < 60 * 20 && !stopped; i++) {
        g.Update(1.0f / 60.0f);
        assert(g.mode == GameMode::PLAYING);
        assert(g.obstacles[0].box.min.z >= run_start - 1e-3f); // nunca entra na curva
        stopped = g.obstacles[0].velocity == 0.0f;
    }
    assert(stopped);
    const Obstacle& o = g.obstacles[0];
    assert(nearf(o.box.min.z, run_start, 1e-3f));
    assert(nearf(0.5f * (o.box.min.z + o.box.max.z), o.s, 1e-3f));
    float s_stop = o.s;
    Run(g, 0.5f); // continua parado
    assert(nearf(g.obstacles[0].s, s_stop) && g.obstacles[0].velocity == 0.0f);

    // Um trem que não cabe inteiro numa reta não nasce móvel (fica parado).
    Obstacle bad = g.MakeMovingTrain(1, L0 + 20.0f, -MOVING_TRAIN_SPEED); // em cima da curva
    assert(bad.velocity == 0.0f);
}

// Um obstáculo à frente na mesma faixa também faz o trem parar (sem atravessá-lo).
void Test_MovingTrainStopsBehindObstacle()
{
    Game g = StraightGame();
    g.obstacles.push_back(g.MakeObstacle(ObstacleType::SIGN, 1, 300.0f));
    g.obstacles.push_back(g.MakeMovingTrain(1, 400.0f, -MOVING_TRAIN_SPEED));
    // Um segundo trem móvel logo atrás do primeiro (mesma faixa): para atrás dele.
    g.obstacles.push_back(g.MakeMovingTrain(1, 430.0f, -MOVING_TRAIN_SPEED));
    // Outra faixa: não é afetada pela placa.
    g.obstacles.push_back(g.MakeMovingTrain(-1, 400.0f, -MOVING_TRAIN_SPEED));
    for (int i = 0; i < 60 * 14; i++)
        g.Update(1.0f / 60.0f);
    assert(g.mode == GameMode::PLAYING);
    const Obstacle& sign = g.obstacles[0];
    const Obstacle& a = g.obstacles[1];
    const Obstacle& b = g.obstacles[2];
    const Obstacle& c = g.obstacles[3];
    assert(a.velocity == 0.0f && b.velocity == 0.0f);
    assert(a.box.min.z >= sign.box.max.z);
    assert(b.box.min.z >= a.box.max.z);
    assert(nearf(a.box.min.z, sign.box.max.z + TRAIN_STOP_GAP, 1e-3f));
    assert(nearf(b.box.min.z, a.box.max.z + TRAIN_STOP_GAP, 1e-3f));
    assert(c.velocity < 0.0f); // a outra faixa segue em frente
}

// Contato com um trem móvel: sem desvio -> GAME_OVER no instante esperado;
// desviando de faixa antes do contato -> segue vivo.
void Test_MovingTrainHitAndDodge()
{
    Game hit = StraightGame();
    hit.obstacles.push_back(hit.MakeMovingTrain(0, 200.0f, -MOVING_TRAIN_SPEED));
    for (int i = 0; i < 60 * 20 && hit.mode == GameMode::PLAYING; i++)
        hit.Update(1.0f / 60.0f);
    assert(hit.mode == GameMode::GAME_OVER);
    // 12 t + 0.075 t^2 + 0.4 = (200 - 6.5) - 8 t  ->  t ~ 9.3 s
    assert(hit.elapsed > 9.0f && hit.elapsed < 9.6f);

    Game dodge = StraightGame();
    dodge.obstacles.push_back(dodge.MakeMovingTrain(0, 200.0f, -MOVING_TRAIN_SPEED));
    Run(dodge, 5.0f); // bem antes do contato (3 s de aviso)
    assert(dodge.mode == GameMode::PLAYING);
    dodge.HandleInput(Input::RIGHT);
    Run(dodge, 8.0f);
    assert(dodge.mode == GameMode::PLAYING);
    assert(dodge.player.s > 100.0f);
}

// Sem "tunneling": mesmo com velocidade de fechamento alta e dt máximo, um
// obstáculo fino não atravessa o jogador entre dois quadros.
void Test_MovingTrainNoTunneling()
{
    // Caso do enunciado: jogador a 30 m/s, trem a -8 m/s, dt = 0.05 s, trem
    // fino (0.3 m) que cruza a caixa do jogador entre dois quadros.
    {
        Game g = QuietGame();
        g.SetObstacleExtents(ObstacleType::TRAIN, glm::vec3(1.1f, 1.6f, 0.15f));
        g.player.speed = PLAYER_MAX_SPEED;
        Obstacle t = g.MakeObstacle(ObstacleType::TRAIN, 0, 0.7f); // z em [0.55, 0.85]
        t.velocity = -MOVING_TRAIN_SPEED;
        t.min_s = -1e9f;
        g.obstacles.push_back(t);
        AABB before = g.PlayerBox(); // z em [-0.27, 0.27]
        assert(before.max.z < t.box.min.z);
        g.Update(MAX_DT);            // jogador a [1.23, 1.77]; trem a [0.15, 0.45]
        assert(g.PlayerBox().min.z > g.obstacles[0].box.max.z); // nenhum dos dois toca o outro
        assert(g.mode == GameMode::GAME_OVER);
    }
    // Obstáculo muito mais rápido que o jogador: só a caixa varrida DO
    // OBSTÁCULO (antes/depois) detecta o cruzamento.
    {
        Game g = QuietGame();
        g.SetObstacleExtents(ObstacleType::TRAIN, glm::vec3(1.1f, 1.6f, 0.15f));
        Obstacle t = g.MakeObstacle(ObstacleType::TRAIN, 0, 3.15f); // z em [3.0, 3.3]
        t.velocity = -100.0f;                                      // 5 m por passo
        t.min_s = -1e9f;
        g.obstacles.push_back(t);
        g.Update(MAX_DT);            // trem a [-2.0, -1.7]; jogador varreu [-0.27, 0.87]
        assert(g.obstacles[0].box.max.z < g.PlayerBox().min.z);
        assert(g.mode == GameMode::GAME_OVER);
    }
}

// Piloto fantasma: segue a faixa segura de cada chunk (a faixa muda na emenda
// livre entre chunks) e ignora barreiras.
static int AutopilotLane(const Game& g)
{
    int c = (int)std::floor(g.player.s / CHUNK_LENGTH);
    float end = (c + 1) * CHUNK_LENGTH;
    return g.SafeLaneOfChunk(g.player.s >= end - CHUNK_CLEAR_TAIL ? c + 1 : c);
}

// Bateu em placa/trem? (barreira não conta: o piloto não pula nem rola.)
static bool HitSolid(const Game& g)
{
    const AABB p = g.PlayerBox();
    for (size_t k = 0; k < g.obstacles.size(); k++) {
        const Obstacle& o = g.obstacles[k];
        if (!IsBlocking(o.type))
            continue;
        // Folga de 2 m em s (passo do jogador + do trem) e 0.5 m em x (troca de faixa).
        if (p.max.z + 2.0f >= o.box.min.z && p.min.z - 2.0f <= o.box.max.z &&
            p.max.x + 0.5f >= o.box.min.x && p.min.x - 0.5f <= o.box.max.x)
            return true;
    }
    return false;
}

// Propriedades dos trens móveis gerados de verdade (Update + spawn por
// chunks), em várias sementes: regras de spawn, e o piloto que segue a faixa
// segura nunca bate em placa nem em trem (parado ou móvel).
void Test_MovingTrainsSpawnAndStayPassable()
{
    int moving_total = 0, trains_total = 0;
    for (unsigned seed = 1; seed <= 30; seed++) {
        Game g(seed);
        int prev_chunk = -1;
        while (g.player.s < 5000.0f) {
            g.player.lane = AutopilotLane(g);
            g.Update(1.0f / 60.0f);
            if (g.mode == GameMode::GAME_OVER) {
                assert(!HitSolid(g)); // só pode ter sido barreira
                g.mode = GameMode::PLAYING;
            }
            int top = prev_chunk;
            for (size_t k = 0; k < g.obstacles.size(); k++) {
                const Obstacle& o = g.obstacles[k];
                int c = (int)std::floor(o.s / CHUNK_LENGTH);
                if (c > prev_chunk && o.type == ObstacleType::TRAIN) {
                    // Recém-gerado neste quadro.
                    trains_total++;
                    if (o.velocity < 0.0f) {
                        moving_total++;
                        // Aviso: visível por >= MOVING_TRAIN_WARNING segundos.
                        assert(o.s - g.player.s >=
                               (g.player.speed + MOVING_TRAIN_SPEED) * MOVING_TRAIN_WARNING - 1e-3f);
                        assert(nearf(o.velocity, -MOVING_TRAIN_SPEED));
                        assert(g.SafeLaneOfChunk(c) != o.lane); // faixa não segura
                        assert(o.min_s <= o.box.min.z + 1e-3f);
                        assert(g.track.IsStraightRange(o.min_s, o.box.max.z));
                    } else {
                        assert(o.velocity == 0.0f);
                    }
                }
                top = std::max(top, c);
            }
            prev_chunk = top;
        }
    }
    printf("  [trens moveis] %d de %d trens gerados\n", moving_total, trains_total);
    assert(moving_total >= 30); // a funcionalidade aparece de fato
    assert(moving_total < trains_total);
}

// Dois obstáculos da mesma faixa nunca se sobrepõem ao longo do tempo (Tarefa
// 7.1a), nem com os trens móveis andando e parando uns contra os outros e
// contra as rampas: só podem se tocar (rampa colada ao trem, folga zero).
void Test_ObstaclesSameLaneNeverOverlap()
{
    int moving_seen = 0;
    for (unsigned seed = 1; seed <= 30; seed++) {
        Game g(seed);
        while (g.player.s < 2500.0f) {
            g.player.lane = AutopilotLane(g);
            g.Update(1.0f / 60.0f);
            g.mode = GameMode::PLAYING; // bater em barreira não encerra o teste
            for (size_t i = 0; i < g.obstacles.size(); i++) {
                const Obstacle& a = g.obstacles[i];
                if (a.velocity < 0.0f) moving_seen++;
                for (size_t j = i + 1; j < g.obstacles.size(); j++) {
                    const Obstacle& b = g.obstacles[j];
                    if (a.lane != b.lane) continue;
                    const float overlap = std::min(a.box.max.z, b.box.max.z) -
                                          std::max(a.box.min.z, b.box.min.z);
                    assert(overlap <= 1e-3f);
                }
            }
        }
    }
    assert(moving_seen > 0); // o cenário de fato teve trens móveis andando
}

// Mesma semente -> mesmos trens móveis (decisão determinística).
void Test_MovingTrainsDeterministic()
{
    auto run = [](unsigned seed) {
        Game g(seed);
        std::vector<float> out;
        while (g.player.s < 1500.0f) {
            g.player.lane = AutopilotLane(g);
            g.Update(1.0f / 60.0f);
            g.mode = GameMode::PLAYING;
            for (size_t k = 0; k < g.obstacles.size(); k++)
                if (g.obstacles[k].velocity < 0.0f)
                    out.push_back(g.obstacles[k].s);
        }
        return out;
    };
    std::vector<float> a = run(7), b = run(7);
    assert(!a.empty() && a == b);
}

// --------------------------------------------- Rampas e andar sobre trens (4.4)

static const float kRoof = OBSTACLE_SIZE[(int)ObstacleType::TRAIN][1];

// Trem parado centrado em train_s na faixa, com a rampa colada à frente dele
// se with_ramp (a rampa entra antes do trem: ordem de s).
static void AddTrain(Game& g, int lane, float train_s, bool with_ramp)
{
    Obstacle t = g.MakeObstacle(ObstacleType::TRAIN, lane, train_s);
    if (with_ramp)
        g.obstacles.push_back(g.MakeRampFor(t));
    g.obstacles.push_back(t);
}

// Avança em passos de dt até o jogador passar de s (ou o jogo acabar).
static void RunUntilS(Game& g, float s, float dt = 1.0f / 60.0f)
{
    while (g.player.s < s && g.mode == GameMode::PLAYING)
        g.Update(dt);
}

// Roda até o jogador voltar ao chão/teto (no máximo 'seconds'); devolve a
// maior altura no caminho. Falha se o jogo acabar.
static float RunUntilGrounded(Game& g, float seconds = 5.0f, float dt = 1.0f / 60.0f)
{
    float max_y = g.player.y;
    for (int i = 0; i < (int)(seconds / dt) && !g.player.grounded; i++) {
        g.Update(dt);
        assert(g.mode == GameMode::PLAYING);
        max_y = std::max(max_y, g.player.y);
    }
    assert(g.player.grounded);
    return max_y;
}

void Test_RampGeometry()
{
    // A rampa sobe exatamente até o teto do trem (mesma altura na tabela).
    assert(OBSTACLE_SIZE[(int)ObstacleType::RAMP][1] == OBSTACLE_SIZE[(int)ObstacleType::TRAIN][1]);
    Game g = StraightGame();
    Obstacle t = g.MakeObstacle(ObstacleType::TRAIN, 1, 40.0f);
    Obstacle r = g.MakeRampFor(t);
    assert(r.type == ObstacleType::RAMP && r.lane == 1 && r.velocity == 0.0f);
    assert(nearf(r.box.max.z, t.box.min.z)); // colada à frente do trem
    assert(nearf(r.box.max.z - r.box.min.z, OBSTACLE_SIZE[(int)ObstacleType::RAMP][2]));
    assert(nearf(r.box.min.y, 0.0f) && nearf(r.box.max.y, t.box.max.y));
    assert(nearf(r.s, 0.5f * (r.box.min.z + r.box.max.z)));
    assert(nearf(0.5f * (r.box.min.x + r.box.max.x), 0.5f * (t.box.min.x + t.box.max.x)));
    assert(r.box.min.x >= t.box.min.x - kEps && r.box.max.x <= t.box.max.x + kEps);
}

// Correndo pela faixa da rampa, sem tocar em nada: os pés seguem a superfície
// da rampa (nem flutuam nem afundam) e o jogador termina em cima do trem.
void Test_RampLiftsPlayerOntoTrain()
{
    Game g = StraightGame();
    AddTrain(g, 0, 40.0f, true); // rampa em [25.5, 33.5], trem em [33.5, 46.5]
    const AABB ramp = g.obstacles[0].box, train = g.obstacles[1].box;
    bool climbed = false;
    while (g.player.s < 42.0f) {
        g.Update(1.0f / 60.0f);
        assert(g.mode == GameMode::PLAYING);
        AABB p = g.PlayerBox();
        float h;
        if (p.max.z < train.min.z && RampHeightUnder(ramp, p, &h)) {
            assert(g.player.grounded && nearf(g.player.y, h, 1e-3f));
            climbed = climbed || h > 2.0f;
        }
    }
    assert(climbed);
    assert(g.player.grounded && nearf(g.player.y, kRoof) && g.player.vy == 0.0f);
    assert(nearf(g.GroundHeight(), kRoof));
}

// Pulo a partir do meio da rampa: passa por cima da borda da frente do trem e
// pousa no meio do teto (não morre, não cai pela rampa).
void Test_JumpOntoTrainRoof()
{
    Game g = StraightGame();
    AddTrain(g, 0, 40.0f, true);
    const AABB train = g.obstacles[1].box;
    RunUntilS(g, 30.0f);
    assert(g.mode == GameMode::PLAYING && g.player.grounded && g.player.y > 1.0f);
    g.HandleInput(Input::JUMP);
    assert(!g.player.grounded);
    float max_y = RunUntilGrounded(g);
    assert(max_y > kRoof + 0.3f);
    assert(nearf(g.player.y, kRoof) && g.player.vy == 0.0f);
    assert(g.player.s > train.min.z + 2.0f && g.player.s < train.max.z - 2.0f);
}

// Passando do fim do trem a gravidade volta: cai, pousa no chão e continua.
void Test_FallOffTrainEnd()
{
    Game g = StraightGame();
    AddTrain(g, 0, 40.0f, true);
    const AABB train = g.obstacles[1].box;
    RunUntilS(g, train.max.z - 1.0f);
    assert(g.mode == GameMode::PLAYING && g.player.grounded && nearf(g.player.y, kRoof));
    RunUntilS(g, train.max.z + PLAYER_HALF_DEPTH + 0.5f); // pegada já fora do trem
    assert(g.mode == GameMode::PLAYING);
    assert(!g.player.grounded && g.player.y < kRoof && g.player.y > 0.0f && g.player.vy < 0.0f);
    Run(g, 1.0f);
    assert(g.mode == GameMode::PLAYING);
    assert(g.player.grounded && g.player.y == 0.0f && g.player.vy == 0.0f);
    assert(g.GroundHeight() == 0.0f);
}

// Só bater na frente/lateral do trem abaixo do teto mata.
void Test_FrontalTrainHitIsGameOver()
{
    // Trem sem rampa, correndo de frente.
    {
        Game g = StraightGame();
        AddTrain(g, 0, 40.0f, false);
        Run(g, 5.0f);
        assert(g.mode == GameMode::GAME_OVER && g.player.s < 33.5f);
    }
    // Pulando contra a frente: o pulo (~2 m) não alcança o teto (3.2 m).
    {
        Game g = StraightGame();
        AddTrain(g, 0, 40.0f, false);
        RunUntilS(g, 29.0f);
        g.HandleInput(Input::JUMP);
        Run(g, 2.0f);
        assert(g.mode == GameMode::GAME_OVER && g.player.s < 33.5f);
    }
    // Velocidade máxima, com o maior dt e com dt pequeno: bate, sem atravessar.
    {
        const float dts[2] = {MAX_DT, 1.0f / 240.0f};
        for (int d = 0; d < 2; d++) {
            Game g = StraightGame();
            AddTrain(g, 0, 40.0f, false);
            g.player.speed = PLAYER_MAX_SPEED;
            Run(g, 3.0f, dts[d]);
            assert(g.mode == GameMode::GAME_OVER);
            assert(g.player.s < 33.5f + PLAYER_MAX_SPEED * dts[d]);
            // E pulando contra a frente na mesma velocidade.
            Game j = StraightGame();
            AddTrain(j, 0, 40.0f, false);
            j.player.speed = PLAYER_MAX_SPEED;
            RunUntilS(j, 26.0f, dts[d]);
            j.HandleInput(Input::JUMP);
            Run(j, 2.0f, dts[d]);
            assert(j.mode == GameMode::GAME_OVER);
            assert(j.player.s < 33.5f + PLAYER_MAX_SPEED * dts[d]);
        }
    }
    // Rampa e trem na faixa vizinha: entrar pelo chão na lateral do trem mata.
    {
        Game g = StraightGame();
        AddTrain(g, 1, 40.0f, true);
        RunUntilS(g, 37.0f);
        assert(g.mode == GameMode::PLAYING);
        g.HandleInput(Input::RIGHT);
        Run(g, 1.0f);
        assert(g.mode == GameMode::GAME_OVER && g.player.y == 0.0f);
    }
}

// A rampa nunca mata e a lateral dela é parede: trocando de faixa pelo chão
// ao lado de um trecho alto da rampa, a troca é desfeita (volta para a faixa
// de onde veio, a pegada nunca entra na faixa x da rampa), sem mudar a altura
// e sem game over; depois o jogador segue ao lado do trem.
void Test_RampNeverKills()
{
    Game g = StraightGame();
    AddTrain(g, 1, 40.0f, true); // rampa em [25.5, 33.5] (altura ~1.4 m em s = 29)
    const AABB ramp = g.obstacles[0].box;
    RunUntilS(g, 28.0f);
    g.HandleInput(Input::RIGHT);
    float max_x = 0.0f;
    while (g.player.s < 50.0f) {
        g.Update(1.0f / 60.0f);
        assert(g.mode == GameMode::PLAYING);
        assert(g.player.y == 0.0f && g.player.grounded); // nenhum "teletransporte"
        assert(g.PlayerBox().max.x < ramp.min.x);        // nunca dentro da lateral
        max_x = std::max(max_x, g.player.x);
    }
    assert(max_x > 0.9f);                                 // tentou de fato
    assert(g.player.lane == 0 && g.player.x == 0.0f);     // troca desfeita
}

// Entrar de lado no começo da rampa (altura < RAMP_STEP_HEIGHT): é só um
// degrau; o jogador entra e sobe suavemente até o teto do trem.
void Test_RampSideLowStartClimbs()
{
    Game g = StraightGame();
    AddTrain(g, 1, 40.0f, true); // rampa em [25.5, 33.5], 0.4 m de altura por metro
    RunUntilS(g, 24.2f);          // a pegada chega à faixa x da rampa ~1 m depois
    g.HandleInput(Input::RIGHT);
    float prev_y = g.player.y, max_y = 0.0f;
    while (g.player.s < 40.0f) {
        g.Update(1.0f / 60.0f);
        assert(g.mode == GameMode::PLAYING);
        assert(std::fabs(g.player.y - prev_y) <= 0.25f); // suave: só o degrau de entrada e a subida
        prev_y = g.player.y;
        max_y = std::max(max_y, g.player.y);
    }
    assert(g.player.lane == 1 && nearf(g.player.x, LANE_WIDTH));
    assert(nearf(max_y, kRoof) && nearf(g.player.y, kRoof) && g.player.grounded);
}

// Entradas laterais em qualquer ponto da rampa (de 25 em 25 cm): em nenhum
// quadro a altura varia mais que RAMP_STEP_HEIGHT; ou entra no começo baixo e
// sobe, ou é barrado na lateral. Nunca morre.
void Test_RampSideEntryDeltaY()
{
    for (int k = 0; k <= 34; k++) {
        Game g = StraightGame();
        AddTrain(g, 1, 40.0f, true);
        RunUntilS(g, 23.5f + 0.25f * k); // entrada ~1 m depois: pegada ainda toca a rampa
        g.HandleInput(Input::RIGHT);
        float prev_y = g.player.y;
        for (int i = 0; i < 120; i++) {
            g.Update(1.0f / 60.0f);
            assert(g.mode == GameMode::PLAYING);
            assert(std::fabs(g.player.y - prev_y) <= RAMP_STEP_HEIGHT + 1e-3f);
            prev_y = g.player.y;
        }
    }
}

// Na emenda rampa/trem o resultado de uma entrada lateral pelo chão não
// depende da fase do quadro e nunca é "subir no teto": com o jogador posto a
// 1 cm da lateral e s varrido de 1 em 1 cm (traseira da pegada no fim do passo
// de 32.0 até ramp.max.z + passo + 0.5 m), em velocidades e dt variados:
//  - se durante o passo a pegada tocou a rampa em s (traseira no começo do
//    passo <= ramp.max.z): barrado (faixa desfeita, altura 0, sem game over,
//    e segue 1 s ao lado do trem sem morrer);
//  - senão: batida de lado no trem (game over), ainda no chão.
// Em nenhum caso a altura sobe mais que RAMP_STEP_HEIGHT no quadro.
void Test_RampTrainJunctionSideEntryConsistent()
{
    const float dts[3] = {1.0f / 240.0f, 1.0f / 60.0f, MAX_DT};
    const float speeds[2] = {PLAYER_START_SPEED, PLAYER_MAX_SPEED};
    for (int d = 0; d < 3; d++) {
        for (int v = 0; v < 2; v++) {
            const float step = speeds[v] * dts[d];
            int blocked = 0, hits = 0;
            const int n = (int)((1.5f + step + 0.5f) / 0.01f);
            for (int k = 0; k <= n; k++) {
                Game g = StraightGame();
                AddTrain(g, 1, 40.0f, true); // emenda em s = 33.5
                const AABB ramp = g.obstacles[0].box;
                g.player.speed = speeds[v];
                g.player.s = 32.0f + PLAYER_HALF_DEPTH - step + 0.01f * k;
                g.player.x = ramp.min.x - PLAYER_HALF_WIDTH - 0.01f;
                g.player.lane = 1;
                const AABB before = g.PlayerBox();
                if (std::fabs(before.min.z - ramp.max.z) < 2e-3f)
                    continue; // exatamente na fronteira (arredondamento de float)
                g.Update(dts[d]);
                assert(g.player.y <= RAMP_STEP_HEIGHT + 1e-3f); // nunca "teletransporta"
                if (before.min.z <= ramp.max.z) {
                    assert(g.mode == GameMode::PLAYING);
                    assert(g.player.lane == 0 && g.player.y == 0.0f);
                    assert(g.PlayerBox().max.x < ramp.min.x);
                    blocked++;
                    Run(g, 1.0f, dts[d]); // segue ao lado do trem, sem morrer
                    assert(g.mode == GameMode::PLAYING && g.player.x == 0.0f && g.player.y == 0.0f);
                } else {
                    assert(g.mode == GameMode::GAME_OVER && g.player.y == 0.0f);
                    hits++;
                }
            }
            assert(blocked > 100 && hits > 40);
        }
    }
}

// Pular do teto não é cancelado pelo teste de pouso (só vale com vy <= 0).
void Test_JumpFromRoofNotCancelled()
{
    Game g = StraightGame();
    AddTrain(g, 0, 40.0f, true);
    RunUntilS(g, 35.0f);
    assert(g.player.grounded && nearf(g.player.y, kRoof));
    g.HandleInput(Input::JUMP);
    g.Update(1.0f / 60.0f);
    assert(!g.player.grounded && g.player.y > kRoof && g.player.vy > 0.0f);
    float max_y = RunUntilGrounded(g);
    assert(max_y > kRoof + 1.5f);
    assert(nearf(g.player.y, kRoof)); // pousa de volta no mesmo trem
    // Pulando no teto, a "altura do chão" (câmera) continua sendo o teto.
    g.HandleInput(Input::JUMP);
    for (int i = 0; i < 10; i++) {
        g.Update(1.0f / 60.0f);
        assert(nearf(g.GroundHeight(), kRoof));
    }
}

// Trocar de faixa em cima do trem para uma faixa sem trem: cai no chão.
void Test_RoofLaneChangeToEmptyLaneFalls()
{
    Game g = StraightGame();
    AddTrain(g, 0, 40.0f, true);
    RunUntilS(g, 36.0f);
    assert(g.player.grounded && nearf(g.player.y, kRoof));
    g.HandleInput(Input::LEFT);
    Run(g, 0.15f); // x = -1.8: pegada [-2.2, -1.4] já fora do trem [-1.1, 1.1]
    assert(g.mode == GameMode::PLAYING && !g.player.grounded && g.player.y < kRoof);
    Run(g, 1.0f);
    assert(g.mode == GameMode::PLAYING && g.player.grounded && g.player.y == 0.0f);
    assert(g.player.lane == -1 && nearf(g.player.x, -LANE_WIDTH));
}

// ...e para uma faixa com outro trem ao lado (mesma altura): continua no teto
// o tempo todo (a pegada sempre toca um dos dois).
void Test_RoofLaneChangeToAdjacentTrainStays()
{
    Game g = StraightGame();
    AddTrain(g, 0, 40.0f, true);   // [33.5, 46.5]
    AddTrain(g, 1, 42.0f, false);  // [35.5, 48.5]
    RunUntilS(g, 36.0f);
    g.HandleInput(Input::RIGHT);
    while (g.player.s < 47.0f) {
        g.Update(1.0f / 60.0f);
        assert(g.mode == GameMode::PLAYING);
        assert(g.player.grounded && nearf(g.player.y, kRoof));
    }
    assert(g.player.lane == 1 && nearf(g.player.x, LANE_WIDTH));
    Run(g, 1.0f); // e cai no fim do segundo trem
    assert(g.mode == GameMode::PLAYING && g.player.grounded && g.player.y == 0.0f);
}

// Na velocidade máxima com o maior dt (1.5 m por passo), em todas as fases do
// passo (de 1 em 1 cm) em relação à emenda rampa/trem: sobe, percorre o trem
// inteiro com os pés no teto e cai no fim, sem nunca morrer. (Com 1.5 m por
// passo a pegada de 0.8 m pode sair inteira da rampa num único passo.)
void Test_RoofRunMaxSpeedBigDt()
{
    for (int k = 0; k < 150; k++) {
        Game g = StraightGame();
        g.player.speed = PLAYER_MAX_SPEED;
        g.player.s = 0.01f * k;
        AddTrain(g, 0, 60.0f, true);
        const AABB train = g.obstacles[1].box;
        float max_y = 0.0f;
        for (int i = 0; i < 60; i++) { // 3 s, ~90 m
            g.Update(MAX_DT);
            assert(g.mode == GameMode::PLAYING);
            max_y = std::max(max_y, g.player.y);
            AABB p = g.PlayerBox();
            if (p.min.z >= train.min.z && p.max.z <= train.max.z)
                assert(g.player.grounded && nearf(g.player.y, kRoof));
        }
        assert(nearf(max_y, kRoof));
        assert(g.player.grounded && g.player.y == 0.0f);
    }
}

// Dentro do túnel a cabeça do jogador nunca passa do teto (abóbada), mesmo
// com um pulo exagerado (tênis + teto de trem chegariam perto); ao bater, a
// velocidade vertical deixa de subir. Fora do túnel o mesmo pulo vai além.
void Test_TunnelCeilingClamp()
{
    Game base = QuietGame();
    base.track = Track(1);
    const float L0 = base.track.Segments()[0].length;
    base.track.AppendSegment(200.0f, 0.0f, true);
    for (int lane = 0; lane <= 1; lane++) {
        Game g = base;
        g.player.s = L0 + 40.0f;
        g.player.lane = lane;
        g.player.x = lane * LANE_WIDTH;
        g.player.grounded = false;
        g.player.vy = 25.0f; // ~10 m de altura sem teto
        bool hit = false;
        for (int i = 0; i < 300 && !(i > 0 && g.player.grounded); i++) {
            g.Update(1.0f / 60.0f);
            assert(g.mode == GameMode::PLAYING && g.track.IsTunnelAt(g.player.s));
            AABB p = g.PlayerBox();
            float ceiling = std::min(TunnelCeilingHeight(p.min.x), TunnelCeilingHeight(p.max.x));
            assert(p.max.y <= ceiling + 1e-4f);
            if (p.max.y > ceiling - 1e-3f) {
                hit = true;
                assert(g.player.vy <= 0.0f);
            }
        }
        assert(hit && g.player.grounded && g.player.y == 0.0f);
    }
    Game out = base;
    out.player.s = 10.0f; // reta da largada, fora do túnel
    out.player.grounded = false;
    out.player.vy = 25.0f;
    float max_y = RunUntilGrounded(out);
    assert(max_y + PLAYER_HEIGHT > 8.0f);
}

// Rampas geradas: sempre coladas à frente de um trem PARADO na mesma faixa
// (logo, nunca na faixa segura), em reta, fora de túneis e sem túnel até
// RAMP_TUNNEL_CLEARANCE depois do fim do trem, dentro do chunk do trem.
void Test_RampSpawnRules()
{
    int ramps = 0, static_trains = 0;
    for (unsigned seed = 1; seed <= 20; seed++) {
        Game g(seed);
        g.GenerateChunksUntil(500 * CHUNK_LENGTH);
        const std::vector<Obstacle>& o = g.obstacles;
        for (size_t i = 0; i < o.size(); i++)
            if (o[i].type == ObstacleType::TRAIN && o[i].velocity == 0.0f)
                static_trains++;
        for (size_t i = 0; i < o.size(); i++) {
            const Obstacle& r = o[i];
            if (r.type != ObstacleType::RAMP)
                continue;
            ramps++;
            const Obstacle* t = NULL;
            for (size_t j = 0; j < o.size(); j++)
                if (o[j].type == ObstacleType::TRAIN && o[j].lane == r.lane &&
                    nearf(o[j].box.min.z, r.box.max.z))
                    t = &o[j];
            assert(t != NULL && t->velocity == 0.0f);
            assert(nearf(r.box.max.y, t->box.max.y) && r.box.min.y == 0.0f);
            assert(nearf(0.5f * (r.box.min.x + r.box.max.x), r.lane * LANE_WIDTH));
            assert(g.track.IsStraightRange(r.box.min.z, t->box.max.z));
            assert(!g.track.TunnelOverlaps(r.box.min.z, t->box.max.z + RAMP_TUNNEL_CLEARANCE));
            const int c = (int)std::floor(t->box.min.z / CHUNK_LENGTH);
            assert(r.box.min.z >= c * CHUNK_LENGTH && r.box.min.z >= OBSTACLE_FREE_START);
            assert(g.SafeLaneOfChunk(c) != r.lane);
        }
    }
    printf("  [rampas] %d rampas para %d trens parados\n", ramps, static_trains);
    assert(ramps >= 100 && ramps < static_trains);
}

// As rampas têm gerador próprio: com ou sem rampas, os demais obstáculos
// (tipos, faixas, posições, trens móveis) saem idênticos; e a mesma semente
// gera as mesmas rampas.
void Test_RampsDontDisturbSpawn()
{
    for (unsigned seed = 1; seed <= 10; seed++) {
        Game a(seed), b(seed), c(seed);
        b.ramps_enabled = false;
        a.GenerateChunksUntil(300 * CHUNK_LENGTH);
        b.GenerateChunksUntil(300 * CHUNK_LENGTH);
        c.GenerateChunksUntil(300 * CHUNK_LENGTH);
        std::vector<const Obstacle*> rest;
        for (size_t i = 0; i < a.obstacles.size(); i++)
            if (a.obstacles[i].type != ObstacleType::RAMP)
                rest.push_back(&a.obstacles[i]);
        assert(rest.size() == b.obstacles.size() && rest.size() < a.obstacles.size());
        for (size_t i = 0; i < rest.size(); i++) {
            const Obstacle& x = *rest[i];
            const Obstacle& y = b.obstacles[i];
            assert(x.type == y.type && x.lane == y.lane && x.s == y.s && x.velocity == y.velocity);
        }
        assert(a.obstacles.size() == c.obstacles.size());
        for (size_t i = 0; i < a.obstacles.size(); i++)
            assert(a.obstacles[i].type == c.obstacles[i].type && a.obstacles[i].s == c.obstacles[i].s);
    }
}

// A altura do chão (usada pela câmera) ignora o arco do pulo: do chão é 0
// durante todo o pulo.
void Test_GroundHeightIgnoresJumpArc()
{
    Game g = StraightGame();
    g.HandleInput(Input::JUMP);
    for (int i = 0; i < 30; i++) {
        g.Update(1.0f / 60.0f);
        assert(g.player.y > 0.0f && g.GroundHeight() == 0.0f);
    }
}

// Câmera em cima do trem: sobe junto com a altura do chão (teto), de forma
// suave e independente de dt, e olha para o jogador lá em cima.
void Test_CameraFollowsGroundHeight()
{
    Track t = StraightTrack();
    ThirdPersonCamera cam;
    cam.Snap(t, 20.0f, 0.0f, false, kRoof);
    CameraPose p = cam.Pose();
    assert(nearf(p.position.y, CAMERA_HEIGHT + kRoof));
    vec4 look(0, CAMERA_LOOK_HEIGHT + kRoof, -20.0f - CAMERA_LOOK_AHEAD, 1);
    assert(near(p.view_vector, look - p.position));

    ThirdPersonCamera c;
    c.Snap(t, 20.0f, 0.0f, false, 0.0f);
    c.Update(t, 20.0f, 0.0f, false, 1.0f / 60.0f, kRoof);
    float prev = c.Pose().position.y;
    assert(prev > CAMERA_HEIGHT && prev < CAMERA_HEIGHT + 0.5f * kRoof); // não pula
    for (int i = 0; i < 180; i++) {
        c.Update(t, 20.0f, 0.0f, false, 1.0f / 60.0f, kRoof);
        float y = c.Pose().position.y;
        assert(y >= prev && y <= CAMERA_HEIGHT + kRoof + kEps);
        prev = y;
    }
    assert(nearf(prev, CAMERA_HEIGHT + kRoof, 1e-3f));

    ThirdPersonCamera a, b;
    a.Snap(t, 0.0f, 0.0f, false, 0.0f);
    b.Snap(t, 0.0f, 0.0f, false, 0.0f);
    for (int i = 1; i <= 15; i++) a.Update(t, 0.0f, 0.0f, false, 1.0f / 30.0f, kRoof);
    for (int i = 1; i <= 60; i++) b.Update(t, 0.0f, 0.0f, false, 1.0f / 120.0f, kRoof);
    assert(near(a.Pose().position, b.Pose().position, 1e-3f));
    assert(near(a.Pose().view_vector, b.Pose().view_vector, 1e-3f));
}

// ---------------------------------------------------------------------------
// Câmera livre (Tarefa 5.1)
// ---------------------------------------------------------------------------

static float Len4(const vec4& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

static CameraPose SomePose()
{
    CameraPose p;
    p.position = vec4(3.0f, 4.0f, 5.0f, 1.0f);
    p.view_vector = vec4(1.0f, -0.5f, -2.0f, 0.0f);
    p.up = vec4(0.0f, 1.0f, 0.0f, 0.0f);
    return p;
}

// StartAt reproduz a pose de origem (posição e direção do olhar), e Look com
// delta zero não muda nada.
void Test_FreeCamStartAtRoundTrip()
{
    CameraPose from = SomePose();
    FreeCamera cam;
    cam.StartAt(from);
    CameraPose p = cam.Pose();
    assert(near(p.position, from.position));
    vec4 expected = from.view_vector / Len4(from.view_vector);
    assert(nearf(Len4(p.view_vector), 1.0f, 1e-3f));
    assert(near(p.view_vector, expected, 1e-3f));
    assert(near(p.up, vec4(0.0f, 1.0f, 0.0f, 0.0f)));
    assert(nearf(p.position.w, 1.0f) && nearf(p.view_vector.w, 0.0f));

    cam.Look(0.0f, 0.0f);
    CameraPose q = cam.Pose();
    assert(near(q.position, p.position));
    assert(near(q.view_vector, p.view_vector, 1e-4f));

    // Olhar para o -Z do mundo (heading 0) e reto para cima também funcionam.
    CameraPose z = SomePose();
    z.view_vector = vec4(0.0f, 0.0f, -1.0f, 0.0f);
    cam.StartAt(z);
    assert(near(cam.Pose().view_vector, z.view_vector, 1e-4f));
    z.view_vector = vec4(0.0f, 5.0f, 0.0f, 0.0f); // vertical: não pode gerar NaN
    cam.StartAt(z);
    CameraPose v = cam.Pose();
    assert(v.view_vector.y > 0.99f && v.view_vector.y == v.view_vector.y);
}

// Pitch limitado a +-89 graus, por mais que o mouse corra.
void Test_FreeCamPitchClamp()
{
    FreeCamera cam;
    cam.StartAt(SomePose());
    const float lim = std::sin(89.0f * kPi / 180.0f);
    cam.Look(0.0f, -100000.0f); // mouse para cima: olha para cima
    assert(cam.Pose().view_vector.y <= lim + 1e-4f);
    assert(cam.Pose().view_vector.y >= lim - 1e-3f);
    cam.Look(0.0f, 100000.0f);  // mouse para baixo: olha para baixo
    assert(cam.Pose().view_vector.y >= -lim - 1e-4f);
    assert(cam.Pose().view_vector.y <= -lim + 1e-3f);
    assert(nearf(Len4(cam.Pose().view_vector), 1.0f, 1e-3f));

    // Yaw: mouse para a direita gira o olhar para a direita (right = (cos h, 0, -sin h)).
    CameraPose z = SomePose();
    z.view_vector = vec4(0.0f, 0.0f, -1.0f, 0.0f);
    cam.StartAt(z);
    cam.Look(100.0f, 0.0f);
    assert(cam.Pose().view_vector.x > 0.1f);
    cam.StartAt(z);
    cam.Look(0.0f, -100.0f);
    assert(cam.Pose().view_vector.y > 0.1f);
}

// Mover 1 s é a mesma distância com dt de 1/30 e de 1/120; 10 m/s; direções
// locais (direita, cima, frente).
void Test_FreeCamMoveDt()
{
    CameraPose z = SomePose();
    z.view_vector = vec4(0.0f, 0.0f, -1.0f, 0.0f);
    FreeCamera a, b;
    a.StartAt(z);
    b.StartAt(z);
    for (int i = 0; i < 30; i++) a.Move(glm::vec3(0.0f, 0.0f, 1.0f), 1.0f / 30.0f);
    for (int i = 0; i < 120; i++) b.Move(glm::vec3(0.0f, 0.0f, 1.0f), 1.0f / 120.0f);
    assert(near(a.Pose().position, b.Pose().position, 1e-3f));
    float d = Len4(a.Pose().position - z.position);
    assert(nearf(d, FREE_CAMERA_SPEED, 1e-3f));
    assert(nearf(a.Pose().position.z, z.position.z - FREE_CAMERA_SPEED, 1e-3f)); // para frente = -Z

    // Direita = +X quando olha para -Z; cima = +Y; recuar = oposto.
    FreeCamera c;
    c.StartAt(z);
    c.Move(glm::vec3(1.0f, 0.0f, 0.0f), 0.5f);
    assert(nearf(c.Pose().position.x, z.position.x + 0.5f * FREE_CAMERA_SPEED, 1e-3f));
    c.Move(glm::vec3(0.0f, 1.0f, 0.0f), 0.5f);
    assert(nearf(c.Pose().position.y, z.position.y + 0.5f * FREE_CAMERA_SPEED, 1e-3f));
    c.Move(glm::vec3(0.0f, 0.0f, -1.0f), 0.5f);
    assert(nearf(c.Pose().position.z, z.position.z + 0.5f * FREE_CAMERA_SPEED, 1e-3f));

    // Diagonal não é mais rápida (direção normalizada), e dir nulo / dt nulo ficam parados.
    FreeCamera e;
    e.StartAt(z);
    e.Move(glm::vec3(1.0f, 0.0f, 1.0f), 1.0f);
    assert(nearf(Len4(e.Pose().position - z.position), FREE_CAMERA_SPEED, 1e-3f));
    FreeCamera f;
    f.StartAt(z);
    f.Move(glm::vec3(0.0f), 1.0f);
    f.Move(glm::vec3(1.0f, 1.0f, 1.0f), 0.0f);
    assert(near(f.Pose().position, z.position));

    // "Voa": com o olhar inclinado, avançar inclui a componente vertical.
    FreeCamera g;
    g.StartAt(z);
    g.Look(0.0f, -200.0f); // sobe o olhar
    float y0 = g.Pose().position.y;
    g.Move(glm::vec3(0.0f, 0.0f, 1.0f), 1.0f);
    assert(g.Pose().position.y > y0 + 1.0f);
}

// ---------------------------------------------------------------------------
// Ponto dentro do túnel (câmera livre; collisions.cpp)
// ---------------------------------------------------------------------------

// Ponto de mundo a (s, lateral, altura) da pista.
static glm::vec3 TrackPoint(const Track& t, float s, float lat, float y)
{
    TrackFrame f = t.FrameAt(s);
    vec4 p = f.position + f.right * lat;
    return glm::vec3(p.x, p.y + y, p.z);
}

static void CheckTunnelVolume(const Track& t, float ts, float te)
{
    const float mid = 0.5f * (ts + te);
    const float R = TUNNEL_INNER_HALF_WIDTH;
    // Dentro: no eixo a meia altura, perto do chão, junto à parede (abaixo da abóbada).
    assert(PointInsideTunnel(t, TrackPoint(t, mid, 0.0f, 3.0f)));
    assert(PointInsideTunnel(t, TrackPoint(t, mid, 0.0f, 0.2f)));
    assert(PointInsideTunnel(t, TrackPoint(t, mid, 4.5f, 1.5f)));
    assert(PointInsideTunnel(t, TrackPoint(t, mid, -4.5f, 1.5f)));
    // Teto: o topo do eixo é TunnelCeilingHeight(0) = 7 m; 8 m (sobre o túnel) é fora.
    assert(PointInsideTunnel(t, TrackPoint(t, mid, 0.0f, TunnelCeilingHeight(0.0f) - 0.1f)));
    assert(!PointInsideTunnel(t, TrackPoint(t, mid, 0.0f, TunnelCeilingHeight(0.0f) + 0.1f)));
    assert(!PointInsideTunnel(t, TrackPoint(t, mid, 0.0f, 8.0f)));
    // Perto da parede, acima do perfil da abóbada (a 4.5 m o teto está a ~4.2 m): fora.
    assert(TunnelCeilingHeight(4.5f) < 5.0f);
    assert(!PointInsideTunnel(t, TrackPoint(t, mid, 4.5f, 5.0f)));
    assert(!PointInsideTunnel(t, TrackPoint(t, mid, -4.5f, 5.0f)));
    // Fora da parede lateral (também a 1,5 m de altura) e abaixo do chão.
    assert(!PointInsideTunnel(t, TrackPoint(t, mid, R + 0.5f, 1.5f)));
    assert(!PointInsideTunnel(t, TrackPoint(t, mid, -(R + 0.5f), 1.5f)));
    assert(!PointInsideTunnel(t, TrackPoint(t, mid, 0.0f, -0.5f)));
    // Antes da entrada e depois da saída (mesmo no eixo, a meia altura).
    assert(!PointInsideTunnel(t, TrackPoint(t, ts - 1.0f, 0.0f, 3.0f)));
    assert(!PointInsideTunnel(t, TrackPoint(t, te + 1.0f, 0.0f, 3.0f)));
    assert(PointInsideTunnel(t, TrackPoint(t, ts + 0.5f, 0.0f, 3.0f)));
    assert(PointInsideTunnel(t, TrackPoint(t, te - 0.5f, 0.0f, 3.0f)));
}

void Test_PointInsideTunnel()
{
    // Túnel numa reta com heading 0 (-Z do mundo).
    float ts, te;
    Track t = TunnelTrack(&ts, &te);
    CheckTunnelVolume(t, ts, te);

    // Túnel depois de uma curva de 90°: o eixo não é paralelo a nenhum eixo do mundo.
    Track c(1);
    const float R = 40.0f;
    c.AppendSegment(30.0f, 0.0f, false);
    c.AppendSegment(0.5f * kPi * R, 1.0f / R, false);
    const float cs = c.Segments().back().s_start + c.Segments().back().length;
    c.AppendSegment(60.0f, 0.0f, true);
    c.AppendSegment(100.0f, 0.0f, false);
    CheckTunnelVolume(c, cs, cs + 60.0f);

    // Um segmento que não é túnel nunca contém o ponto.
    assert(!PointInsideTunnelSegment(t.Segments()[0], TrackPoint(t, 10.0f, 0.0f, 3.0f)));
}

// ---------------------------------------------------------- Power-ups (5.2)

// Power-up de 'type' na faixa e em s (ordem de s preservada pelo chamador).
static PowerUp MakePowerUp(PowerUpType type, int lane, float s)
{
    PowerUp p = {type, lane, s, false};
    return p;
}

static float MaxJumpHeight(Game& g)
{
    g.HandleInput(Input::JUMP);
    float max_y = 0.0f;
    for (int i = 0; i < 300 && !(i > 0 && g.player.grounded); i++) {
        g.Update(1.0f / 60.0f);
        assert(g.mode == GameMode::PLAYING);
        max_y = std::max(max_y, g.player.y);
    }
    return max_y;
}

// Pegar o power-up põe o tempo cheio; pegar outro do mesmo tipo com o primeiro
// ainda ativo só renova (não soma nem empilha); tipos diferentes têm tempos
// independentes.
void Test_SameTypeRenewsTimer()
{
    Game g = QuietGame();
    g.powerups.push_back(MakePowerUp(PowerUpType::MAGNET, 0, 3.0f));
    g.powerups.push_back(MakePowerUp(PowerUpType::MAGNET, 0, 60.0f));
    g.powerups.push_back(MakePowerUp(PowerUpType::MULTIPLIER, 0, 63.0f));
    assert(!g.PowerUpActive(PowerUpType::MAGNET));
    // (Os power-ups ficam para trás e são descartados: não indexar depois.)
    RunWhile(g, [&] { return !g.PowerUpActive(PowerUpType::MAGNET); });
    assert(g.powerup_timer[(int)PowerUpType::MAGNET] > POWERUP_DURATION - 0.05f);
    assert(g.powerup_timer[(int)PowerUpType::MAGNET] <= POWERUP_DURATION);

    // Corre até logo antes do segundo ímã: o tempo já caiu bem abaixo do máximo.
    RunWhile(g, [&] { return g.player.s < 58.0f; });
    const float before = g.powerup_timer[(int)PowerUpType::MAGNET];
    assert(before > 0.0f && before < POWERUP_DURATION - 3.0f);

    // O tempo só sobe quando o 2º ímã é pego.
    float prev = before;
    RunWhile(g, [&] {
        const bool wait = g.powerup_timer[(int)PowerUpType::MAGNET] <= prev;
        prev = g.powerup_timer[(int)PowerUpType::MAGNET];
        return wait;
    });
    const float renewed = g.powerup_timer[(int)PowerUpType::MAGNET];
    assert(renewed > POWERUP_DURATION - 0.05f && renewed <= POWERUP_DURATION); // não passa de 10 s
    assert(!g.PowerUpActive(PowerUpType::MULTIPLIER)); // ainda não pegou o outro tipo

    RunWhile(g, [&] { return !g.PowerUpActive(PowerUpType::MULTIPLIER); });
    assert(g.PowerUpActive(PowerUpType::MAGNET));
    assert(g.powerup_timer[(int)PowerUpType::MAGNET] < POWERUP_DURATION - 0.1f); // não renovou o ímã
    assert(g.multiplier == 2);
}

// Com o ímã, moedas a menos de MAGNET_RADIUS do jogador (inclusive de outra
// faixa e no ar) voam até ele e são coletadas; sem o ímã a mesma moeda passa
// batido. Moedas longe do alcance não são tocadas.
void Test_MagnetPullsCoin()
{
    // Sem ímã: moeda na faixa vizinha, ao alcance do raio, não é coletada.
    {
        Game g = QuietGame();
        Coin c = {1, 4.0f, COIN_HEIGHT, false};
        g.coins.push_back(c);
        Run(g, 1.0f);
        assert(g.coins_collected == 0 && !g.coins[0].pulled);
    }
    // Com ímã.
    Game g = QuietGame();
    g.powerup_timer[(int)PowerUpType::MAGNET] = POWERUP_DURATION;
    Coin near_side = {1, 4.0f, COIN_HEIGHT, false};
    Coin high = {-1, 5.0f, 2.2f, false};
    Coin far_away = {1, 60.0f, COIN_HEIGHT, false}; // fora do raio por enquanto
    g.coins.push_back(near_side);
    g.coins.push_back(high);
    g.coins.push_back(far_away);
    g.Update(1.0f / 60.0f);
    assert(g.coins[0].pulled && g.coins[1].pulled && !g.coins[2].pulled);
    // A moeda puxada anda em direção ao jogador (já no 1º quadro está mais perto).
    const glm::vec3 body(g.player.x, g.player.y + 0.5f * PLAYER_HEIGHT, g.player.s);
    const glm::vec3 start(2.5f, COIN_HEIGHT, 4.0f);
    assert(glm::distance(CoinCenter(g.coins[0]), body) < glm::distance(start, body));
    Run(g, 1.0f);
    assert(g.coins[0].collected && g.coins[1].collected);
    assert(g.coins_collected == 2 && g.mode == GameMode::PLAYING);
    assert(g.score >= 20.0f);
    // A moeda longe só é puxada quando o jogador chega a menos de MAGNET_RADIUS
    // (no s = ~12 a distância é ~48 m: nem puxada nem coletada)...
    assert(g.player.s < 60.0f - MAGNET_RADIUS - 20.0f);
    assert(!g.coins[2].pulled && !g.coins[2].collected);
    // ...e depois de chegar perto, foi puxada e coletada, mesmo estando na
    // faixa 1 (as outras já tinham sido descartadas atrás: só o contador vale).
    assert(g.coins_collected == 2);
    Run(g, 4.5f);
    assert(g.player.s > 60.0f - MAGNET_RADIUS && g.mode == GameMode::PLAYING);
    assert(g.coins_collected == 3);
}

// Tênis: pulo com SNEAKERS_JUMP_MULT x a velocidade inicial => altura ~ x^2.
void Test_SneakersHigherJump()
{
    Game plain = QuietGame();
    const float normal = MaxJumpHeight(plain);
    assert(normal > 1.5f && normal < 2.3f);

    Game g = QuietGame();
    g.powerups.push_back(MakePowerUp(PowerUpType::SNEAKERS, 0, 2.0f));
    RunWhile(g, [&] { return !g.powerups[0].collected; });
    RunWhile(g, [&] { return !g.player.grounded; });
    const float high = MaxJumpHeight(g);
    assert(nearf(high / normal, SNEAKERS_JUMP_MULT * SNEAKERS_JUMP_MULT, 0.15f));
    assert(high > 4.0f);
    // O pulo comum da velocidade inicial: vy = JUMP_VELOCITY * multiplicador.
    Game h = QuietGame();
    h.powerup_timer[(int)PowerUpType::SNEAKERS] = 1.0f;
    h.HandleInput(Input::JUMP);
    assert(nearf(h.player.vy, JUMP_VELOCITY * SNEAKERS_JUMP_MULT));
}

// Multiplicador: enquanto ativo (multiplier = 2) cada moeda vale 20 pontos.
void Test_MultiplierDoublesCoinScore()
{
    Game plain = QuietGame();
    Coin c = {0, 2.0f, COIN_HEIGHT, false};
    plain.coins.push_back(c);
    Run(plain, 0.5f);
    assert(plain.coins_collected == 1 && plain.multiplier == 1);
    assert(nearf(plain.score, plain.player.s + 10.0f, 0.05f));

    Game g = QuietGame();
    g.powerups.push_back(MakePowerUp(PowerUpType::MULTIPLIER, 0, 1.0f));
    Coin c1 = {0, 6.0f, COIN_HEIGHT, false};
    Coin c2 = {0, 8.0f, COIN_HEIGHT, false};
    g.coins.push_back(c1);
    g.coins.push_back(c2);
    Run(g, 1.0f);
    assert(g.coins_collected == 2 && g.multiplier == 2);
    assert(nearf(g.score, g.player.s + 2 * 20.0f, 0.05f));
}

// Passados POWERUP_DURATION s os efeitos acabam: multiplicador volta a 1, o
// pulo volta ao normal, o ímã deixa de puxar.
void Test_PowerupExpires()
{
    Game g = QuietGame();
    g.powerup_timer[(int)PowerUpType::MAGNET] = POWERUP_DURATION;
    g.powerup_timer[(int)PowerUpType::SNEAKERS] = POWERUP_DURATION;
    g.powerup_timer[(int)PowerUpType::MULTIPLIER] = POWERUP_DURATION;
    Run(g, POWERUP_DURATION - 0.5f);
    assert(g.PowerUpActive(PowerUpType::MAGNET) && g.PowerUpActive(PowerUpType::SNEAKERS));
    assert(g.multiplier == 2);
    Run(g, 0.6f);
    for (int t = 0; t < POWERUP_TYPE_COUNT; t++) assert(g.powerup_timer[t] == 0.0f);
    assert(g.multiplier == 1);
    assert(!g.PowerUpActive(PowerUpType::SNEAKERS));
    g.HandleInput(Input::JUMP);
    assert(nearf(g.player.vy, JUMP_VELOCITY));
    // Sem ímã, a moeda ao lado deixa de ser puxada.
    RunWhile(g, [&] { return !g.player.grounded; });
    Coin side = {1, g.player.s + 4.0f, COIN_HEIGHT, false};
    g.coins.push_back(side);
    g.Update(1.0f / 60.0f);
    assert(!g.coins.back().pulled);
}

// Fora de PLAYING (pausa, câmera livre, fim de jogo) os tempos não correm.
void Test_PowerupTimersFrozenOutsidePlaying()
{
    const GameMode modes[3] = {GameMode::PAUSED, GameMode::FREE_CAMERA, GameMode::GAME_OVER};
    for (int m = 0; m < 3; m++) {
        Game g = QuietGame();
        g.powerup_timer[(int)PowerUpType::MAGNET] = 5.0f;
        g.powerup_timer[(int)PowerUpType::MULTIPLIER] = 5.0f;
        g.mode = modes[m];
        Run(g, 2.0f);
        assert(g.powerup_timer[(int)PowerUpType::MAGNET] == 5.0f);
        assert(g.powerup_timer[(int)PowerUpType::MULTIPLIER] == 5.0f);
    }
    // Voltando a PLAYING (pausa e retoma) o tempo continua de onde parou.
    Game g = QuietGame();
    g.powerup_timer[(int)PowerUpType::MAGNET] = 5.0f;
    g.mode = GameMode::PAUSED;
    Run(g, 3.0f);
    g.mode = GameMode::PLAYING;
    Run(g, 1.0f);
    assert(nearf(g.powerup_timer[(int)PowerUpType::MAGNET], 4.0f, 0.05f));
}

// O cofrinho não tem duração: pegá-lo não mexe nos tempos nem no multiplicador;
// o pedido de porco (piggy_pickups) é consumido no mesmo passo (Tarefa 5.3) e
// lança o porco voador. O segundo cofrinho, pego com o porco ainda voando, é
// ignorado.
void Test_PiggyPickupLaunchesPig()
{
    Game g = QuietGame();
    g.powerups.push_back(MakePowerUp(PowerUpType::PIGGY, 0, 3.0f));
    g.powerups.push_back(MakePowerUp(PowerUpType::PIGGY, 0, 20.0f));
    assert(g.piggy_pickups == 0 && !g.pig.active);
    RunWhile(g, [&] { return !g.pig.active; });
    assert(g.piggy_pickups == 0); // consumido no lançamento
    for (int t = 0; t < POWERUP_TYPE_COUNT; t++) assert(g.powerup_timer[t] == 0.0f);
    assert(g.multiplier == 1);
    Run(g, 0.5f);
    assert(g.pig.active && g.pig.t > 0.0f); // um único porco, ainda voando
}

// Power-up de outra faixa não é coletado; rolando também pega (a esfera de
// coleta cobre a altura do rolamento).
void Test_PowerupOtherLaneNotCollected()
{
    Game g = QuietGame();
    g.powerups.push_back(MakePowerUp(PowerUpType::MULTIPLIER, 1, 3.0f));
    Run(g, 1.0f);
    assert(!g.powerups[0].collected && !g.PowerUpActive(PowerUpType::MULTIPLIER));
    Game r = QuietGame();
    r.powerups.push_back(MakePowerUp(PowerUpType::MAGNET, 0, 3.0f));
    r.HandleInput(Input::ROLL);
    Run(r, 0.3f);
    assert(r.powerups[0].collected);
}

// Câmera nos pulos: o pulo comum não a move; o do tênis a sobe (parte do que
// passa de CAMERA_JUMP_FREE_HEIGHT) sem nunca passar do jogador.
void Test_CameraFollowsHighJump()
{
    Game plain = QuietGame();
    plain.HandleInput(Input::JUMP);
    for (int i = 0; i < 60; i++) {
        plain.Update(1.0f / 60.0f);
        assert(plain.CameraTargetHeight() == plain.GroundHeight());
    }
    Game g = QuietGame();
    g.powerup_timer[(int)PowerUpType::SNEAKERS] = POWERUP_DURATION;
    g.HandleInput(Input::JUMP);
    float max_target = 0.0f;
    for (int i = 0; i < 90; i++) {
        g.Update(1.0f / 60.0f);
        const float target = g.CameraTargetHeight();
        assert(target >= g.GroundHeight() && target < g.player.y + 1e-3f);
        max_target = std::max(max_target, target);
    }
    assert(max_target > 1.5f && max_target < 3.0f);
    assert(g.player.grounded && g.CameraTargetHeight() == 0.0f);
}

// Reset zera tempos, power-ups, cofrinhos e refaz os mesmos power-ups.
void Test_PowerupResetClears()
{
    Game g(5);
    g.GenerateChunksUntil(60 * CHUNK_LENGTH);
    const std::vector<PowerUp> first = g.powerups;
    assert(!first.empty());
    g.powerup_timer[(int)PowerUpType::MAGNET] = 4.0f;
    g.piggy_pickups = 3;
    g.multiplier = 2;
    g.Reset();
    assert(g.powerups.empty() && g.piggy_pickups == 0 && g.multiplier == 1);
    for (int t = 0; t < POWERUP_TYPE_COUNT; t++) assert(g.powerup_timer[t] == 0.0f);
    g.GenerateChunksUntil(60 * CHUNK_LENGTH);
    assert(g.powerups.size() == first.size());
    for (size_t i = 0; i < first.size(); i++)
        assert(g.powerups[i].type == first[i].type && g.powerups[i].lane == first[i].lane &&
               g.powerups[i].s == first[i].s);
}

// Spawn: raro (~1 por 6 chunks), todos os tipos aparecem, sempre na faixa
// segura do chunk, dentro do chunk, e nunca sobre obstáculo ou moeda.
void Test_PowerupSpawnRules()
{
    int total = 0, chunks_total = 0;
    int per_type[POWERUP_TYPE_COUNT] = {0, 0, 0, 0};
    for (unsigned seed = 1; seed <= 20; seed++) {
        Game g(seed);
        const int chunks = 300;
        g.GenerateChunksUntil(chunks * CHUNK_LENGTH);
        chunks_total += chunks;
        for (size_t i = 0; i < g.powerups.size(); i++) {
            const PowerUp& p = g.powerups[i];
            total++;
            per_type[(int)p.type]++;
            const int chunk = (int)std::floor(p.s / CHUNK_LENGTH);
            assert(chunk >= 1);
            assert(p.lane == g.SafeLaneOfChunk(chunk));
            assert(!p.collected);
            if (i > 0) assert(g.powerups[i].s >= g.powerups[i - 1].s); // ordem de s
            const glm::vec3 c(p.lane * LANE_WIDTH, POWERUP_HEIGHT, p.s);
            // Nada sólido a menos de POWERUP_CLEARANCE em s, na mesma faixa.
            for (size_t k = 0; k < g.obstacles.size(); k++) {
                const Obstacle& o = g.obstacles[k];
                if (o.lane != p.lane) continue;
                AABB parts[3];
                int n = ObstacleParts(o, parts);
                for (int q = 0; q < n; q++) {
                    assert(!SphereIntersectsAABB(c, POWERUP_RADIUS, parts[q]));
                    assert(p.s < parts[q].min.z - POWERUP_CLEARANCE + 1e-3f ||
                           p.s > parts[q].max.z + POWERUP_CLEARANCE - 1e-3f);
                }
            }
            for (size_t k = 0; k < g.coins.size(); k++) {
                const Coin& co = g.coins[k];
                assert(!SphereIntersectsSphere(c, POWERUP_RADIUS, CoinCenter(co), COIN_RADIUS));
                if (co.lane == p.lane) assert(std::fabs(co.s - p.s) >= POWERUP_CLEARANCE - 1e-3f);
            }
        }
    }
    const float rate = (float)total / (float)chunks_total;
    assert(rate > 0.10f && rate < 0.22f); // ~ 1/6
    for (int t = 0; t < POWERUP_TYPE_COUNT; t++) assert(per_type[t] > 0);
    printf("  [power-ups] %d em %d chunks (%.3f por chunk); ima %d, tenis %d, x2 %d, cofrinho %d\n",
           total, chunks_total, rate, per_type[0], per_type[1], per_type[2], per_type[3]);
}

// Gerador próprio: ligar/desligar power-ups não muda obstáculos nem moedas, e
// a mesma semente dá os mesmos power-ups.
void Test_PowerupSpawnDeterministic()
{
    for (unsigned seed = 1; seed <= 5; seed++) {
        Game a(seed), b(seed), none(seed);
        none.powerups_enabled = false;
        none.Reset();
        a.GenerateChunksUntil(200 * CHUNK_LENGTH);
        b.GenerateChunksUntil(200 * CHUNK_LENGTH);
        none.GenerateChunksUntil(200 * CHUNK_LENGTH);
        assert(none.powerups.empty() && !a.powerups.empty());
        assert(a.powerups.size() == b.powerups.size());
        for (size_t i = 0; i < a.powerups.size(); i++)
            assert(a.powerups[i].type == b.powerups[i].type && a.powerups[i].lane == b.powerups[i].lane &&
                   a.powerups[i].s == b.powerups[i].s);
        assert(a.obstacles.size() == none.obstacles.size() && a.coins.size() == none.coins.size());
        for (size_t i = 0; i < a.obstacles.size(); i++)
            assert(a.obstacles[i].type == none.obstacles[i].type && a.obstacles[i].s == none.obstacles[i].s &&
                   a.obstacles[i].lane == none.obstacles[i].lane);
        for (size_t i = 0; i < a.coins.size(); i++)
            assert(a.coins[i].lane == none.coins[i].lane && a.coins[i].s == none.coins[i].s);
    }
}

// O que ficou mais de DESPAWN_BEHIND para trás é descartado (sem vazamento).
void Test_PowerupDespawnBehind()
{
    Game g(3);
    g.GenerateChunksUntil(100 * CHUNK_LENGTH);
    const size_t before = g.powerups.size();
    assert(before > 5);
    g.player.s = 1500.0f; // teleporte: tudo antes de 1480 m fica para trás
    g.track.EnsureGeneratedUntil(1500.0f + SPAWN_AHEAD);
    g.spawn_enabled = false;
    g.Update(1.0f / 60.0f);
    assert(g.powerups.size() < before);
    for (size_t i = 0; i < g.powerups.size(); i++)
        assert(g.powerups[i].s >= g.player.s - DESPAWN_BEHIND);
}

// Teto na aproximação do túnel: contínuo (no portal vale o teto de verdade) e
// crescente com a distância; Track::TunnelDistanceAhead dá 0 dentro, a
// distância até a boca à frente, ou -1.
void Test_TunnelApproach()
{
    assert(TunnelApproachCeiling(7.0f, 0.0f, 0.25f) == 7.0f);
    assert(TunnelApproachCeiling(7.0f, -3.0f, 0.25f) == 7.0f);
    assert(nearf(TunnelApproachCeiling(7.0f, 8.0f, 0.25f), 9.0f));
    Track t(1);
    const float L0 = t.Segments()[0].length;
    t.AppendSegment(50.0f, 0.0f, true);
    assert(t.TunnelDistanceAhead(L0 + 10.0f, 16.0f) == 0.0f);
    assert(t.TunnelDistanceAhead(L0, 16.0f) == 0.0f);
    assert(nearf(t.TunnelDistanceAhead(L0 - 5.0f, 16.0f), 5.0f));
    assert(t.TunnelDistanceAhead(L0 - 17.0f, 16.0f) < 0.0f);
    assert(t.TunnelDistanceAhead(0.0f, 16.0f) < 0.0f || L0 <= 16.0f);
}

// Tênis + túnel (Tarefas 4.4/5.2): um pulo alto que ENTRA no túnel no ar não
// leva um tranco. Em cada quadro no ar, o teto pode (a) absorver a subida do
// quadro (bater a cabeça: para de subir) e (b) acompanhar a descida suave da
// folga da aproximação; o que passar disso é um "tranco" (a cabeça sendo
// empurrada para baixo além do movimento livre). Esse excesso fica abaixo de
// TUNNEL_BOUND, também saindo do teto de um trem (pulo de 3.2 + 4.5 m) em
// direção à boca do túnel; e dentro do túnel a cabeça nunca passa da abóbada.
static float MaxCeilingCorrection(Game& g, bool* entered_tunnel_airborne)
{
    float worst = 0.0f;
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 400; i++) {
        const bool air = !g.player.grounded;
        const float y0 = g.player.y, vy0 = g.player.vy;
        g.Update(dt);
        assert(g.mode == GameMode::PLAYING);
        if (air && !g.player.grounded) {
            // Movimento livre do quadro (cinemática exata sob gravidade).
            const float free_dy = vy0 * dt - 0.5f * GRAVITY * dt * dt;
            const float pushed_down = free_dy - (g.player.y - y0);
            worst = std::max(worst, pushed_down - std::max(free_dy, 0.0f));
            if (g.track.IsTunnelAt(g.player.s))
                *entered_tunnel_airborne = true;
        }
        const AABB p = g.PlayerBox();
        if (g.track.IsTunnelAt(p.max.z) || g.track.IsTunnelAt(p.min.z)) {
            const float ceiling = std::min(TunnelCeilingHeight(p.min.x), TunnelCeilingHeight(p.max.x));
            assert(p.max.y <= ceiling + 1e-3f);
        }
        if (g.player.grounded && i > 5) break;
    }
    return worst;
}

void Test_SneakersTunnelEntryNoPop()
{
    const float TUNNEL_BOUND = 0.07f; // ~ TUNNEL_APPROACH_SLOPE * 12.5 m/s / 60 + folga
    int cases = 0, entered_cases = 0;
    for (int lane = 0; lane <= 1; lane++)
        for (int from_roof = 0; from_roof <= 1; from_roof++)
            for (float off = 2.0f; off <= 24.0f; off += 0.5f) {
                Game g = QuietGame();
                g.track = Track(1);
                const float L0 = g.track.Segments()[0].length;
                g.track.AppendSegment(200.0f, 0.0f, true);
                g.player.lane = lane;
                g.player.x = lane * LANE_WIDTH;
                g.player.s = L0 - off;
                g.powerup_timer[(int)PowerUpType::SNEAKERS] = POWERUP_DURATION;
                if (from_roof) {
                    // Já no ar, saindo do teto de um trem (3.2 m), pulo de tênis.
                    g.player.y = 3.2f;
                    g.player.grounded = false;
                    g.player.vy = JUMP_VELOCITY * SNEAKERS_JUMP_MULT;
                } else {
                    g.HandleInput(Input::JUMP);
                }
                bool entered = false;
                const float worst = MaxCeilingCorrection(g, &entered);
                assert(worst <= TUNNEL_BOUND);
                cases++;
                if (entered) entered_cases++;
            }
    assert(entered_cases > 20); // o varrimento realmente entra no túnel no ar
    printf("  [tunel+tenis] %d saltos, %d entraram no tunel no ar, correcao do teto por quadro <= %.2f m\n",
           cases, entered_cases, TUNNEL_BOUND);
}

// ------------------------------------------------- Bézier e porco voador (5.3)

static const glm::vec3 kP0(-5.0f, 5.0f, 12.0f), kP1(3.0f, 8.0f, 20.0f), kP2(-3.0f, 8.0f, 28.0f),
                       kP3(5.0f, 5.0f, 36.0f);

static bool nearv(const glm::vec3& a, const glm::vec3& b, float eps = kEps)
{
    return nearf(a.x, b.x, eps) && nearf(a.y, b.y, eps) && nearf(a.z, b.z, eps);
}

void Test_BezierEndpoints()
{
    assert(nearv(CubicBezier(kP0, kP1, kP2, kP3, 0.0f), kP0));
    assert(nearv(CubicBezier(kP0, kP1, kP2, kP3, 1.0f), kP3));
}

// B(0.5) = (P0 + 3 P1 + 3 P2 + P3) / 8.
void Test_BezierMidpoint()
{
    const glm::vec3 expected = (kP0 + 3.0f * kP1 + 3.0f * kP2 + kP3) / 8.0f;
    assert(nearv(CubicBezier(kP0, kP1, kP2, kP3, 0.5f), expected));
    // Pontos de controle colineares e equidistantes: t é linear.
    const glm::vec3 a(0.0f), b(1.0f, 0.0f, 0.0f), c(2.0f, 0.0f, 0.0f), d(3.0f, 0.0f, 0.0f);
    assert(nearv(CubicBezier(a, b, c, d, 0.25f), glm::vec3(0.75f, 0.0f, 0.0f)));
}

// A tangente é a derivada: bate com a diferença finita central e, nas pontas,
// com 3 (P1 - P0) e 3 (P3 - P2).
void Test_BezierTangent()
{
    assert(nearv(CubicBezierTangent(kP0, kP1, kP2, kP3, 0.0f), 3.0f * (kP1 - kP0)));
    assert(nearv(CubicBezierTangent(kP0, kP1, kP2, kP3, 1.0f), 3.0f * (kP3 - kP2)));
    const float h = 1e-3f;
    for (float t = 0.1f; t < 0.95f; t += 0.1f) {
        const glm::vec3 fd = (CubicBezier(kP0, kP1, kP2, kP3, t + h) -
                              CubicBezier(kP0, kP1, kP2, kP3, t - h)) / (2.0f * h);
        assert(nearv(CubicBezierTangent(kP0, kP1, kP2, kP3, t), fd, 5e-2f));
    }
}

// Lança um porco: um cofrinho pego é consumido no Update seguinte.
static void LaunchPig(Game& g)
{
    g.piggy_pickups = 1;
    g.Update(1.0f / 60.0f);
    assert(g.pig.active && g.piggy_pickups == 0);
}

// Ao longo de PIGGY_FLIGHT_TIME o porco solta ~PIGGY_FLIGHT_TIME /
// PIGGY_COIN_INTERVAL = 26 moedas, todas numa faixa válida e à frente do
// jogador, e o porco acaba desativando.
void Test_PigDropsCoins()
{
    Game g = QuietGame();
    LaunchPig(g);
    float last_s = -1.0f;
    for (int i = 0; i < 300 && g.pig.active; i++) {
        g.Update(1.0f / 60.0f);
        for (size_t k = 0; k < g.coins.size(); k++) {
            assert(g.coins[k].lane >= -1 && g.coins[k].lane <= 1);
            assert(g.coins[k].s > 0.0f);
            last_s = std::max(last_s, g.coins[k].s);
        }
    }
    assert(!g.pig.active);
    assert(g.pig_coins_dropped >= 25 && g.pig_coins_dropped <= 27);
    // O rastro vai até perto do fim da curva (P3.z = 36 à frente do jogador).
    assert(last_s > g.player.s + 30.0f);
}

// Cada moeda cai do porco até COIN_HEIGHT (animação por dt, nunca abaixo) e
// termina parada em COIN_HEIGHT.
void Test_PigCoinsFallToCoinHeight()
{
    Game g = QuietGame();
    g.player.speed = 0.0f; // jogador fica no lugar: nenhuma moeda é coletada de passagem
    LaunchPig(g);
    g.player.lane = 0;
    bool saw_falling = false;
    for (int i = 0; i < 60 * 6; i++) {
        g.player.speed = 0.0f;
        g.Update(1.0f / 60.0f);
        for (size_t k = 0; k < g.coins.size(); k++) {
            const Coin& c = g.coins[k];
            if (c.collected || c.pulled) continue;
            assert(c.y >= COIN_HEIGHT - 1e-5f);
            if (c.falling) { saw_falling = true; assert(c.y > COIN_HEIGHT); }
        }
    }
    assert(saw_falling);
    for (size_t k = 0; k < g.coins.size(); k++)
        if (!g.coins[k].collected) assert(!g.coins[k].falling && g.coins[k].y == COIN_HEIGHT);
}

// Só um porco por vez: cofrinhos pegos com ele voando são descartados (o
// contador zera, o voo continua de onde estava) e não nasce outro depois.
void Test_SinglePigActive()
{
    Game g = QuietGame();
    LaunchPig(g);
    Run(g, 1.0f);
    const float t_before = g.pig.t;
    g.piggy_pickups = 2;
    g.Update(1.0f / 60.0f);
    assert(g.pig.active && g.piggy_pickups == 0);
    assert(nearf(g.pig.t, t_before + (1.0f / 60.0f) / PIGGY_FLIGHT_TIME, 1e-4f)); // não reiniciou
    Run(g, PIGGY_FLIGHT_TIME);
    assert(!g.pig.active);
    for (int i = 0; i < 30; i++) g.Update(1.0f / 60.0f);
    assert(!g.pig.active && g.piggy_pickups == 0); // nenhum segundo porco
    // Um novo cofrinho, com o porco já parado, lança outro voo.
    g.piggy_pickups = 1;
    g.Update(1.0f / 60.0f);
    assert(g.pig.active && g.pig.t < 0.05f);
}

// O parâmetro t anda com dt: o estado ao fim do voo não depende da taxa de
// quadros (30, 60 ou 120 Hz).
void Test_PigDtIndependent()
{
    const float rates[3] = {30.0f, 60.0f, 120.0f};
    int dropped[3];
    for (int r = 0; r < 3; r++) {
        Game g = QuietGame();
        LaunchPig(g); // 1 passo de 1/60 s já dado
        Run(g, 2.0f - 1.0f / 60.0f, 1.0f / rates[r]);
        assert(nearf(g.pig.t, 0.5f, 0.02f));
        Run(g, 2.5f, 1.0f / rates[r]);
        assert(!g.pig.active);
        dropped[r] = g.pig_coins_dropped;
        assert(dropped[r] >= 25 && dropped[r] <= 27);
    }
    assert(std::abs(dropped[0] - dropped[2]) <= 1);
}

// A posição do porco acompanha o jogador: em t = 0 está em (-5, 5, s + 12) e
// em t = 1, em (5, 5, s + 36), sendo s o do jogador naquele instante.
void Test_PigFollowsPlayer()
{
    Game g = QuietGame();
    g.piggy_pickups = 1;
    g.Update(1e-6f); // lança com t ~ 0
    glm::vec3 p = g.PigTrackPosition();
    assert(nearv(p, glm::vec3(-5.0f, 5.0f, g.player.s + 12.0f), 1e-2f));
    g.pig.t = 1.0f;
    p = g.PigTrackPosition();
    assert(nearv(p, glm::vec3(5.0f, 5.0f, g.player.s + 36.0f), 1e-3f));
    // A velocidade absoluta inclui a do jogador (sempre avançando).
    g.pig.t = 0.5f;
    assert(g.PigTrackVelocity().z > g.player.speed);
}

// Moedas soltas nunca ficam dentro de obstáculo: se o ponto de queda cai
// numa caixa (trem, barreira, placa), a moeda é pulada.
void Test_PigCoinsAvoidObstacles()
{
    Game g = QuietGame();
    Game free_game = QuietGame();
    for (int lane = -1; lane <= 1; lane++) {
        g.obstacles.push_back(g.MakeObstacle(ObstacleType::BARRIER_LOW, lane, 18.0f));
        if (lane != 0) g.obstacles.push_back(g.MakeObstacle(ObstacleType::TRAIN, lane, 30.0f));
        g.obstacles.push_back(g.MakeObstacle(ObstacleType::SIGN, lane, 40.0f));
    }
    // Jogador parado (speed = 0 a cada passo): o porco fica sobre o mesmo
    // trecho e as moedas ficam onde caíram.
    LaunchPig(g);
    LaunchPig(free_game);
    for (int i = 0; i < 60 * 5; i++) {
        g.player.speed = 0.0f;
        g.Update(1.0f / 60.0f);
        free_game.player.speed = 0.0f;
        free_game.Update(1.0f / 60.0f);
        assert(g.mode == GameMode::PLAYING);
        for (size_t k = 0; k < g.coins.size(); k++) {
            const Coin& c = g.coins[k];
            const glm::vec3 center(c.lane * LANE_WIDTH, COIN_HEIGHT, c.s);
            for (size_t j = 0; j < g.obstacles.size(); j++) {
                AABB parts[3];
                const int n = ObstacleParts(g.obstacles[j], parts);
                for (int q = 0; q < n; q++)
                    assert(!SphereIntersectsAABB(center, COIN_RADIUS, parts[q]));
            }
        }
    }
    // Sem obstáculos sai o rastro inteiro; com eles, algumas moedas foram puladas.
    assert(g.pig_coins_dropped < free_game.pig_coins_dropped);
}

// Nenhum cofrinho nasce com um túnel nos PIGGY_SPAWN_TUNNEL_CLEARANCE m à
// frente dele (o voo inteiro cabe aí); cofrinhos continuam existindo, e a
// sequência de power-ups é determinística (o cofrinho só troca de tipo).
void Test_PigNoSpawnBeforeTunnel()
{
    // O remap do cofrinho perto de túnel deriva do enum (Tarefa 7.1a): o PIGGY
    // é o último tipo e os demais (com duração) são os POWERUP_TIMED_TYPE_COUNT
    // primeiros.
    assert((int)PowerUpType::PIGGY == POWERUP_TYPE_COUNT - 1);
    assert(POWERUP_TIMED_TYPE_COUNT == POWERUP_TYPE_COUNT - 1);
    int piggies = 0, near_tunnel = 0;
    int near_by_type[POWERUP_TYPE_COUNT] = {0};
    for (unsigned seed = 1; seed <= 60; seed++) {
        Game g(seed);
        g.GenerateChunksUntil(400 * CHUNK_LENGTH);
        for (size_t i = 0; i < g.powerups.size(); i++) {
            const PowerUp& p = g.powerups[i];
            if (g.track.TunnelOverlaps(p.s, p.s + PIGGY_SPAWN_TUNNEL_CLEARANCE)) {
                near_tunnel++;
                near_by_type[(int)p.type]++;
            }
            if (p.type != PowerUpType::PIGGY) continue;
            piggies++;
            assert(!g.track.TunnelOverlaps(p.s, p.s + PIGGY_SPAWN_TUNNEL_CLEARANCE));
        }
        Game h(seed);
        h.GenerateChunksUntil(400 * CHUNK_LENGTH);
        assert(h.powerups.size() == g.powerups.size());
        for (size_t i = 0; i < g.powerups.size(); i++)
            assert(h.powerups[i].type == g.powerups[i].type && h.powerups[i].s == g.powerups[i].s);
    }
    assert(piggies > 50);
    assert(near_tunnel > 0); // há power-ups perto de túnel: a regra foi exercida
    // Perto de túnel, o cofrinho some e os outros tipos (todos) aparecem.
    assert(near_by_type[(int)PowerUpType::PIGGY] == 0);
    for (int t = 0; t < POWERUP_TIMED_TYPE_COUNT; t++)
        assert(near_by_type[t] > 0);
}

// Rede de segurança: com o porco sobrevoando um túnel, nenhuma moeda cai
// dentro dele.
void Test_PigNoCoinsInsideTunnel()
{
    for (unsigned seed = 1; seed <= 40; seed++) {
        Game g(seed);
        g.spawn_enabled = false;
        g.track.EnsureGeneratedUntil(1000.0f);
        float tunnel_start = -1.0f;
        for (size_t i = 0; i < g.track.Segments().size(); i++)
            if (g.track.Segments()[i].tunnel && g.track.Segments()[i].s_start > 40.0f) {
                tunnel_start = g.track.Segments()[i].s_start;
                break;
            }
        if (tunnel_start < 0.0f || tunnel_start > 500.0f) continue;
        g.player.s = tunnel_start - 20.0f; // o rastro (12..36 m à frente) cruza a boca
        LaunchPig(g);
        for (int i = 0; i < 60 * 5; i++) {
            g.player.speed = 0.0f;
            g.Update(1.0f / 60.0f);
            for (size_t k = 0; k < g.coins.size(); k++)
                assert(!g.track.IsTunnelAt(g.coins[k].s));
        }
        assert(g.pig_coins_dropped > 0 && g.pig_coins_dropped < 25);
        return; // uma semente com túnel perto basta
    }
    assert(false && "nenhuma semente com túnel na faixa esperada");
}

// Pausa (e câmera livre) congela o porco; Reset o apaga junto com as moedas.
void Test_PigPauseAndReset()
{
    Game g = QuietGame();
    LaunchPig(g);
    Run(g, 1.0f);
    const float t = g.pig.t;
    g.mode = GameMode::PAUSED;
    Run(g, 2.0f);
    g.mode = GameMode::FREE_CAMERA;
    Run(g, 2.0f);
    assert(g.pig.t == t && g.pig.active);
    g.mode = GameMode::PLAYING;
    g.Reset();
    assert(!g.pig.active && g.pig.t == 0.0f && g.pig_coins_dropped == 0 && g.coins.empty());
}

// ------------------------------------------------- Partículas (6.1)

static int AliveCount(const ParticleSystem& ps) { return (int)ps.Alive().size(); }

// Toda partícula some depois do maior tempo de vida (0.9 s) e a explosão cria
// o número pedido, no ponto da coleta, com os limites de velocidade/vida/tamanho.
void Test_ParticlesExpire()
{
    ParticleSystem ps(PARTICLE_CAPACITY);
    assert(AliveCount(ps) == 0);
    const vec4 origin(3.0f, 1.0f, -20.0f, 1.0f);
    ps.Emit(origin, 30, glm::vec3(1.0f, 0.8f, 0.2f), 7);
    assert(AliveCount(ps) == 30);
    for (size_t i = 0; i < ps.Alive().size(); ++i) {
        const Particle& p = ps.Alive()[i];
        assert(near(p.position, origin));
        assert(p.velocity.y >= 0.0f);                       // hemisfério superior
        const float speed = glm::length(glm::vec3(p.velocity.x, p.velocity.y, p.velocity.z));
        assert(speed >= PARTICLE_MIN_SPEED - 1e-3f && speed <= PARTICLE_MAX_SPEED + 1e-3f);
        assert(p.life == p.max_life);
        assert(p.max_life >= PARTICLE_MIN_LIFE && p.max_life <= PARTICLE_MAX_LIFE);
        assert(p.size >= PARTICLE_MIN_SIZE && p.size <= PARTICLE_MAX_SIZE);
    }
    // Antes do menor tempo de vida (0.4 s) todas estão vivas.
    for (int i = 0; i < 20; ++i) ps.Update(0.016f);   // 0.32 s
    assert(AliveCount(ps) == 30);
    // Depois de 0.9 s (+ folga) nenhuma sobra.
    for (int i = 0; i < 50; ++i) ps.Update(0.016f);   // 1.12 s no total
    assert(AliveCount(ps) == 0);

    // As vidas diferem: aos 0.512 s algumas já morreram e outras não.
    ps.Emit(origin, 80, glm::vec3(1.0f), 11);
    for (int i = 0; i < 32; ++i) ps.Update(0.016f);   // 0.512 s
    assert(AliveCount(ps) > 0 && AliveCount(ps) < 80);
    ps.Clear();
    assert(AliveCount(ps) == 0);
}

// O pool tem capacidade fixa: emitir 2000 não passa de 512 vivas; as vagas
// liberadas pela morte voltam a ser usadas.
void Test_CapacityRespected()
{
    ParticleSystem ps(512);
    ps.Emit(vec4(0, 0, 0, 1), 2000, glm::vec3(1.0f), 1);
    assert(AliveCount(ps) <= 512);
    assert(AliveCount(ps) == 512);
    ps.Emit(vec4(0, 0, 0, 1), 100, glm::vec3(1.0f), 2);   // cheio: descarta
    assert(AliveCount(ps) == 512);
    for (int i = 0; i < 70; ++i) ps.Update(0.016f);
    assert(AliveCount(ps) == 0);
    ps.Emit(vec4(0, 0, 0, 1), 40, glm::vec3(1.0f), 3);
    assert(AliveCount(ps) == 40);

    ParticleSystem tiny(10);
    tiny.Emit(vec4(0, 0, 0, 1), 25, glm::vec3(1.0f), 4);
    assert(AliveCount(tiny) == 10);
    tiny.Emit(vec4(0, 0, 0, 1), -5, glm::vec3(1.0f), 5);  // contagem negativa: nada
    assert(AliveCount(tiny) == 10);
}

// Sem alocação em tempo de jogo: depois de muitas emissões e atualizações o
// armazenamento interno continua no mesmo endereço (o vetor nunca realoca).
void Test_ParticlesNoReallocation()
{
    ParticleSystem ps(512);
    ps.Emit(vec4(0, 0, 0, 1), 1, glm::vec3(1.0f), 0);
    const Particle* storage = ps.Alive().data();
    assert(storage != nullptr);
    for (int round = 0; round < 200; ++round) {
        ps.Emit(vec4(0, 1, 0, 1), 80, glm::vec3(1.0f), 100 + round);
        for (int k = 0; k < 5; ++k) ps.Update(0.016f);
        assert(ps.Alive().data() == storage);
        assert(AliveCount(ps) <= 512);
    }
}

// Mesmo resultado com passos de 1/30 s e 1/120 s (posição integrada com dt
// de forma exata): partículas movem-se por dt, não por quadro. Compara pares
// de mesma semente (a ordem é a mesma enquanto nenhuma morre).
void Test_ParticlesMoveWithDt()
{
    ParticleSystem a(512), b(512);
    const vec4 origin(1.0f, 0.5f, -10.0f, 1.0f);
    a.Emit(origin, 40, glm::vec3(1.0f), 42);
    b.Emit(origin, 40, glm::vec3(1.0f), 42);
    for (int i = 0; i < 9; ++i)  a.Update(1.0f / 30.0f);    // 0.3 s
    for (int i = 0; i < 36; ++i) b.Update(1.0f / 120.0f);   // 0.3 s
    assert(AliveCount(a) == 40 && AliveCount(b) == 40);
    for (size_t i = 0; i < 40; ++i) {
        assert(near(a.Alive()[i].position, b.Alive()[i].position, 1e-3f));
        assert(near(a.Alive()[i].velocity, b.Alive()[i].velocity, 1e-3f));
        assert(nearf(a.Alive()[i].life, b.Alive()[i].life, 1e-3f));
    }
    // Analítico com uma partícula: v.y cai PARTICLE_GRAVITY por segundo e
    // p = p0 + v0 t - g t^2 / 2 em y.
    ParticleSystem one(8);
    one.Emit(origin, 1, glm::vec3(1.0f), 5);
    const Particle p0 = one.Alive()[0];
    for (int i = 0; i < 12; ++i) one.Update(0.02f);          // 0.24 s < vida mínima
    const Particle p1 = one.Alive()[0];
    const float t = 0.24f;
    assert(nearf(p1.velocity.y, p0.velocity.y - PARTICLE_GRAVITY * t, 1e-3f));
    assert(nearf(p1.position.y, p0.position.y + p0.velocity.y * t - 0.5f * PARTICLE_GRAVITY * t * t, 1e-3f));
    assert(nearf(p1.position.x, p0.position.x + p0.velocity.x * t, 1e-3f));
    assert(nearf(p1.position.z, p0.position.z + p0.velocity.z * t, 1e-3f));
    // dt = 0 congela; dt negativo é ignorado.
    one.Update(0.0f);
    one.Update(-1.0f);
    assert(near(one.Alive()[0].position, p1.position, 1e-6f));
}

// Mesma semente -> mesma explosão; sementes diferentes -> explosões diferentes.
void Test_ParticlesDeterministic()
{
    ParticleSystem a(512), b(512), c(512);
    const vec4 o(0, 1, 0, 1);
    a.Emit(o, 50, glm::vec3(1.0f), 9);
    b.Emit(o, 50, glm::vec3(1.0f), 9);
    c.Emit(o, 50, glm::vec3(1.0f), 10);
    bool differs = false;
    for (size_t i = 0; i < 50; ++i) {
        assert(near(a.Alive()[i].velocity, b.Alive()[i].velocity, 1e-9f));
        assert(a.Alive()[i].life == b.Alive()[i].life && a.Alive()[i].size == b.Alive()[i].size);
        if (!near(a.Alive()[i].velocity, c.Alive()[i].velocity)) differs = true;
    }
    assert(differs);
}

// Game: a coleta de uma moeda registra um evento COIN no centro dela.
void Test_CoinPickupEmitsEvent()
{
    Game g = QuietGame();
    assert(g.events.empty());
    Coin c = {0, 4.0f, COIN_HEIGHT, false};
    g.coins.push_back(c);
    RunWhile(g, [&] { return g.coins_collected == 0 && g.player.s < 20.0f; });
    assert(g.coins_collected == 1);
    assert(g.events.size() == 1);
    assert(g.events[0].kind == CollectKind::COIN);
    assert(nearf(g.events[0].position.x, 0.0f) && nearf(g.events[0].position.y, COIN_HEIGHT));
    assert(nearf(g.events[0].position.z, 4.0f));
    // O evento fica até o consumidor limpar; a mesma moeda não gera outro.
    for (int i = 0; i < 30; ++i) g.Update(1.0f / 60.0f);
    assert(g.events.size() == 1);
}

// Game: a coleta de power-up registra POWERUP com o tipo e a posição dele
// (inclusive o cofrinho).
void Test_PowerupPickupEmitsEvent()
{
    Game g = QuietGame();
    g.powerups.push_back(MakePowerUp(PowerUpType::SNEAKERS, 0, 3.0f));
    g.powerups.push_back(MakePowerUp(PowerUpType::PIGGY, 0, 9.0f));
    RunWhile(g, [&] { return g.player.s < 14.0f; });
    // (O cofrinho lança o porco, que solta moedas: filtra só os power-ups.)
    std::vector<CollectEvent> pu;
    for (size_t i = 0; i < g.events.size(); ++i)
        if (g.events[i].kind == CollectKind::POWERUP) pu.push_back(g.events[i]);
    assert(pu.size() == 2);
    assert(pu[0].powerup == PowerUpType::SNEAKERS);
    assert(nearf(pu[0].position.x, 0.0f) && nearf(pu[0].position.y, POWERUP_HEIGHT));
    assert(nearf(pu[0].position.z, 3.0f));
    assert(pu[1].powerup == PowerUpType::PIGGY && nearf(pu[1].position.z, 9.0f));
}

// Reset limpa os eventos; se ninguém os consome a lista não cresce sem limite
// (e não realoca: capacidade reservada).
void Test_EventsResetAndBounded()
{
    Game g = QuietGame();
    Coin c = {0, 3.0f, COIN_HEIGHT, false};
    g.coins.push_back(c);
    RunWhile(g, [&] { return g.coins_collected == 0; });
    assert(!g.events.empty());
    g.Reset();
    assert(g.events.empty());

    const size_t cap = g.events.capacity();
    assert(cap >= COLLECT_EVENT_CAPACITY);
    for (int i = 0; i < 200; ++i) {
        Coin extra = {0, 2.0f + 0.5f * i, COIN_HEIGHT, false};
        g.coins.push_back(extra);
    }
    for (int i = 0; i < 1200 && g.mode == GameMode::PLAYING; ++i) g.Update(1.0f / 60.0f);
    assert(g.coins_collected > (int)COLLECT_EVENT_CAPACITY);
    assert(g.events.size() <= COLLECT_EVENT_CAPACITY);
    assert(g.events.capacity() == cap);
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
    Test_DtSanitized();
    Test_TrackIgnoresNonFiniteS();
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
    Test_SweepAABB();
    Test_SweepAABBAlongZ();
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
    // Rampas e tetos de trem (4.4)
    Test_RampHeightUnderFootprint();
    Test_LandsOnTopBigFall();
    Test_LandsOnTopToleranceBoundary();
    Test_LandsOnTopClimbingIn();
    Test_LandsOnTopEntryBelowRoof();
    Test_CeilingPenetration();
    Test_RampSideBlocks();
    // Obstáculos, moedas e game over (2.2)
    Test_EveryChunkHasPassableLane();
    Test_ChunkBordersAllowLaneChange();
    Test_LowHighNeverTogetherInLane();
    Test_FirstMetersObstacleFree();
    Test_ObstaclesInsideLane();
    Test_ObstacleSizeTableIsSourceOfTruth();
    Test_ObstacleExtentsSetter();
    Test_PlayerBoxRollHeight();
    Test_PlayerBoxMatchesModel();
    Test_HitBarrierGameOver();
    Test_GameOverFrameNoCollectNoScore();
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
    // Trens em movimento (4.3)
    Test_StraightRunStart();
    Test_MovingTrainAdvances();
    Test_MovingTrainStopsAtSegmentEnd();
    Test_MovingTrainStopsBehindObstacle();
    Test_MovingTrainHitAndDodge();
    Test_MovingTrainNoTunneling();
    Test_MovingTrainsSpawnAndStayPassable();
    Test_ObstaclesSameLaneNeverOverlap();
    Test_MovingTrainsDeterministic();
    // Rampas e andar sobre trens (4.4)
    Test_RampGeometry();
    Test_RampLiftsPlayerOntoTrain();
    Test_JumpOntoTrainRoof();
    Test_FallOffTrainEnd();
    Test_FrontalTrainHitIsGameOver();
    Test_RampNeverKills();
    Test_RampSideLowStartClimbs();
    Test_RampSideEntryDeltaY();
    Test_RampTrainJunctionSideEntryConsistent();
    Test_JumpFromRoofNotCancelled();
    Test_RoofLaneChangeToEmptyLaneFalls();
    Test_RoofLaneChangeToAdjacentTrainStays();
    Test_RoofRunMaxSpeedBigDt();
    Test_TunnelCeilingClamp();
    Test_RampSpawnRules();
    Test_RampsDontDisturbSpawn();
    Test_GroundHeightIgnoresJumpArc();
    Test_CameraFollowsGroundHeight();
    Test_FreeCamStartAtRoundTrip();
    Test_FreeCamPitchClamp();
    Test_FreeCamMoveDt();
    Test_PointInsideTunnel();
    // Power-ups (5.2)
    Test_SameTypeRenewsTimer();
    Test_MagnetPullsCoin();
    Test_SneakersHigherJump();
    Test_MultiplierDoublesCoinScore();
    Test_PowerupExpires();
    Test_PowerupTimersFrozenOutsidePlaying();
    Test_PiggyPickupLaunchesPig();
    Test_PowerupOtherLaneNotCollected();
    Test_CameraFollowsHighJump();
    Test_PowerupResetClears();
    Test_PowerupSpawnRules();
    Test_PowerupSpawnDeterministic();
    Test_PowerupDespawnBehind();
    Test_TunnelApproach();
    Test_SneakersTunnelEntryNoPop();
    // Bézier cúbica e porco voador (5.3)
    Test_BezierEndpoints();
    Test_BezierMidpoint();
    Test_BezierTangent();
    Test_PigDropsCoins();
    Test_PigCoinsFallToCoinHeight();
    Test_SinglePigActive();
    Test_PigDtIndependent();
    Test_PigFollowsPlayer();
    Test_PigCoinsAvoidObstacles();
    Test_PigNoSpawnBeforeTunnel();
    Test_PigNoCoinsInsideTunnel();
    Test_PigPauseAndReset();
    // Partículas na coleta (6.1)
    Test_ParticlesExpire();
    Test_CapacityRespected();
    Test_ParticlesNoReallocation();
    Test_ParticlesMoveWithDt();
    Test_ParticlesDeterministic();
    Test_CoinPickupEmitsEvent();
    Test_PowerupPickupEmitsEvent();
    Test_EventsResetAndBounded();
    printf("OK\n");
    return 0;
}
