// Harness de testes ponta a ponta (E2E). Veja include/autotest.h.
//
// COMO ACRESCENTAR UM CENÁRIO (um único lugar: BuildScenarios(), mais abaixo):
//   1. Se o cenário precisa checar o estado do jogo, escreva uma função
//          static bool Check_<nome>(std::string* why)
//      logo acima de BuildScenarios(). Ela devolve false e preenche *why com o
//      motivo quando algo estiver errado. Para ler o estado do jogo, inclua o
//      header do módulo (ex.: "game.h") neste arquivo e use as funções/globais
//      que ele expõe; não é preciso mexer em main.cpp.
//   2. Em BuildScenarios(), acrescente um bloco "scenarios.push_back(...)" com
//      nome, duração (s simulados), a lista de AutotestStep {tempo, tecla,
//      ação, rótulo do screenshot ou NULL} e o final_check (ou NULL).
//      Opcionais: 'setup' (configura g_Game antes do 1º quadro, ex.: SetupNoSpawn
//      para uma corrida que não pode terminar em colisão) e 'frame_hook'
//      (chamado a cada quadro, para registrar estados intermediários).
//   3. Nada mais: tests/run_e2e.sh obtém a lista por "main --autotest-list".
// As teclas são entregues à mesma função HandleKey() usada pelo KeyCallback
// real (nunca ao Correcao_KeyCallback, pois Shift+0..9 encerra o processo).
// Screenshots vão para e2e/<cenario>_<rotulo>.png, relativo a bin/<plataforma>/.

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "autotest.h"
#include "camera.h"
#include "game.h"
#include "scenery.h"

// Definida em main.cpp: o mesmo tratamento de teclas que o KeyCallback usa.
void HandleKey(GLFWwindow* window, int key, int action, int mods);

// Estado do jogo, definido em main.cpp (semente fixa 12345 no autotest).
extern Game g_Game;
// Câmera em 3ª pessoa e rampa de escurecimento do túnel (main.cpp).
extern ThirdPersonCamera g_Camera;
extern float g_TunnelDimming;

// ---------------------------------------------------------------------------
// Cenários
// ---------------------------------------------------------------------------

// setup: corrida sem obstáculos/moedas gerados (cenários de física/câmera que
// não podem terminar em colisão).
static void SetupNoSpawn()
{
    g_Game.spawn_enabled = false;
    g_Game.Reset();
}

// Estados intermediários registrados pelos frame_hooks (um cenário por processo).
static float g_MaxY          = 0.0f;  // maior altura do jogador
static bool  g_SawRoll       = false; // jogador rolando no chão em algum quadro
static bool  g_SawGameOver   = false; // passou por GAME_OVER
static float g_DeathS        = -1.0f; // s do jogador no 1º quadro em GAME_OVER
static bool  g_DeathTouching = false; // nesse quadro, algum obstáculo encostava no jogador

static void Hook_JumpRoll()
{
    const Player& p = g_Game.player;
    g_MaxY = std::max(g_MaxY, p.y);
    if (p.roll_timer > 0.0f && p.grounded)
        g_SawRoll = true;
}

static void Hook_GameOver()
{
    if (g_Game.mode == GameMode::GAME_OVER && !g_SawGameOver)
    {
        g_SawGameOver = true;
        g_DeathS      = g_Game.player.s;
        // A caixa do jogador, alongada para trás pelo passo do quadro (caixa
        // varrida), toca alguma parte de algum obstáculo?
        AABB swept = g_Game.PlayerBox();
        swept.min.z -= PLAYER_MAX_SPEED * Autotest_FixedDt();
        AABB parts[3];
        for (size_t i = 0; i < g_Game.obstacles.size(); ++i)
        {
            int n = ObstacleParts(g_Game.obstacles[i], parts);
            for (int k = 0; k < n; ++k)
                if (AABBIntersectsAABB(swept, parts[k]))
                    g_DeathTouching = true;
        }
    }
}

// lane_jump_roll: troca para a direita, pulo, rolamento e duas vezes para a
// esquerda; no fim o jogador está na faixa -1, no chão, ainda jogando, e o
// pulo e o rolamento de fato aconteceram no meio do caminho.
static bool Check_lane_jump_roll(std::string* why)
{
    char buf[160];
    const Player& p = g_Game.player;
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "modo de jogo diferente de PLAYING";
        return false;
    }
    if (g_MaxY < 1.5f)
    {
        snprintf(buf, sizeof(buf), "o pulo nao aconteceu: altura maxima = %.2f m (esperava >= 1.5)", g_MaxY);
        *why = buf;
        return false;
    }
    if (!g_SawRoll)
    {
        *why = "o rolamento nao aconteceu (roll_timer nunca > 0 no chao)";
        return false;
    }
    if (p.lane != -1 || std::fabs(p.x - (-LANE_WIDTH)) > 1e-3f)
    {
        snprintf(buf, sizeof(buf), "esperava faixa -1 em x = %.2f; faixa = %d, x = %.3f",
                 -LANE_WIDTH, p.lane, p.x);
        *why = buf;
        return false;
    }
    if (p.s <= 60.0f)
    {
        snprintf(buf, sizeof(buf), "jogador nao correu: s = %.2f m (esperava > 60)", p.s);
        *why = buf;
        return false;
    }
    if (p.y != 0.0f || !p.grounded)
    {
        snprintf(buf, sizeof(buf), "jogador nao esta no chao: y = %.4f", p.y);
        *why = buf;
        return false;
    }
    return true;
}

// long_run: 60 s sem entrada. A distância tem de bater com a integração da
// velocidade (aceleração constante, sem atingir a máxima em 60 s).
static bool Check_long_run(std::string* why)
{
    char buf[160];
    const Player& p = g_Game.player;
    float t = g_Game.elapsed;
    float v = std::min(PLAYER_START_SPEED + PLAYER_ACCEL * t, PLAYER_MAX_SPEED);
    float expected = PLAYER_START_SPEED * t + 0.5f * PLAYER_ACCEL * t * t;
    if (g_Game.mode != GameMode::PLAYING || t < 59.9f)
    {
        snprintf(buf, sizeof(buf), "jogo parou antes do fim: tempo de jogo = %.2f s", t);
        *why = buf;
        return false;
    }
    if (p.s <= 700.0f || std::fabs(p.s - expected) > 0.01f * expected ||
        std::fabs(p.speed - v) > 0.01f)
    {
        snprintf(buf, sizeof(buf), "s = %.2f m (esperava %.2f, > 700), v = %.3f (esperava %.3f)",
                 p.s, expected, p.speed, v);
        *why = buf;
        return false;
    }
    return true;
}

// obstacle_types: um obstáculo de cada tipo (e moedas) logo à frente, sem
// spawn, para conferir de perto no screenshot que estão apoiados nos trilhos,
// dentro das faixas e com a textura sem esticar.
static void SetupObstacleTypes()
{
    SetupNoSpawn();
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::BARRIER_LOW, -1, 14.0f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::BARRIER_HIGH, 0, 18.0f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::SIGN, 1, 14.0f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::TRAIN, -1, 24.0f));
    for (int i = 0; i < 4; ++i)
    {
        Coin c = {0, 8.0f + i * COIN_SPACING, COIN_HEIGHT, false};
        g_Game.coins.push_back(c);
    }
}

static bool Check_obstacle_types(std::string* why)
{
    if (g_Game.mode != GameMode::PLAYING || g_Game.obstacles.size() != 4)
    {
        *why = "o jogador bateu ou os obstaculos do cenario sumiram";
        return false;
    }
    return true;
}

// showcase: arranjo fixo, à mão, com um exemplar de cada coisa do jogo à
// vista (barreira baixa, barreira alta, placa, dois trens, moedas), para o
// README/vídeo. Sem spawn: nada muda de uma execução para outra.
static void SetupShowcase()
{
    SetupNoSpawn();
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::BARRIER_LOW, -1, 13.0f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::BARRIER_HIGH, 0, 19.0f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::SIGN, 1, 13.0f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::TRAIN, -1, 30.0f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::TRAIN, 1, 34.0f));
    for (int i = 0; i < 5; ++i)
    {
        Coin c = {0, 6.0f + i * COIN_SPACING, COIN_HEIGHT, false};
        g_Game.coins.push_back(c);
    }
    // Moedas no ar sobre a barreira baixa (o caminho do pulo).
    for (int i = 0; i < 3; ++i)
    {
        Coin c = {-1, 11.0f + i * COIN_SPACING, 2.2f, false};
        g_Game.coins.push_back(c);
    }
}

static bool Check_showcase(std::string* why)
{
    if (g_Game.mode != GameMode::PLAYING || g_Game.obstacles.size() != 5)
    {
        *why = "o jogador bateu ou os obstaculos do showcase sumiram";
        return false;
    }
    return true;
}

// coins: sem entrada, o jogador passa pela fileira de boas-vindas do 1º chunk
// (WELCOME_COIN_COUNT moedas na faixa inicial, antes de qualquer obstáculo) e
// coleta todas. Não depende da semente: o 1º chunk é sempre igual.
static bool Check_coins(std::string* why)
{
    char buf[160];
    float expected = g_Game.player.s + 10.0f * g_Game.multiplier * g_Game.coins_collected;
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "jogo terminou antes de passar pelas moedas";
        return false;
    }
    if (g_Game.coins_collected != WELCOME_COIN_COUNT || std::fabs(g_Game.score - expected) > 0.1f)
    {
        snprintf(buf, sizeof(buf), "moedas = %d (esperava %d), pontos = %.2f (esperava %.2f)",
                 g_Game.coins_collected, WELCOME_COIN_COUNT, g_Game.score, expected);
        *why = buf;
        return false;
    }
    return true;
}

// Confere que a corrida acabou por uma colisão de verdade (depois do trecho
// inicial livre e com um obstáculo de fato junto do jogador, sem "colisão
// fantasma").
static bool CheckDeath(std::string* why)
{
    char buf[160];
    if (!g_SawGameOver)
    {
        *why = "o jogador nunca bateu (sem GAME_OVER em 40 s sem entrada)";
        return false;
    }
    if (g_DeathS < OBSTACLE_FREE_START - 2.0f * PLAYER_HALF_DEPTH)
    {
        snprintf(buf, sizeof(buf), "GAME_OVER cedo demais: s = %.2f m (trecho inicial e livre)", g_DeathS);
        *why = buf;
        return false;
    }
    if (!g_DeathTouching)
    {
        snprintf(buf, sizeof(buf), "GAME_OVER em s = %.2f sem obstaculo encostado no jogador", g_DeathS);
        *why = buf;
        return false;
    }
    return true;
}

// game_over: spawn ligado (semente fixa), sem entrada: o jogador bate em algo
// e o jogo fica parado em GAME_OVER até o fim.
static bool Check_game_over(std::string* why)
{
    if (g_Game.mode != GameMode::GAME_OVER)
    {
        *why = "modo de jogo diferente de GAME_OVER no fim";
        return false;
    }
    return CheckDeath(why);
}

// restart / restart_space: depois do GAME_OVER, Enter (ou Espaço) recomeça a
// corrida do zero; com Espaço, o jogador NÃO pula na corrida nova.
static bool Check_restart(std::string* why)
{
    char buf[160];
    const Player& p = g_Game.player;
    if (!CheckDeath(why))
        return false;
    if (g_Game.mode != GameMode::PLAYING || p.s > 8.0f || g_Game.score > 8.0f ||
        g_Game.coins_collected != 0)
    {
        snprintf(buf, sizeof(buf), "nao reiniciou: modo %d, s = %.2f, pontos = %.2f, moedas = %d",
                 (int)g_Game.mode, p.s, g_Game.score, g_Game.coins_collected);
        *why = buf;
        return false;
    }
    if (!p.grounded || p.y != 0.0f)
    {
        snprintf(buf, sizeof(buf), "a tecla de reinicio virou pulo: y = %.3f", p.y);
        *why = buf;
        return false;
    }
    return true;
}

// Primeiro segmento da pista 'track' que é curva (want_tunnel = false) ou
// túnel (true), gerando a pista aos poucos até achá-lo. Devolve uma cópia (o
// vetor de segmentos realoca ao crescer). Os cenários de curva e de túnel
// tiram daqui as posições, em vez de fixar valores de s ligados à semente:
// continuam válidos se o gerador mudar.
static TrackSegment FindFirstSegment(Track* track, bool want_tunnel)
{
    for (float s = 200.0f; s <= 50000.0f; s += 200.0f)
    {
        track->EnsureGeneratedUntil(s);
        const std::vector<TrackSegment>& segs = track->Segments();
        for (size_t i = 0; i < segs.size(); ++i)
            if (want_tunnel ? segs[i].tunnel : segs[i].curvature != 0.0f)
                return segs[i];
    }
    fprintf(stderr, "E2E: nenhum segmento %s em 50 km de pista\n", want_tunnel ? "de tunel" : "curvo");
    std::exit(1);
}

// Tempo (s) para o jogador andar d metros a partir da velocidade inicial,
// com aceleração constante (sem chegar à máxima): d = v0 t + a t²/2.
static float TimeToRun(float d)
{
    const float v0 = PLAYER_START_SPEED, a = PLAYER_ACCEL;
    return (-v0 + std::sqrt(v0 * v0 + 2.0f * a * d)) / a;
}

// curve: sem spawn, jogador CURVE_LEAD m antes da 1ª curva da pista (achada
// em tempo de execução). Fotos na entrada, no meio (logo depois de trocar de
// faixa dentro da curva) e na saída. Os tempos servem para qualquer curva do
// gerador (45° a 90°, 31 a 63 m): a troca de faixa termina ~6 m depois do
// início da curva e a foto/checagem final é ~13 m depois do fim da mais longa.
// O frame_hook registra se o jogador esteve num segmento curvo e se a troca
// de faixa terminou dentro dele.
static const float CURVE_LEAD = 24.0f;
static bool  g_SawCurve        = false; // jogador num segmento com curvature != 0
static bool  g_LaneChangeInCurve = false; // chegou na faixa 1 (x = LANE_WIDTH) dentro da curva
static float g_StartHeading      = 0.0f;  // heading do referencial do jogador no início
static float g_MaxHeadingChange  = 0.0f;  // maior |heading - inicial| (rad)

static void SetupCurve()
{
    SetupNoSpawn();
    TrackSegment c = FindFirstSegment(&g_Game.track, false);
    g_Game.player.s = c.s_start - CURVE_LEAD;
    g_StartHeading = g_Game.track.FrameAt(g_Game.player.s).heading;
}

static void Hook_Curve()
{
    const Player& p = g_Game.player;
    const TrackSegment* seg = g_Game.track.SegmentAt(p.s);
    bool in_curve = seg != NULL && seg->curvature != 0.0f;
    if (in_curve)
    {
        g_SawCurve = true;
        if (std::fabs(p.x - LANE_WIDTH) < 1e-3f)
            g_LaneChangeInCurve = true;
    }
    g_MaxHeadingChange = std::max(g_MaxHeadingChange,
                                  std::fabs(g_Game.track.FrameAt(p.s).heading - g_StartHeading));
}

static bool Check_curve(std::string* why)
{
    char buf[200];
    const Player& p = g_Game.player;
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "modo de jogo diferente de PLAYING";
        return false;
    }
    if (!g_SawCurve || g_MaxHeadingChange < 0.785398f - 1e-3f) // 45°
    {
        snprintf(buf, sizeof(buf), "nao passou por curva: viu curva = %d, heading max = %.1f graus",
                 (int)g_SawCurve, g_MaxHeadingChange * 57.29578f);
        *why = buf;
        return false;
    }
    if (!g_LaneChangeInCurve || p.lane != 1)
    {
        snprintf(buf, sizeof(buf), "troca de faixa na curva falhou: faixa = %d, x = %.3f", p.lane, p.x);
        *why = buf;
        return false;
    }
    const TrackSegment* seg = g_Game.track.SegmentAt(p.s);
    if (seg == NULL || seg->curvature != 0.0f)
    {
        *why = "no fim o jogador deveria estar na reta depois da curva";
        return false;
    }
    return true;
}

// curve_obstacles: sem spawn, jogador colocado pouco antes da 1ª curva (10.5 m),
// com barreiras, placa e moedas posicionadas DENTRO da curva (até 25.5 m
// depois do início; a curva mais curta tem 31 m) e um trem na reta seguinte:
// todos devem acompanhar a direção local da pista.
static void SetupCurveObstacles()
{
    SetupNoSpawn();
    TrackSegment c = FindFirstSegment(&g_Game.track, false);
    const float c0 = c.s_start, c1 = c.s_start + c.length;
    g_Game.player.s = c0 - 10.5f;
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::BARRIER_LOW, -1, c0 + 5.5f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::SIGN, 1, c0 + 9.5f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::BARRIER_HIGH, 0, c0 + 17.5f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::BARRIER_LOW, 1, c0 + 25.5f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::TRAIN, 0, c1 + 16.0f));
    for (int i = 0; i < 8; ++i)
    {
        Coin coin = {0, c0 - 4.5f + i * COIN_SPACING, COIN_HEIGHT, false};
        g_Game.coins.push_back(coin);
    }
}

// tunnel_side: sem spawn, jogador no fim da curva que antecede a reta de
// chegada do 1º túnel (o gerador sempre faz curva, reta, túnel): o portal
// visto de longe, saindo da curva, para conferir que ele se destaca na rua e
// que não há prédios nem postes encostados nele. (A casca externa da abóbada
// fica escondida atrás do portal e dos prédios: de dentro da curva os
// prédios a encobrem.)
static void SetupTunnelSide()
{
    SetupNoSpawn();
    TrackSegment t = FindFirstSegment(&g_Game.track, true);
    const std::vector<TrackSegment>& segs = g_Game.track.Segments();
    for (size_t i = 2; i < segs.size(); ++i)
        if (segs[i].s_start == t.s_start)
        {
            const TrackSegment& curve = segs[i - 2];
            g_Game.player.s = curve.s_start + curve.length - 12.0f;
            return;
        }
}

// tunnel: sem spawn, jogador TUNNEL_LEAD m antes do 1º túnel da pista (achado
// em tempo de execução), um trem parado na faixa -1 dentro do túnel e troca
// para a faixa 1 (junto à parede) lá dentro. Fotos chegando ao portal, na
// entrada (câmera ainda fora), no meio, perto da saída e depois dela. O
// frame_hook mede a altura da câmera (antes, dentro, depois), o maior salto
// por quadro da altura e do escurecimento, e a folga entre a câmera e o teto/
// parede sempre que ela está no túnel ou no vão de um portal.
static const float TUNNEL_LEAD = 30.0f;
static float g_TunnelS0 = 0.0f, g_TunnelS1 = 0.0f;    // trecho do túnel
static float g_CamHeightBefore   = -1.0f;  // altura da câmera antes do jogador entrar
static float g_CamHeightMinInside = 1e9f;  // menor altura com o jogador no túnel
static float g_PrevCamHeight     = -1.0f;
static float g_MaxCamHeightStep  = 0.0f;   // maior variação da altura entre quadros
static float g_PrevDimming       = -1.0f;
static float g_MaxDimmingStep    = 0.0f;   // maior variação do escurecimento entre quadros
static float g_MaxDimming        = 0.0f;
static float g_MinCeilingGap     = 1e9f;   // menor (teto - câmera) no túnel/portal
static float g_MaxCamLateral     = 0.0f;   // maior |lateral| da câmera no túnel/portal
static bool  g_PlayerWasInTunnel = false;

static void SetupTunnel()
{
    SetupNoSpawn();
    TrackSegment t = FindFirstSegment(&g_Game.track, true);
    g_TunnelS0 = t.s_start;
    g_TunnelS1 = t.s_start + t.length;
    g_Game.player.s = g_TunnelS0 - TUNNEL_LEAD;
    // Trem parado na faixa externa esquerda, logo depois do meio do túnel.
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::TRAIN, -1,
                                                   0.5f * (g_TunnelS0 + g_TunnelS1) + 8.0f));
}

static void Hook_Tunnel()
{
    const Track& track = g_Game.track;
    const Player& p = g_Game.player;
    const float h = g_Camera.Height();
    const float cam_s = p.s - g_Camera.Back();

    if (g_PrevCamHeight >= 0.0f)
        g_MaxCamHeightStep = std::max(g_MaxCamHeightStep, std::fabs(h - g_PrevCamHeight));
    g_PrevCamHeight = h;
    if (g_PrevDimming >= 0.0f)
        g_MaxDimmingStep = std::max(g_MaxDimmingStep, std::fabs(g_TunnelDimming - g_PrevDimming));
    g_PrevDimming = g_TunnelDimming;
    g_MaxDimming = std::max(g_MaxDimming, g_TunnelDimming);

    if (track.IsTunnelAt(p.s))
    {
        g_PlayerWasInTunnel = true;
        g_CamHeightMinInside = std::min(g_CamHeightMinInside, h);
    }
    else if (!g_PlayerWasInTunnel)
        g_CamHeightBefore = h;

    // Câmera dentro do túnel ou atravessando um portal (com a moldura):
    // altura e lateral (no referencial da pista no ponto da câmera) têm de
    // caber no vão.
    const float margin = TUNNEL_PORTAL_DEPTH + 0.5f;
    if (cam_s >= g_TunnelS0 - margin && cam_s <= g_TunnelS1 + margin)
    {
        const CameraPose pose = g_Camera.Pose();
        const TrackFrame f = track.FrameAt(cam_s);
        const glm::vec4 d = pose.position - f.position;
        const float lateral = d.x * f.right.x + d.z * f.right.z;
        g_MaxCamLateral = std::max(g_MaxCamLateral, std::fabs(lateral));
        g_MinCeilingGap = std::min(g_MinCeilingGap, TunnelCeilingHeight(lateral) - d.y);
    }
}

static bool Check_tunnel(std::string* why)
{
    char buf[240];
    const Player& p = g_Game.player;
    const float dt = Autotest_FixedDt();
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "o jogador bateu (modo diferente de PLAYING)";
        return false;
    }
    if (!g_PlayerWasInTunnel || p.s < g_TunnelS1 + 10.0f || p.lane != 1)
    {
        snprintf(buf, sizeof(buf), "nao atravessou o tunel [%.1f, %.1f] na faixa 1: s = %.1f, faixa = %d",
                 g_TunnelS0, g_TunnelS1, p.s, p.lane);
        *why = buf;
        return false;
    }
    // Câmera mais baixa dentro do túnel e de volta à altura normal depois.
    if (!(g_CamHeightMinInside < g_CamHeightBefore - 1.0f) ||
        std::fabs(g_CamHeightMinInside - CAMERA_TUNNEL_HEIGHT) > 0.05f ||
        std::fabs(g_Camera.Height() - CAMERA_HEIGHT) > 0.05f)
    {
        snprintf(buf, sizeof(buf), "altura da camera: antes %.2f, dentro (min) %.2f, depois %.2f",
                 g_CamHeightBefore, g_CamHeightMinInside, g_Camera.Height());
        *why = buf;
        return false;
    }
    // Transições suaves: a altura anda no máximo a fração (1 - e^(-6 dt)) do
    // desnível por quadro; o escurecimento, dt / TUNNEL_DIM_TIME.
    const float max_h_step = (CAMERA_HEIGHT - CAMERA_TUNNEL_HEIGHT) *
                             (1.0f - std::exp(-CAMERA_SMOOTHING_RATE * dt)) + 1e-3f;
    if (g_MaxCamHeightStep > max_h_step || g_MaxDimmingStep > dt / TUNNEL_DIM_TIME + 1e-4f)
    {
        snprintf(buf, sizeof(buf), "salto entre quadros: altura %.3f m (max %.3f), escurecimento %.3f",
                 g_MaxCamHeightStep, max_h_step, g_MaxDimmingStep);
        *why = buf;
        return false;
    }
    if (g_MaxDimming < 1.0f || g_TunnelDimming != 0.0f)
    {
        snprintf(buf, sizeof(buf), "escurecimento: maximo %.2f (esperava 1), no fim %.2f (esperava 0)",
                 g_MaxDimming, g_TunnelDimming);
        *why = buf;
        return false;
    }
    // A câmera nunca atravessa o teto nem a parede (folga de 1 m no teto).
    if (g_MinCeilingGap < 1.0f || g_MaxCamLateral > TUNNEL_INNER_HALF_WIDTH - 1.0f)
    {
        snprintf(buf, sizeof(buf), "camera perto demais do teto/parede: folga %.2f m, lateral %.2f m",
                 g_MinCeilingGap, g_MaxCamLateral);
        *why = buf;
        return false;
    }
    return true;
}

// moving_train / moving_train_hit: sem spawn, um trem móvel (MOVING_TRAIN_SPEED)
// na faixa do jogador, vindo na direção dele pela reta da largada. No
// primeiro o jogador desvia de faixa (seta direita) com folga e o trem passa;
// no segundo não faz nada e bate, no instante previsto pela cinemática
// (jogador a PLAYER_START_SPEED + PLAYER_ACCEL, trem a -MOVING_TRAIN_SPEED).
static float g_TrainS0   = 0.0f;   // s inicial do trem
static float g_TrainMinS = 1e9f;   // menor s do trem observado
static bool  g_TrainGone = false;  // o trem foi descartado (ficou para trás do jogador)

static void SetupMovingTrain()
{
    SetupNoSpawn();
    // O trem nasce junto ao fim da reta da largada (a mais longa possível).
    g_TrainS0 = g_Game.track.Segments()[0].length - 8.0f;
    g_Game.obstacles.push_back(g_Game.MakeMovingTrain(0, g_TrainS0, -MOVING_TRAIN_SPEED));
}

static void Hook_MovingTrain()
{
    if (g_Game.obstacles.empty())
    {
        g_TrainGone = true;
        return;
    }
    g_TrainMinS = std::min(g_TrainMinS, g_Game.obstacles[0].s);
}

// Instante (s) em que o nariz do trem alcança a frente do jogador, resolvendo
// v0 t + a t²/2 + meia profundidade = (s0 - meio comprimento) - 8 t.
static float ExpectedTrainHitTime()
{
    const float hz  = 0.5f * OBSTACLE_SIZE[(int)ObstacleType::TRAIN][2];
    const float gap = g_TrainS0 - hz - PLAYER_HALF_DEPTH;
    const float v   = PLAYER_START_SPEED + MOVING_TRAIN_SPEED, a = 0.5f * PLAYER_ACCEL;
    return (-v + std::sqrt(v * v + 4.0f * a * gap)) / (2.0f * a);
}

static bool Check_moving_train(std::string* why)
{
    char buf[200];
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "o jogador bateu apesar de desviar de faixa";
        return false;
    }
    if (g_Game.player.lane != 1)
    {
        *why = "o jogador nao trocou de faixa";
        return false;
    }
    if (g_TrainMinS > g_TrainS0 - 20.0f)
    {
        snprintf(buf, sizeof(buf), "o trem quase nao andou: s inicial %.1f, menor s %.1f", g_TrainS0, g_TrainMinS);
        *why = buf;
        return false;
    }
    if (!g_TrainGone && g_Game.obstacles[0].s > g_Game.player.s)
    {
        *why = "o trem nao passou pelo jogador";
        return false;
    }
    return true;
}

static bool Check_moving_train_hit(std::string* why)
{
    char buf[200];
    if (g_Game.mode != GameMode::GAME_OVER)
    {
        *why = "o jogador nao bateu no trem que vinha na faixa dele";
        return false;
    }
    const float expected = ExpectedTrainHitTime();
    if (std::fabs(g_Game.elapsed - expected) > 0.25f)
    {
        snprintf(buf, sizeof(buf), "bateu em t = %.2f s, esperava %.2f s", g_Game.elapsed, expected);
        *why = buf;
        return false;
    }
    if (g_TrainMinS > g_TrainS0 - 10.0f)
    {
        *why = "o trem nao andou ate o contato";
        return false;
    }
    return true;
}

static std::vector<AutotestScenario> BuildScenarios()
{
    std::vector<AutotestScenario> scenarios;

    // smoke: 3 s de simulação, uma foto em 1 s, sem erro de GL.
    {
        AutotestScenario s;
        s.name        = "smoke";
        s.duration    = 3.0f;
        s.final_check = NULL;
        s.steps.push_back(AutotestStep{1.0f, 0, 0, "t1"});
        scenarios.push_back(s);
    }

    // lane_jump_roll: controles do jogador vistos pela câmera em 3ª pessoa.
    {
        AutotestScenario s;
        s.name        = "lane_jump_roll";
        s.duration    = 6.0f;
        s.final_check = Check_lane_jump_roll;
        s.setup       = SetupNoSpawn;
        s.frame_hook  = Hook_JumpRoll;
        s.steps.push_back(AutotestStep{0.5f, 0, 0, "start"});
        s.steps.push_back(AutotestStep{1.0f, GLFW_KEY_RIGHT, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{1.6f, 0, 0, "right"});
        s.steps.push_back(AutotestStep{2.5f, GLFW_KEY_UP, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{2.8f, 0, 0, "jump"});
        s.steps.push_back(AutotestStep{4.0f, GLFW_KEY_DOWN, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{4.2f, 0, 0, "roll"});
        s.steps.push_back(AutotestStep{5.0f, GLFW_KEY_LEFT, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{5.0f, GLFW_KEY_A, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{5.9f, 0, 0, "left2"});
        scenarios.push_back(s);
    }

    // long_run: 60 s de corrida sem entrada (estoura buffer das faixas? vaza
    // recursos? erro de GL?), com fotos no meio e no fim.
    {
        AutotestScenario s;
        s.name        = "long_run";
        s.duration    = 60.0f;
        s.final_check = Check_long_run;
        s.setup       = SetupNoSpawn;
        s.steps.push_back(AutotestStep{30.0f, 0, 0, "t30"});
        s.steps.push_back(AutotestStep{59.5f, 0, 0, "t60"});
        scenarios.push_back(s);
    }

    // obstacle_types: close-up de cada tipo de obstáculo e das moedas.
    {
        AutotestScenario s;
        s.name        = "obstacle_types";
        s.duration    = 0.5f;
        s.final_check = Check_obstacle_types;
        s.setup       = SetupObstacleTypes;
        s.steps.push_back(AutotestStep{0.1f, 0, 0, "closeup"});
        scenarios.push_back(s);
    }

    // coins: fileira de moedas de boas-vindas coletada sem entrada.
    {
        AutotestScenario s;
        s.name        = "coins";
        s.duration    = 3.0f;
        s.final_check = Check_coins;
        s.steps.push_back(AutotestStep{0.6f, 0, 0, "ahead"});
        s.steps.push_back(AutotestStep{2.0f, 0, 0, "collected"});
        scenarios.push_back(s);
    }

    // lighting: spawn ligado (semente fixa), sem entrada; fotos em 2 s e 6 s para
    // conferir a iluminação Blinn-Phong: faces voltadas para a câmera iluminadas,
    // brilho especular em moedas/obstáculos, cabeças dos postes acesas e o chão
    // clareado perto delas. Sem verificação de estado (só erros de GL).
    {
        AutotestScenario s;
        s.name        = "lighting";
        s.duration    = 6.5f;
        s.final_check = NULL;
        s.steps.push_back(AutotestStep{2.0f, 0, 0, "t2"});
        s.steps.push_back(AutotestStep{6.0f, 0, 0, "t6"});
        scenarios.push_back(s);
    }

    // showcase: um exemplar de cada obstáculo, trens e moedas, em arranjo fixo.
    {
        AutotestScenario s;
        s.name        = "showcase";
        s.duration    = 0.5f;
        s.final_check = Check_showcase;
        s.setup       = SetupShowcase;
        s.steps.push_back(AutotestStep{0.1f, 0, 0, "scene"});
        s.steps.push_back(AutotestStep{0.35f, GLFW_KEY_UP, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{0.45f, 0, 0, "jump"});
        scenarios.push_back(s);
    }

    // showcase_props: modelos dos power-ups (ímã, tênis, x2, cofrinho) e do
    // porco voador, que só entram no jogo nas Tarefas 5.2/5.3.
    {
        AutotestScenario s;
        s.name        = "showcase_props";
        s.duration    = 0.3f;
        s.final_check = NULL;
        s.setup       = SetupNoSpawn;
        s.show_props  = true;
        s.steps.push_back(AutotestStep{0.1f, 0, 0, "props"});
        scenarios.push_back(s);
    }

    // game_over: obstáculos gerados com a semente fixa, sem entrada, até bater.
    {
        AutotestScenario s;
        s.name        = "game_over";
        s.duration    = 40.0f;
        s.final_check = Check_game_over;
        s.frame_hook  = Hook_GameOver;
        s.steps.push_back(AutotestStep{2.0f, 0, 0, "obstacles"});
        s.steps.push_back(AutotestStep{39.5f, 0, 0, "end"});
        scenarios.push_back(s);
    }

    // restart: como game_over, e no fim Enter recomeça a corrida.
    {
        AutotestScenario s;
        s.name        = "restart";
        s.duration    = 40.0f;
        s.final_check = Check_restart;
        s.frame_hook  = Hook_GameOver;
        s.steps.push_back(AutotestStep{39.5f, GLFW_KEY_ENTER, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{39.9f, 0, 0, "after"});
        scenarios.push_back(s);
    }

    // restart_space: idem com Espaço (que em PLAYING é pulo).
    {
        AutotestScenario s;
        s.name        = "restart_space";
        s.duration    = 40.0f;
        s.final_check = Check_restart;
        s.frame_hook  = Hook_GameOver;
        s.steps.push_back(AutotestStep{39.5f, GLFW_KEY_SPACE, GLFW_PRESS, NULL});
        scenarios.push_back(s);
    }

    // moving_train: trem móvel na faixa do jogador; ele desvia e o trem passa.
    {
        AutotestScenario s;
        s.name        = "moving_train";
        s.duration    = 8.0f;
        s.final_check = Check_moving_train;
        s.setup       = SetupMovingTrain;
        s.frame_hook  = Hook_MovingTrain;
        s.steps.push_back(AutotestStep{0.3f, 0, 0, "far"});
        s.steps.push_back(AutotestStep{2.0f, 0, 0, "approach"});
        s.steps.push_back(AutotestStep{2.6f, GLFW_KEY_RIGHT, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.2f, 0, 0, "dodge"});
        s.steps.push_back(AutotestStep{5.6f, 0, 0, "pass"});
        s.steps.push_back(AutotestStep{6.8f, 0, 0, "alongside"});
        scenarios.push_back(s);
    }

    // moving_train_hit: o mesmo trem, sem desvio: bate no instante previsto.
    {
        AutotestScenario s;
        s.name        = "moving_train_hit";
        s.duration    = 8.0f;
        s.final_check = Check_moving_train_hit;
        s.setup       = SetupMovingTrain;
        s.frame_hook  = Hook_MovingTrain;
        s.steps.push_back(AutotestStep{2.0f, 0, 0, "approach"});
        s.steps.push_back(AutotestStep{7.5f, 0, 0, "gameover"});
        scenarios.push_back(s);
    }

    // curve: atravessa a 1ª curva da pista trocando de faixa no meio dela.
    {
        AutotestScenario s;
        s.name        = "curve";
        s.duration    = 8.0f;
        s.final_check = Check_curve;
        s.setup       = SetupCurve;
        s.frame_hook  = Hook_Curve;
        s.steps.push_back(AutotestStep{1.0f, 0, 0, "entering"});
        s.steps.push_back(AutotestStep{2.3f, GLFW_KEY_RIGHT, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.0f, 0, 0, "mid"});
        s.steps.push_back(AutotestStep{7.5f, 0, 0, "exiting"});
        scenarios.push_back(s);
    }

    // curve_obstacles: obstáculos e moedas dentro da curva, vistos de perto.
    {
        AutotestScenario s;
        s.name        = "curve_obstacles";
        s.duration    = 0.4f;
        s.final_check = NULL;
        s.setup       = SetupCurveObstacles;
        s.steps.push_back(AutotestStep{0.1f, 0, 0, "scene"});
        scenarios.push_back(s);
    }

    // tunnel: atravessa o 1º túnel da pista. Os tempos das fotos dependem do
    // comprimento desse túnel: ele é achado numa CÓPIA da pista de g_Game
    // (mesma semente; já construída, pois g_Game é global), sem alterá-la.
    {
        Track probe = g_Game.track;
        const TrackSegment t = FindFirstSegment(&probe, true);
        const float L = t.length;
        AutotestScenario s;
        s.name        = "tunnel";
        s.final_check = Check_tunnel;
        s.setup       = SetupTunnel;
        s.frame_hook  = Hook_Tunnel;
        s.steps.push_back(AutotestStep{TimeToRun(TUNNEL_LEAD - 18.0f), 0, 0, "approach"});
        s.steps.push_back(AutotestStep{TimeToRun(TUNNEL_LEAD - 1.0f), 0, 0, "portal"});
        s.steps.push_back(AutotestStep{TimeToRun(TUNNEL_LEAD + 3.0f), 0, 0, "entering"});
        s.steps.push_back(AutotestStep{TimeToRun(TUNNEL_LEAD + 0.5f * L) - 0.6f, GLFW_KEY_RIGHT, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{TimeToRun(TUNNEL_LEAD + 0.5f * L), 0, 0, "inside"});
        s.steps.push_back(AutotestStep{TimeToRun(TUNNEL_LEAD + L - 3.0f), 0, 0, "exit"});
        s.steps.push_back(AutotestStep{TimeToRun(TUNNEL_LEAD + L + 3.0f), 0, 0, "outside"});
        const float t_after = TimeToRun(TUNNEL_LEAD + L + 30.0f);
        s.steps.push_back(AutotestStep{t_after, 0, 0, "after"});
        s.duration = t_after + 0.3f;
        scenarios.push_back(s);
    }

    // tunnel_side: portal e abóbada vistos de viés, da curva antes do túnel.
    {
        AutotestScenario s;
        s.name        = "tunnel_side";
        s.duration    = 0.3f;
        s.final_check = NULL;
        s.setup       = SetupTunnelSide;
        s.steps.push_back(AutotestStep{0.1f, 0, 0, "view"});
        scenarios.push_back(s);
    }

    return scenarios;
}

// ---------------------------------------------------------------------------
// Estado do harness
// ---------------------------------------------------------------------------

static const float AUTOTEST_DT = 1.0f / 60.0f;

static bool             g_Active   = false;
static int              g_ExitCode = 0;
static bool             g_Finished = false;
static AutotestScenario g_Scenario;
static size_t           g_NextStep = 0;

bool  Autotest_Active()    { return g_Active; }
bool  Autotest_ShowProps() { return g_Active && g_Scenario.show_props; }
float Autotest_FixedDt()   { return AUTOTEST_DT; }

void Autotest_Start()
{
    if (g_Active && g_Scenario.setup != NULL)
        g_Scenario.setup();
}

// Se a janela foi fechada antes do fim do cenário (ESC, botão de fechar,
// crash do contexto...), conta como falha e avisa no mesmo formato.
int Autotest_ExitCode()
{
    if (g_Active && !g_Finished)
    {
        printf("E2E %s FAIL: janela fechada antes do fim\n", g_Scenario.name);
        fflush(stdout);
        return 1;
    }
    return g_ExitCode;
}

bool Autotest_Begin(int argc, char* argv[])
{
    std::vector<AutotestScenario> scenarios = BuildScenarios();

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--autotest-list") == 0)
        {
            for (size_t k = 0; k < scenarios.size(); ++k)
                printf("%s\n", scenarios[k].name);
            fflush(stdout);
            std::exit(0);
        }

        if (strcmp(argv[i], "--autotest") == 0)
        {
            const char* name = (i + 1 < argc) ? argv[i + 1] : "";
            for (size_t k = 0; k < scenarios.size(); ++k)
            {
                if (strcmp(scenarios[k].name, name) == 0)
                {
                    g_Scenario = scenarios[k];
                    g_Active   = true;
                    return true;
                }
            }
            printf("E2E %s FAIL: cenario desconhecido (disponiveis:", name);
            for (size_t k = 0; k < scenarios.size(); ++k)
                printf(" %s", scenarios[k].name);
            printf(")\n");
            fflush(stdout);
            std::exit(1);
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Execução
// ---------------------------------------------------------------------------

static void Finish(GLFWwindow* window, bool ok, const std::string& why)
{
    g_Finished = true;
    g_ExitCode = ok ? 0 : 1;
    if (ok)
        printf("E2E %s OK\n", g_Scenario.name);
    else
        printf("E2E %s FAIL: %s\n", g_Scenario.name, why.c_str());
    fflush(stdout);
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// Salva o back buffer (já desenhado, antes do swap) em e2e/<cenario>_<rotulo>.png.
static bool SaveScreenshot(GLFWwindow* window, const char* label, std::string* why)
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    if (w <= 0 || h <= 0)
    {
        *why = "framebuffer com tamanho invalido";
        return false;
    }

    std::vector<unsigned char> pixels((size_t)w * h * 3);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, &pixels[0]);

#ifdef _WIN32
    _mkdir("e2e");
#else
    mkdir("e2e", 0755);
#endif

    std::string path = std::string("e2e/") + g_Scenario.name + "_" + label + ".png";
    stbi_flip_vertically_on_write(1); // glReadPixels começa pela linha de baixo
    if (!stbi_write_png(path.c_str(), w, h, 3, &pixels[0], w * 3))
    {
        *why = "falha ao gravar " + path;
        return false;
    }
    printf("E2E screenshot: %s\n", path.c_str());
    return true;
}

// Esvazia TODOS os erros de GL pendentes (glGetError devolve um por vez).
// Devolve false e descreve os erros em *why se havia algum.
static bool DrainGlErrors(float sim_time, std::string* why)
{
    std::string codes;
    // Limite de segurança: sem contexto válido, glGetError pode não "esvaziar".
    for (int i = 0; i < 64; ++i)
    {
        GLenum err = glGetError();
        if (err == GL_NO_ERROR)
            break;
        char buf[16];
        snprintf(buf, sizeof(buf), "%s0x%04X", codes.empty() ? "" : ", ", (unsigned)err);
        codes += buf;
    }
    if (codes.empty())
        return true;
    char buf[64];
    snprintf(buf, sizeof(buf), " no tempo %.3f s", sim_time);
    *why = "erro de GL " + codes + buf;
    return false;
}

void Autotest_AfterFrame(GLFWwindow* window, float sim_time)
{
    if (!g_Active || g_Finished)
        return;

    std::string why;

    // Erros de GL gerados neste quadro (ou antes, desde o último glGetError).
    if (!DrainGlErrors(sim_time, &why))
    {
        Finish(window, false, why);
        return;
    }

    if (g_Scenario.frame_hook != NULL)
        g_Scenario.frame_hook();

    // Executa os passos vencidos (teclas e/ou screenshot).
    while (g_NextStep < g_Scenario.steps.size() &&
           g_Scenario.steps[g_NextStep].time <= sim_time)
    {
        const AutotestStep& step = g_Scenario.steps[g_NextStep++];
        if (step.key != 0)
            HandleKey(window, step.key, step.action, 0);
        if (step.screenshot != NULL && !SaveScreenshot(window, step.screenshot, &why))
        {
            Finish(window, false, why);
            return;
        }
    }

    if (sim_time >= g_Scenario.duration)
    {
        // Teclas injetadas neste último quadro podem ter gerado erros de GL
        // (ex.: R recarrega shaders): checa de novo antes de encerrar.
        if (!DrainGlErrors(sim_time, &why))
        {
            Finish(window, false, why);
            return;
        }
        // Passos com tempo além da duração nunca rodariam: erro no cenário.
        if (g_NextStep < g_Scenario.steps.size())
        {
            char buf[96];
            snprintf(buf, sizeof(buf), "passos nao executados (%d, o primeiro em %.3f s > duracao %.3f s)",
                     (int)(g_Scenario.steps.size() - g_NextStep),
                     g_Scenario.steps[g_NextStep].time, g_Scenario.duration);
            Finish(window, false, buf);
            return;
        }
        if (g_Scenario.final_check != NULL && !g_Scenario.final_check(&why))
        {
            Finish(window, false, why);
            return;
        }
        Finish(window, true, "");
    }
}
