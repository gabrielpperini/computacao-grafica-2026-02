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
//      para uma corrida que não pode terminar em colisão), 'frame_hook'
//      (chamado a cada quadro, para registrar estados intermediários) e
//      'start_in_menu' (começa no MENU em vez de PLAYING). Botões da GUI são
//      clicados pelo rótulo: AutotestStep{t, 0, 0, NULL, 0, 0, "Jogar"}.
//   3. Nada mais: tests/run_e2e.sh obtém a lista por "main --autotest-list".
// As teclas são entregues à mesma função HandleKey() usada pelo KeyCallback
// real (nunca ao Correcao_KeyCallback, pois Shift+0..9 encerra o processo).
// Screenshots vão para e2e/<cenario>_<rotulo>.png, relativo a bin/<plataforma>/.

#include <glad/glad.h>

#include <algorithm>
#include <cctype>
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
#include "particles.h"
#include "scenery.h"

// Definida em main.cpp: o mesmo tratamento de teclas que o KeyCallback usa.
void HandleKey(GLFWwindow* window, int key, int action, int mods);
// Deslocamento do mouse em pixels (câmera livre), também definido em main.cpp.
void FreeCameraMouseLook(float dx_pixels, float dy_pixels);
// Clique no botão da GUI com esse rótulo (main.cpp; false se não está na tela).
bool Gui_ClickButton(const char* label);
// O botão foi desenhado no último quadro, inteiro dentro da janela (main.cpp).
bool Gui_ButtonOnScreen(const char* label);

// O que os callbacks de foco/minimização fazem ao perder o foco (main.cpp).
void HandleWindowFocusLost(GLFWwindow* window);

// Dispara as partículas dos eventos de coleta e limpa a lista (main.cpp).
void EmitCollectParticles(Game& game);
// Estado do jogo, definido em main.cpp (semente fixa 12345 no autotest).
extern Game g_Game;
// Câmera em 3ª pessoa e rampa de escurecimento do túnel (main.cpp).
extern ThirdPersonCamera g_Camera;
// Câmera livre e teclas seguradas (main.cpp).
extern FreeCamera g_FreeCamera;
extern bool g_KeyHeld[];
extern float g_TunnelDimming;
// Partículas da coleta (main.cpp).
extern ParticleSystem g_Particles;

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

// powerups (Tarefa 5.2): ímã, multiplicador e tênis em fila na faixa do
// jogador (o cofrinho tem o cenário próprio 'piggy', 5.3: o porco soltaria
// moedas que bagunçariam a contagem daqui); moedas nas faixas VIZINHAS (só o
// ímã as alcança), em seguida duas na faixa dele bem depois de os efeitos
// acabarem. Sem obstáculos.
static const int PU_SIDE_COINS = 12; // 6 em cada faixa vizinha
static void SetupPowerups()
{
    SetupNoSpawn();
    const PowerUp magnet = {PowerUpType::MAGNET, 0, 8.0f, false};
    const PowerUp mult   = {PowerUpType::MULTIPLIER, 0, 14.0f, false};
    const PowerUp shoes  = {PowerUpType::SNEAKERS, 0, 20.0f, false};
    g_Game.powerups.push_back(magnet);
    g_Game.powerups.push_back(mult);
    g_Game.powerups.push_back(shoes);
    for (int i = 0; i < 6; ++i)
        for (int lane = -1; lane <= 1; lane += 2)
        {
            Coin c = {lane, 34.0f + i * COIN_SPACING, COIN_HEIGHT, false};
            g_Game.coins.push_back(c);
        }
    // Depois dos 10 s dos efeitos (o jogador está em ~s = 170 aos 14 s).
    for (int i = 0; i < 2; ++i)
    {
        Coin c = {0, 190.0f + i * COIN_SPACING, COIN_HEIGHT, false};
        g_Game.coins.push_back(c);
    }
}

static int   g_PuPrevCoins   = 0;
static float g_PuPrevScore   = 0.0f;
static float g_PuPrevS       = 0.0f;
static int   g_PuDoubled     = 0;     // moedas que valeram 20 pontos
static int   g_PuSingle      = 0;     // moedas que valeram 10 pontos
static bool  g_PuSawSneakers = false; // tênis já esteve ativo
static float g_PuMaxYBoost   = 0.0f;  // maior altura com o tênis ativo
static float g_PuMaxYPlain   = 0.0f;  // maior altura depois que ele acabou
static bool  g_PuSawMultipl  = false; // multiplicador x2 já esteve ativo

static void Hook_Powerups()
{
    const int gained = g_Game.coins_collected - g_PuPrevCoins;
    if (gained > 0)
    {
        const float per_coin = ((g_Game.score - g_PuPrevScore) - (g_Game.player.s - g_PuPrevS)) / gained;
        if (std::fabs(per_coin - 20.0f) < 0.5f)
            g_PuDoubled += gained;
        else if (std::fabs(per_coin - 10.0f) < 0.5f)
            g_PuSingle += gained;
    }
    g_PuPrevCoins = g_Game.coins_collected;
    g_PuPrevScore = g_Game.score;
    g_PuPrevS     = g_Game.player.s;

    if (g_Game.multiplier == 2)
        g_PuSawMultipl = true;
    if (g_Game.PowerUpActive(PowerUpType::SNEAKERS))
    {
        g_PuSawSneakers = true;
        g_PuMaxYBoost = std::max(g_PuMaxYBoost, g_Game.player.y);
    }
    else if (g_PuSawSneakers)
        g_PuMaxYPlain = std::max(g_PuMaxYPlain, g_Game.player.y);
}

static bool Check_powerups(std::string* why)
{
    char buf[200];
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "jogo terminou (nao ha obstaculos neste cenario)";
        return false;
    }
    if (g_Game.coins_collected != PU_SIDE_COINS + 2)
    {
        snprintf(buf, sizeof(buf), "moedas = %d (esperava %d: %d das faixas vizinhas, so pelo ima, + 2)",
                 g_Game.coins_collected, PU_SIDE_COINS + 2, PU_SIDE_COINS);
        *why = buf;
        return false;
    }
    if (g_PuDoubled != PU_SIDE_COINS || g_PuSingle != 2)
    {
        snprintf(buf, sizeof(buf), "moedas a 20 pts = %d (esperava %d), a 10 pts = %d (esperava 2)",
                 g_PuDoubled, PU_SIDE_COINS, g_PuSingle);
        *why = buf;
        return false;
    }
    if (!g_PuSawMultipl || g_Game.multiplier != 1)
    {
        *why = "o multiplicador nao ficou ativo ou nao voltou a 1 ao acabar";
        return false;
    }
    for (int t = 0; t < POWERUP_TYPE_COUNT; ++t)
        if (g_Game.powerup_timer[t] != 0.0f)
        {
            *why = "algum power-up ainda ativo no fim (devia ter expirado)";
            return false;
        }
    if (g_PuMaxYBoost < 4.0f || g_PuMaxYPlain < 1.5f || g_PuMaxYPlain > 2.3f ||
        g_PuMaxYBoost < 1.8f * g_PuMaxYPlain)
    {
        snprintf(buf, sizeof(buf), "altura do pulo com tenis = %.2f m, sem tenis = %.2f m", g_PuMaxYBoost,
                 g_PuMaxYPlain);
        *why = buf;
        return false;
    }
    return true;
}

// piggy (Tarefa 5.3): um cofrinho na faixa do jogador; ao pegá-lo o porco voa
// em Bézier cúbica no céu soltando moedas, que formam um rastro na pista (e o
// jogador, na faixa central, pega as que passam por ela). Sem obstáculos.
static void SetupPiggy()
{
    SetupNoSpawn();
    const PowerUp piggy = {PowerUpType::PIGGY, 0, 6.0f, false};
    g_Game.powerups.push_back(piggy);
}

static bool  g_PigSawActive = false;  // o porco esteve ativo
static float g_PigMinX      = 1e9f;   // menor/maior x do porco (curva lateral)
static float g_PigMaxX      = -1e9f;
static float g_PigMaxY      = 0.0f;   // maior altura do porco

static void Hook_Piggy()
{
    if (g_Game.pig.active)
    {
        g_PigSawActive = true;
        const glm::vec3 p = g_Game.PigTrackPosition();
        g_PigMinX = std::min(g_PigMinX, p.x);
        g_PigMaxX = std::max(g_PigMaxX, p.x);
        g_PigMaxY = std::max(g_PigMaxY, p.y);
    }
}

static bool Check_piggy(std::string* why)
{
    char buf[200];
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "jogo terminou (nao ha obstaculos neste cenario)";
        return false;
    }
    if (!g_PigSawActive || g_Game.pig.active || g_Game.piggy_pickups != 0)
    {
        *why = "o porco nao voou, ou nao terminou o voo, ou sobrou pedido de cofrinho";
        return false;
    }
    if (g_Game.pig_coins_dropped < 20)
    {
        snprintf(buf, sizeof(buf), "moedas soltas = %d (esperava >= 20)", g_Game.pig_coins_dropped);
        *why = buf;
        return false;
    }
    if (g_Game.coins_collected < 1 || g_Game.coins_collected > g_Game.pig_coins_dropped)
    {
        snprintf(buf, sizeof(buf), "moedas coletadas = %d de %d soltas (esperava 1..%d)",
                 g_Game.coins_collected, g_Game.pig_coins_dropped, g_Game.pig_coins_dropped);
        *why = buf;
        return false;
    }
    if (g_PigMinX > -4.5f || g_PigMaxX < 4.5f || g_PigMaxY < 6.5f)
    {
        snprintf(buf, sizeof(buf), "curva do porco x = [%.1f, %.1f], altura max = %.1f (nao parece a Bezier)",
                 g_PigMinX, g_PigMaxX, g_PigMaxY);
        *why = buf;
        return false;
    }
    return true;
}

// particles (Tarefa 6.1): uma moeda e um power-up (ímã) na faixa do jogador,
// sem obstáculos. Cada coleta dispara uma explosão de partículas no ponto dela:
// 30 douradas na moeda, 80 vermelhas no power-up. O hook registra quantas
// partículas existiam no quadro da coleta; no fim (> 1 s depois) todas já
// expiraram. (O main.cpp consome os eventos do Game antes do hook, então a
// contagem de Alive() no quadro da coleta é a da explosão recém-criada.)
static void SetupParticles()
{
    SetupNoSpawn();
    Coin coin = {0, 12.0f, COIN_HEIGHT, false};
    g_Game.coins.push_back(coin);
    const PowerUp magnet = {PowerUpType::MAGNET, 0, 30.0f, false};
    g_Game.powerups.push_back(magnet);
}

static int  g_PtPrevCoins   = 0;
static bool g_PtPrevMagnet  = false;
static int  g_PtCoinBurst   = -1;  // partículas vivas no quadro da coleta da moeda
static int  g_PtPowerBurst  = -1;  // idem, do power-up
static int  g_PtMaxAlive    = 0;
static int  g_PtFrame       = 0;
static bool g_PtColorsOk    = true; // cores: douradas na moeda, vermelhas no power-up
static int  g_PtBelowFloor  = 0;    // partículas com o sprite atravessando o chão (Tarefa 7.1b)
static int  g_PtOnFloor     = 0;    // partículas apoiadas no chão (quicaram)

static void Hook_Particles()
{
    ++g_PtFrame;
    const std::vector<Particle>& alive = g_Particles.Alive();
    g_PtMaxAlive = std::max(g_PtMaxAlive, (int)alive.size());
    // Chão da pista (y = 0 nesta pista): nenhum sprite o atravessa (Tarefa 7.1b).
    for (size_t i = 0; i < alive.size(); ++i)
    {
        const float rest = g_Game.track.FrameAt(g_Game.player.s).position.y + 0.5f * alive[i].size;
        if (alive[i].position.y < rest - 1e-3f)
            ++g_PtBelowFloor;
        else if (alive[i].position.y < rest + 1e-3f)
            ++g_PtOnFloor;
    }
    if (g_Game.coins_collected > g_PtPrevCoins)
    {
        g_PtCoinBurst = (int)alive.size();
        for (size_t i = 0; i < alive.size(); ++i)
            if (alive[i].color.r < 0.9f || alive[i].color.b > 0.4f)
                g_PtColorsOk = false;
    }
    g_PtPrevCoins = g_Game.coins_collected;
    const bool magnet = g_Game.PowerUpActive(PowerUpType::MAGNET);
    if (magnet && !g_PtPrevMagnet)
    {
        g_PtPowerBurst = (int)alive.size();
        for (size_t i = 0; i < alive.size(); ++i)
            if (alive[i].color.r < 0.9f || alive[i].color.g > 0.4f)
                g_PtColorsOk = false;
    }
    g_PtPrevMagnet = magnet;
}

static bool Check_particles(std::string* why)
{
    char buf[200];
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "jogo terminou (nao ha obstaculos neste cenario)";
        return false;
    }
    if (g_PtCoinBurst != 30)
    {
        snprintf(buf, sizeof(buf), "particulas no quadro da moeda = %d (esperava 30)", g_PtCoinBurst);
        *why = buf;
        return false;
    }
    if (g_PtPowerBurst != 80)
    {
        snprintf(buf, sizeof(buf), "particulas no quadro do power-up = %d (esperava 80)", g_PtPowerBurst);
        *why = buf;
        return false;
    }
    if (!g_PtColorsOk)
    {
        *why = "cor das particulas errada (moeda dourada / power-up vermelho)";
        return false;
    }
    if (g_PtBelowFloor > 0 || g_PtOnFloor == 0)
    {
        snprintf(buf, sizeof(buf), "particulas abaixo do chao em %d amostras; apoiadas no chao em %d (esperava > 0)",
                 g_PtBelowFloor, g_PtOnFloor);
        *why = buf;
        return false;
    }
    if (!g_Game.events.empty())
    {
        *why = "eventos de coleta nao foram consumidos";
        return false;
    }
    if (!g_Particles.Alive().empty())
    {
        snprintf(buf, sizeof(buf), "ainda ha %d particulas vivas no fim (deviam ter expirado)",
                 (int)g_Particles.Alive().size());
        *why = buf;
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

// ---- Rampas e andar sobre os trens (Tarefa 4.4) ----
//
// ramp_train: sem spawn, rampa + trem parado na faixa do jogador (faixa 0,
// trem centrado em RAMP_TRAIN_S: rampa em [25.5, 33.5], trem em [33.5, 46.5]),
// sem nenhuma entrada: o jogador sobe a rampa, corre no teto, cai no fim e
// continua jogando. Fotos na rampa, no teto e depois da queda.
// roof_hop: o mesmo trem e outro na faixa 1 (sem rampa, [35.5, 48.5]); em
// cima do primeiro troca para o teto do vizinho, e no fim troca duas faixas
// para a esquerda (faixa -1, sem trem) e cai.
static const float RAMP_TRAIN_S     = 40.0f;
static const float HOP_TRAIN_S      = 42.0f;
static float g_RoofMaxY             = 0.0f;  // maior altura do jogador
static bool  g_SawOnRoof            = false; // em pé no teto (faixa 0) em algum quadro
static bool  g_SawOnNeighborRoof    = false; // em pé no teto do trem da faixa 1
static float g_MaxFootGap           = 0.0f;  // maior |pés - superfície da rampa| em pé na rampa

static float TrainRoof()
{
    return OBSTACLE_SIZE[(int)ObstacleType::TRAIN][1];
}

static void SetupRampTrain()
{
    SetupNoSpawn();
    Obstacle t = g_Game.MakeObstacle(ObstacleType::TRAIN, 0, RAMP_TRAIN_S);
    g_Game.obstacles.push_back(g_Game.MakeRampFor(t));
    g_Game.obstacles.push_back(t);
}

static void SetupRoofHop()
{
    SetupRampTrain();
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::TRAIN, 1, HOP_TRAIN_S));
}

static void Hook_Roof()
{
    const Player& p = g_Game.player;
    g_RoofMaxY = std::max(g_RoofMaxY, p.y);
    const bool on_roof = p.grounded && std::fabs(p.y - TrainRoof()) < 1e-3f;
    if (on_roof && std::fabs(p.x) < 1e-3f)
        g_SawOnRoof = true;
    if (on_roof && std::fabs(p.x - LANE_WIDTH) < 1e-3f)
        g_SawOnNeighborRoof = true;
    // Em pé sobre a rampa (frente do jogador antes do trem): pés na superfície.
    const AABB box = g_Game.PlayerBox();
    float h;
    if (p.grounded && !g_Game.obstacles.empty() && g_Game.obstacles[0].type == ObstacleType::RAMP &&
        box.max.z < g_Game.obstacles[0].box.max.z &&
        RampHeightUnder(g_Game.obstacles[0].box, box, &h))
        g_MaxFootGap = std::max(g_MaxFootGap, std::fabs(p.y - h));
}

// Comum aos dois: chegou ao teto (altura máxima = teto, sem pular), andou em
// cima dele, os pés seguiram a rampa, e no fim está no chão, ainda jogando.
static bool CheckRoofRun(std::string* why)
{
    char buf[200];
    const Player& p = g_Game.player;
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "o jogo acabou (bateu no trem/rampa?)";
        return false;
    }
    if (std::fabs(g_RoofMaxY - TrainRoof()) > 0.01f)
    {
        snprintf(buf, sizeof(buf), "altura maxima %.3f m, esperava o teto do trem (%.2f m)", g_RoofMaxY,
                 TrainRoof());
        *why = buf;
        return false;
    }
    if (!g_SawOnRoof)
    {
        *why = "o jogador nunca ficou em pe no teto do trem";
        return false;
    }
    if (g_MaxFootGap > 1e-3f)
    {
        snprintf(buf, sizeof(buf), "pes fora da superficie da rampa: %.4f m", g_MaxFootGap);
        *why = buf;
        return false;
    }
    if (!p.grounded || p.y != 0.0f)
    {
        snprintf(buf, sizeof(buf), "no fim, esperava o jogador no chao: y = %.3f", p.y);
        *why = buf;
        return false;
    }
    return true;
}

static bool Check_ramp_train(std::string* why)
{
    if (!CheckRoofRun(why))
        return false;
    if (g_Game.player.s < RAMP_TRAIN_S + 15.0f)
    {
        *why = "o jogador nao passou do fim do trem";
        return false;
    }
    return true;
}

static bool Check_roof_hop(std::string* why)
{
    if (!CheckRoofRun(why))
        return false;
    if (!g_SawOnNeighborRoof)
    {
        *why = "o jogador nunca ficou em pe no teto do trem da faixa 1";
        return false;
    }
    const Player& p = g_Game.player;
    if (p.lane != -1 || std::fabs(p.x + LANE_WIDTH) > 1e-3f)
    {
        *why = "esperava o jogador na faixa -1 (sem trem) no fim";
        return false;
    }
    return true;
}

// ramp_side_block: rampa + trem na faixa 1, jogador na faixa 0 troca para a
// direita ao lado do trecho alto da rampa: a lateral é parede, a troca é
// desfeita (volta para a faixa 0), sem subir e sem morrer.
static float g_SideMaxX = 0.0f;   // maior x do jogador (chegou a encostar?)
static bool  g_SideInRamp = false; // pegada dentro da faixa x da rampa em algum quadro

static void SetupRampSide()
{
    SetupNoSpawn();
    Obstacle t = g_Game.MakeObstacle(ObstacleType::TRAIN, 1, RAMP_TRAIN_S);
    g_Game.obstacles.push_back(g_Game.MakeRampFor(t));
    g_Game.obstacles.push_back(t);
}

static void Hook_RampSide()
{
    const Player& p = g_Game.player;
    g_RoofMaxY = std::max(g_RoofMaxY, p.y);
    g_SideMaxX = std::max(g_SideMaxX, p.x);
    if (!g_Game.obstacles.empty() && g_Game.PlayerBox().max.x >= g_Game.obstacles[0].box.min.x)
        g_SideInRamp = true;
}

static bool Check_ramp_side_block(std::string* why)
{
    char buf[200];
    const Player& p = g_Game.player;
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "o jogo acabou (bateu na lateral da rampa/trem?)";
        return false;
    }
    if (g_RoofMaxY != 0.0f || g_SideInRamp)
    {
        snprintf(buf, sizeof(buf), "entrou na lateral da rampa: altura maxima %.3f m", g_RoofMaxY);
        *why = buf;
        return false;
    }
    if (g_SideMaxX < 0.9f)
    {
        *why = "o jogador nem chegou a encostar na lateral da rampa";
        return false;
    }
    if (p.lane != 0 || p.x != 0.0f)
    {
        snprintf(buf, sizeof(buf), "esperava a troca desfeita (faixa 0, x = 0); faixa %d, x = %.3f", p.lane, p.x);
        *why = buf;
        return false;
    }
    return true;
}

// free_camera: corre 2 s, C (câmera livre, jogo congelado), voa para a frente
// 1 s segurando W, gira o mouse (olha para trás e para baixo, vendo o jogador
// de frente), sobe 0,5 s com Espaço, gira para o lado, C (volta) e corre mais
// 1 s. O jogo não pode andar enquanto a câmera livre está ativa.
static bool  g_FcSeen         = false;  // esteve em FREE_CAMERA
static float g_FcStartS       = 0.0f;   // player.s ao entrar
static float g_FcStartElapsed = 0.0f;
static int   g_FcStartCoins   = 0;
static float g_FcMaxDriftS    = 0.0f;   // maior |player.s - s de entrada| em FREE_CAMERA
static bool  g_FcElapsedMoved = false;  // o relógio do jogo andou em FREE_CAMERA
static glm::vec4 g_FcStartPos;          // posição da câmera livre no 1º quadro
static glm::vec4 g_FcPosAfterW;         // posição no 1º quadro depois de soltar W
static glm::vec4 g_FcStartView;         // view vector no 1º quadro em FREE_CAMERA
static glm::vec4 g_FcLastView;          // view vector no último quadro em FREE_CAMERA
static bool  g_FcWasHeld      = false;
static bool  g_FcAfterWSaved  = false;
static int   g_FcWFrames      = 0;      // quadros em FREE_CAMERA com W segurado
static bool  g_FcResumed      = false;  // voltou a PLAYING depois da câmera livre
static float g_FcResumeDist   = 1e9f;   // distância câmera 3ª pessoa-jogador ao voltar

static void SetupFreeCamera()
{
    SetupNoSpawn();
    // Um trem parado na faixa vizinha, para dar o que olhar.
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::TRAIN, 1, 40.0f));
}

static void Hook_FreeCamera()
{
    const Player& p = g_Game.player;
    if (g_Game.mode == GameMode::FREE_CAMERA)
    {
        const CameraPose pose = g_FreeCamera.Pose();
        if (!g_FcSeen)
        {
            g_FcSeen = true;
            g_FcStartS = p.s;
            g_FcStartElapsed = g_Game.elapsed;
            g_FcStartCoins = g_Game.coins_collected;
            g_FcStartPos = pose.position;
            g_FcStartView = pose.view_vector;
        }
        g_FcMaxDriftS = std::max(g_FcMaxDriftS, std::fabs(p.s - g_FcStartS));
        if (std::fabs(g_Game.elapsed - g_FcStartElapsed) > 1e-6f)
            g_FcElapsedMoved = true;
        const bool w = g_KeyHeld[GLFW_KEY_W];
        if (w)
            ++g_FcWFrames;
        if (g_FcWasHeld && !w && !g_FcAfterWSaved)
        {
            g_FcAfterWSaved = true;
            g_FcPosAfterW = pose.position;
        }
        g_FcWasHeld = w;
        g_FcLastView = pose.view_vector;
    }
    else if (g_FcSeen && !g_FcResumed && g_Game.mode == GameMode::PLAYING)
    {
        g_FcResumed = true;
        const glm::vec4 c = g_Camera.Pose().position;
        const glm::vec4 pl = g_Game.track.FrameAt(p.s).position;
        g_FcResumeDist = std::sqrt((c.x - pl.x) * (c.x - pl.x) + (c.y - pl.y) * (c.y - pl.y) +
                                   (c.z - pl.z) * (c.z - pl.z));
    }
}

static bool Check_free_camera(std::string* why)
{
    char buf[200];
    if (!g_FcSeen)
    {
        *why = "nunca entrou em FREE_CAMERA";
        return false;
    }
    if (g_FcMaxDriftS > 1e-6f || g_FcElapsedMoved || g_Game.coins_collected != g_FcStartCoins)
    {
        snprintf(buf, sizeof(buf), "o jogo andou com a camera livre ativa (deriva de s = %.4f m)",
                 g_FcMaxDriftS);
        *why = buf;
        return false;
    }
    // W segurado por ~1 s: 10 m/s * tempo (dt fixo 1/60), na direção em que olha.
    if (g_FcWFrames < 55 || g_FcWFrames > 65 || !g_FcAfterWSaved)
    {
        snprintf(buf, sizeof(buf), "W segurado por %d quadros (esperava ~60)", g_FcWFrames);
        *why = buf;
        return false;
    }
    const glm::vec4 d = g_FcPosAfterW - g_FcStartPos;
    const float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    const float expected = FREE_CAMERA_SPEED * (float)g_FcWFrames / 60.0f;
    if (std::fabs(dist - expected) > 0.05f)
    {
        snprintf(buf, sizeof(buf), "deslocou %.3f m com W; esperava %.3f m (10 m/s x tempo)", dist, expected);
        *why = buf;
        return false;
    }
    // O mouse injetado girou o olhar de fato: ~108 graus de yaw + pitch (> 1,5 rad no total).
    const float cosang = g_FcStartView.x * g_FcLastView.x + g_FcStartView.y * g_FcLastView.y +
                         g_FcStartView.z * g_FcLastView.z;
    const float angle = std::acos(std::max(-1.0f, std::min(1.0f, cosang)));
    if (angle < 1.5f)
    {
        snprintf(buf, sizeof(buf), "o mouse nao girou o olhar (angulo %.2f rad, esperava > 1.5)", angle);
        *why = buf;
        return false;
    }
    if (!g_FcResumed || g_Game.mode != GameMode::PLAYING)
    {
        *why = "nao voltou a PLAYING no fim";
        return false;
    }
    if (g_FcResumeDist > 10.0f)
    {
        snprintf(buf, sizeof(buf), "ao voltar a camera de 3a pessoa ficou a %.1f m do jogador (Snap?)", g_FcResumeDist);
        *why = buf;
        return false;
    }
    if (g_Game.player.s < g_FcStartS + 10.0f)
    {
        snprintf(buf, sizeof(buf), "o jogo nao continuou depois de voltar (s = %.1f, entrada = %.1f)",
                 g_Game.player.s, g_FcStartS);
        *why = buf;
        return false;
    }
    return true;
}

// GUI (Tarefa 6.2): menu, pausa e game over feitos com ImGui. Os botões são
// clicados pelo rótulo (AutotestStep.click), com um clique real na fila de
// eventos do ImGui. Hook_Gui registra por quantos quadros o jogo ficou em
// MENU/PAUSED e se algo andou durante a pausa (jogador, relógio, porco e
// partículas devem ficar congelados).
static int       g_GuiMenuFrames      = 0;     // quadros observados em MENU
static int       g_GuiPausedFrames    = 0;     // quadros observados em PAUSED
static int       g_GuiPauseEntries    = 0;     // vezes que entrou em PAUSED
static float     g_GuiMenuMaxS        = 0.0f;  // maior |s| do jogador no MENU (deve ser 0)
static float     g_GuiPauseDrift      = 0.0f;  // maior variação de estado dentro de uma pausa
static bool      g_GuiPausedWithPig   = false; // alguma pausa pegou o porco voando
static bool      g_GuiPausedWithParts = false; // alguma pausa pegou partículas vivas
static bool      g_GuiSawReset        = false; // s voltou a ~0 vindo de uma corrida (Reiniciar)
static float     g_GuiPrevS           = 0.0f;
static GameMode  g_GuiPrevMode        = GameMode::PLAYING;
static float     g_GuiPauseS          = 0.0f;  // estado no 1º quadro da pausa atual
static float     g_GuiPauseElapsed    = 0.0f;
static float     g_GuiPausePigT       = 0.0f;
static size_t    g_GuiPauseParticles  = 0;
static glm::vec4 g_GuiPauseP0;
static float     g_GuiPauseLife0      = 0.0f;

static void Hook_Gui()
{
    const GameMode m = g_Game.mode;
    const float s = g_Game.player.s;
    if (m == GameMode::MENU)
    {
        ++g_GuiMenuFrames;
        g_GuiMenuMaxS = std::max(g_GuiMenuMaxS, std::fabs(s));
    }
    if (m == GameMode::PAUSED)
    {
        ++g_GuiPausedFrames;
        const std::vector<Particle>& alive = g_Particles.Alive();
        if (g_GuiPrevMode != GameMode::PAUSED)
        {
            ++g_GuiPauseEntries;
            g_GuiPauseS         = s;
            g_GuiPauseElapsed   = g_Game.elapsed;
            g_GuiPausePigT      = g_Game.pig.t;
            g_GuiPauseParticles = alive.size();
            if (!alive.empty())
            {
                g_GuiPauseP0    = alive[0].position;
                g_GuiPauseLife0 = alive[0].life;
            }
            g_GuiPausedWithPig   = g_GuiPausedWithPig || g_Game.pig.active;
            g_GuiPausedWithParts = g_GuiPausedWithParts || !alive.empty();
        }
        else
        {
            float d = std::fabs(s - g_GuiPauseS);
            d = std::max(d, std::fabs(g_Game.elapsed - g_GuiPauseElapsed));
            d = std::max(d, std::fabs(g_Game.pig.t - g_GuiPausePigT));
            if (alive.size() != g_GuiPauseParticles)
                d = std::max(d, 1.0f);
            else if (!alive.empty())
            {
                const glm::vec4 dp = alive[0].position - g_GuiPauseP0;
                d = std::max(d, std::fabs(dp.x) + std::fabs(dp.y) + std::fabs(dp.z));
                d = std::max(d, std::fabs(alive[0].life - g_GuiPauseLife0));
            }
            g_GuiPauseDrift = std::max(g_GuiPauseDrift, d);
        }
    }
    if (g_GuiPrevS > 3.0f && s < 1.0f)
        g_GuiSawReset = true;
    g_GuiPrevS    = s;
    g_GuiPrevMode = m;
}

// Confere o que vale para todo cenário de GUI: terminou jogando, nada andou
// durante as pausas e o relógio do jogo só correu fora do MENU/PAUSED
// (elapsed + quadros parados * dt = tempo simulado, com folga de 6 quadros
// por causa das transições).
static bool CheckGuiCommon(float duration, std::string* why)
{
    char buf[200];
    if (g_Game.mode != GameMode::PLAYING)
    {
        snprintf(buf, sizeof(buf), "esperava PLAYING no fim; modo = %d", (int)g_Game.mode);
        *why = buf;
        return false;
    }
    if (g_GuiMenuMaxS != 0.0f)
    {
        snprintf(buf, sizeof(buf), "o jogador andou no MENU (s = %.3f)", g_GuiMenuMaxS);
        *why = buf;
        return false;
    }
    if (g_GuiPauseDrift > 1e-6f)
    {
        snprintf(buf, sizeof(buf), "algo andou durante a pausa (variacao = %g)", g_GuiPauseDrift);
        *why = buf;
        return false;
    }
    const float dt = Autotest_FixedDt();
    const float stopped = (g_GuiMenuFrames + g_GuiPausedFrames) * dt;
    // (Com "Reiniciar" o relógio volta a zero no meio: não dá para comparar.)
    if (!g_GuiSawReset && std::fabs(g_Game.elapsed + stopped - duration) > 6.0f * dt)
    {
        snprintf(buf, sizeof(buf), "relogio do jogo %.2f s + parado %.2f s != %.2f s simulados",
                 g_Game.elapsed, stopped, duration);
        *why = buf;
        return false;
    }
    return true;
}

// menu_pause: começa no menu (o jogador parado), abre a tela "Controles" (sem
// "Jogar" nem "Sair" nela), volta pelo botão "Voltar", abre de novo e volta
// com ESC (que no menu não pausa nem fecha), clica em "Jogar"; pausa com P e
// volta com P; pausa com ESC (que não fecha mais a janela) e volta clicando
// em "Continuar".
static const float MENU_PAUSE_DURATION = 5.0f;
static bool Check_menu_pause(std::string* why)
{
    char buf[200];
    if (!CheckGuiCommon(MENU_PAUSE_DURATION, why))
        return false;
    if (g_GuiMenuFrames < 30 || g_GuiPauseEntries != 2 || g_GuiPausedFrames < 70)
    {
        snprintf(buf, sizeof(buf), "quadros no menu = %d (>= 30), pausas = %d (2), quadros pausado = %d (>= 70)",
                 g_GuiMenuFrames, g_GuiPauseEntries, g_GuiPausedFrames);
        *why = buf;
        return false;
    }
    if (g_Game.player.s < 20.0f)
    {
        snprintf(buf, sizeof(buf), "jogador quase nao correu (s = %.1f)", g_Game.player.s);
        *why = buf;
        return false;
    }
    return true;
}

// pause_freeze: pausa no meio do voo do porco do cofrinho, com partículas de
// uma moeda recém-coletada e o ímã ativo (barra no HUD). Tudo fica parado
// durante a pausa e o voo termina depois de continuar.
static const float PAUSE_FREEZE_DURATION = 7.5f;
static void SetupPauseFreeze()
{
    SetupPiggy();
    const PowerUp magnet = {PowerUpType::MAGNET, 0, 3.0f, false};
    g_Game.powerups.push_back(magnet);
    Coin coin = {0, 16.0f, COIN_HEIGHT, false};
    g_Game.coins.push_back(coin);
}

static void Hook_PauseFreeze()
{
    Hook_Gui();
    Hook_Piggy();
}

static bool Check_pause_freeze(std::string* why)
{
    if (!CheckGuiCommon(PAUSE_FREEZE_DURATION, why))
        return false;
    if (g_GuiPauseEntries != 1 || !g_GuiPausedWithPig || !g_GuiPausedWithParts)
    {
        *why = "a pausa nao pegou o porco voando e particulas vivas";
        return false;
    }
    if (!g_PigSawActive || g_Game.pig.active)
    {
        *why = "o voo do porco nao terminou depois da pausa";
        return false;
    }
    return true;
}

// focus_pause (Revisão final, item 9): perder o foco / minimizar a janela
// pausa a corrida. Os callbacks da GLFW ignoram foco no autotest (a janela do
// E2E perde o foco quando se mexe no desktop), então o hook chama direto o
// tratamento deles, HandleWindowFocusLost(): em PLAYING (1 s) -> PAUSED, nada
// anda na pausa, P continua; na câmera livre (C em 3 s, foco perdido em 3.5 s)
// -> PAUSED (sai da câmera livre); "Continuar" volta; de novo em 4.7 s e
// "Continuar" em 5 s. No fim o jogo está correndo.
static const float FOCUS_PAUSE_DURATION = 6.0f;
static int   g_FpFrame          = 0;
static int   g_FpLostCalls      = 0;     // chamadas de HandleWindowFocusLost
static int   g_FpPausedByFocus  = 0;     // chamadas que deixaram o jogo em PAUSED
static bool  g_FpFreeCamOk      = false; // a da câmera livre pausou (e saiu dela)
static float g_FpPauseS         = -1.0f; // s do jogador logo após a 1ª pausa
static float g_FpPauseDrift     = 0.0f;  // quanto o jogador andou durante a 1ª pausa

static void Hook_FocusPause()
{
    GLFWwindow* window = glfwGetCurrentContext();
    const int f = g_FpFrame++;
    // Quadros a 60 Hz: 1.0 s, 3.5 s e 4.7 s (o último com o jogo já pausado).
    if (f == 60 || f == 210 || f == 282)
    {
        const bool was_free = g_Game.mode == GameMode::FREE_CAMERA;
        HandleWindowFocusLost(window);
        ++g_FpLostCalls;
        if (g_Game.mode == GameMode::PAUSED)
            ++g_FpPausedByFocus;
        if (f == 60)
            g_FpPauseS = g_Game.player.s;
        if (f == 210)
            g_FpFreeCamOk = was_free && g_Game.mode == GameMode::PAUSED;
    }
    if (f > 60 && f < 120 && g_Game.mode == GameMode::PAUSED)
        g_FpPauseDrift = std::max(g_FpPauseDrift, std::fabs(g_Game.player.s - g_FpPauseS));
}

static bool Check_focus_pause(std::string* why)
{
    char buf[200];
    if (g_FpLostCalls != 3 || g_FpPausedByFocus != 3 || !g_FpFreeCamOk)
    {
        snprintf(buf, sizeof(buf), "foco perdido %d vezes (3), pausado em %d (3), camera livre pausou = %d",
                 g_FpLostCalls, g_FpPausedByFocus, (int)g_FpFreeCamOk);
        *why = buf;
        return false;
    }
    if (g_FpPauseDrift > 1e-4f)
    {
        snprintf(buf, sizeof(buf), "o jogador andou %.3f m na pausa por perda de foco", g_FpPauseDrift);
        *why = buf;
        return false;
    }
    if (g_Game.mode != GameMode::PLAYING || g_Game.player.s < 30.0f)
    {
        snprintf(buf, sizeof(buf), "no fim o jogo devia estar correndo (modo %d, s = %.1f)", (int)g_Game.mode,
                 g_Game.player.s);
        *why = buf;
        return false;
    }
    return true;
}

// marathon (Tarefa 7.1a): partida longa, com spawn ligado e um piloto
// automático que segue a faixa segura de cada chunk e pula/rola as barreiras
// dela, ocupando 600 s de JOGO. Para caber no tempo do E2E (cada quadro
// desenhado custa ~10 ms), cada quadro renderizado roda MARATHON_SUBSTEPS
// passos de lógica a mais (dt = 1/60 s cada, a mesma sequência do jogo normal,
// com partículas e eventos de coleta): 60 s de quadros x 10 = 600 s de jogo.
// Confere sem erro de GL (o harness faz), jogo vivo, e que o que o jogo
// mantém (obstáculos, moedas, power-ups, partículas, segmentos da pista,
// eventos) fica LIMITADO, ou seja, não vaza ao longo de ~17 km de pista.
static const int   MARATHON_SUBSTEPS   = 9;
static const float MARATHON_DURATION   = 60.0f; // s de quadros (x10 = 600 s de jogo)
static size_t g_MaraMaxObstacles  = 0;
static size_t g_MaraMaxCoins      = 0;
static size_t g_MaraMaxPowerups   = 0;
static size_t g_MaraMaxParticles  = 0;
static size_t g_MaraMaxSegments   = 0;
static size_t g_MaraMaxEvents     = 0;
static int    g_MaraDeaths        = 0;
static int    g_MaraBarriersDodged = 0;
static bool   g_MaraNonFinite     = false;

// Piloto: faixa segura do chunk (a troca acontece na emenda livre entre
// chunks), pulo na barreira baixa e rolamento na alta da faixa em que está.
static void MarathonPilot()
{
    Player& p = g_Game.player;
    const int c = (int)std::floor(p.s / CHUNK_LENGTH);
    const float end = (c + 1) * CHUNK_LENGTH;
    const int target = g_Game.SafeLaneOfChunk(p.s >= end - CHUNK_CLEAR_TAIL ? c + 1 : c);
    if (target >= -1 && target <= 1)
        p.lane = target;
    // Só a barreira mais próxima à frente, na faixa (placas e trens não entram:
    // a faixa segura não os tem).
    const float nose = p.s + PLAYER_HALF_DEPTH;
    const Obstacle* next = NULL;
    for (size_t i = 0; i < g_Game.obstacles.size(); ++i)
    {
        const Obstacle& o = g_Game.obstacles[i];
        if (o.lane != p.lane || o.box.min.z < nose)
            continue;
        if (o.type != ObstacleType::BARRIER_LOW && o.type != ObstacleType::BARRIER_HIGH)
            continue;
        if (next == NULL || o.box.min.z < next->box.min.z)
            next = &o;
    }
    if (next == NULL)
        return;
    const float t = (next->box.min.z - nose) / p.speed; // s até o contato
    if (next->type == ObstacleType::BARRIER_LOW)
    {
        if (p.grounded && t <= 0.30f)
        {
            g_Game.HandleInput(Input::JUMP);
            ++g_MaraBarriersDodged;
        }
    }
    else
    {
        // Rola colada à viga (renovando o rolamento a cada passo); no ar, depois
        // de uma barreira baixa, rola antes para descer a tempo.
        if (t <= 0.25f || (!p.grounded && t <= 0.5f))
        {
            if (p.roll_timer <= 0.0f)
                ++g_MaraBarriersDodged;
            g_Game.HandleInput(Input::ROLL);
        }
    }
}

static void MarathonObserve()
{
    g_MaraMaxObstacles = std::max(g_MaraMaxObstacles, g_Game.obstacles.size());
    g_MaraMaxCoins     = std::max(g_MaraMaxCoins, g_Game.coins.size());
    g_MaraMaxPowerups  = std::max(g_MaraMaxPowerups, g_Game.powerups.size());
    g_MaraMaxParticles = std::max(g_MaraMaxParticles, g_Particles.Alive().size());
    g_MaraMaxSegments  = std::max(g_MaraMaxSegments, g_Game.track.Segments().size());
    g_MaraMaxEvents    = std::max(g_MaraMaxEvents, g_Game.events.size());
    if (!std::isfinite(g_Game.player.s) || !std::isfinite(g_Game.player.y) ||
        !std::isfinite(g_Game.score) || !std::isfinite(g_Game.player.x))
        g_MaraNonFinite = true;
    if (g_Game.mode == GameMode::GAME_OVER)
    {
        ++g_MaraDeaths;
        g_Game.mode = GameMode::PLAYING; // segue a corrida: conta e continua
    }
}

static void Hook_Marathon()
{
    if (g_Game.mode != GameMode::PLAYING)
        return;
    const float dt = Autotest_FixedDt();
    for (int k = 0; k < MARATHON_SUBSTEPS; ++k)
    {
        MarathonPilot();
        g_Game.Update(dt);
        g_Particles.Update(dt);
        EmitCollectParticles(g_Game); // (main.cpp) dispara as partículas e limpa os eventos
        MarathonObserve();
    }
    MarathonPilot(); // para o passo que o laço principal dá em seguida
    MarathonObserve();
}

static bool Check_marathon(std::string* why)
{
    char buf[260];
    snprintf(buf, sizeof(buf),
             "[marathon] jogo %.0f s, %.1f km, v = %.1f m/s, mortes %d, barreiras %d; maximos: obstaculos %d, "
             "moedas %d, power-ups %d, particulas %d, segmentos %d, eventos %d",
             g_Game.elapsed, g_Game.player.s / 1000.0f, g_Game.player.speed, g_MaraDeaths,
             g_MaraBarriersDodged, (int)g_MaraMaxObstacles, (int)g_MaraMaxCoins, (int)g_MaraMaxPowerups,
             (int)g_MaraMaxParticles, (int)g_MaraMaxSegments, (int)g_MaraMaxEvents);
    printf("%s\n", buf);
    if (g_MaraNonFinite)
    {
        *why = "posicao/pontuacao nao finita durante a partida";
        return false;
    }
    if (g_Game.mode != GameMode::PLAYING || g_MaraDeaths != 0)
    {
        snprintf(buf, sizeof(buf), "o piloto morreu (%d vez(es)); modo no fim = %d", g_MaraDeaths, (int)g_Game.mode);
        *why = buf;
        return false;
    }
    const float expected = MARATHON_DURATION * (float)(MARATHON_SUBSTEPS + 1);
    if (g_Game.elapsed < expected - 2.0f || g_Game.player.s < 10000.0f ||
        std::fabs(g_Game.player.speed - PLAYER_MAX_SPEED) > 1e-3f)
    {
        snprintf(buf, sizeof(buf), "partida curta: %.1f s de jogo (>= %.0f), s = %.0f m (>= 10000), v = %.2f (max)",
                 g_Game.elapsed, expected - 2.0f, g_Game.player.s, g_Game.player.speed);
        *why = buf;
        return false;
    }
    // Limites do que fica na memória: a janela é DESPAWN_BEHIND + SPAWN_AHEAD
    // (~170 m = ~6 chunks de ate 4 obstaculos); moedas: ate 7 por chunk mais as
    // do porco (ate 27 por voo); particulas: o pool (nunca passa da capacidade).
    if (g_MaraMaxObstacles > 40 || g_MaraMaxCoins > 120 || g_MaraMaxPowerups > 8 ||
        g_MaraMaxParticles > g_Particles.Capacity() || g_MaraMaxSegments > 16 ||
        g_MaraMaxEvents > COLLECT_EVENT_CAPACITY)
    {
        *why = "contagem fora do limite (vazamento?): veja a linha [marathon] acima";
        return false;
    }
    if (g_Game.obstacles.size() < 3 || g_Game.coins.empty())
    {
        *why = "o spawn parou de gerar obstaculos/moedas";
        return false;
    }
    return true;
}

// gui_keys (Tarefa 7.1a): teclado fora da corrida. Setas/WASD no MENU e no
// PAUSED não pulam, rolam nem trocam de faixa (o jogador fica como estava);
// Enter no MENU inicia a corrida sem pular; P e ESC saem da câmera livre
// direto para a pausa (duas vezes, uma com cada tecla). Sem obstáculos.
static int   g_KeysMenuToPlaying = 0;     // transições MENU -> PLAYING
static int   g_KeysFreeToPaused  = 0;     // transições FREE_CAMERA -> PAUSED
static bool  g_KeysMenuMoved     = false; // o jogador mexeu (faixa/altura/rolamento) no MENU
static bool  g_KeysPausedMoved   = false; // idem dentro de uma pausa
static bool  g_KeysJumpOnStart   = false; // 1º quadro jogando já estava no ar / fora da faixa 0
static bool  g_KeysCheckStart    = false;
static int   g_KeysPausedLane    = 0;
static float g_KeysPausedX       = 0.0f;
static float g_KeysPausedY       = 0.0f;
static float g_KeysPausedRoll    = 0.0f;
static GameMode g_KeysPrevMode   = GameMode::PLAYING;

static bool KeysPlayerMoved(int lane, float x, float y, float roll)
{
    const Player& p = g_Game.player;
    return p.lane != lane || p.x != x || p.y != y || p.vy != 0.0f || p.roll_timer != roll;
}

static void Hook_GuiKeys()
{
    Hook_Gui();
    const GameMode m = g_Game.mode;
    if (m == GameMode::MENU && KeysPlayerMoved(0, 0.0f, 0.0f, 0.0f))
        g_KeysMenuMoved = true;
    if (g_KeysPrevMode == GameMode::MENU && m == GameMode::PLAYING)
    {
        ++g_KeysMenuToPlaying;
        g_KeysCheckStart = true;
    }
    if (g_KeysCheckStart && m == GameMode::PLAYING)
    {
        // Enter não pode virar pulo na corrida nova.
        g_KeysJumpOnStart = KeysPlayerMoved(0, 0.0f, 0.0f, 0.0f) || !g_Game.player.grounded;
        g_KeysCheckStart  = false;
    }
    if (g_KeysPrevMode == GameMode::FREE_CAMERA && m == GameMode::PAUSED)
        ++g_KeysFreeToPaused;
    if (m == GameMode::PAUSED)
    {
        if (g_KeysPrevMode != GameMode::PAUSED)
        {
            g_KeysPausedLane = g_Game.player.lane;
            g_KeysPausedX    = g_Game.player.x;
            g_KeysPausedY    = g_Game.player.y;
            g_KeysPausedRoll = g_Game.player.roll_timer;
        }
        else if (KeysPlayerMoved(g_KeysPausedLane, g_KeysPausedX, g_KeysPausedY, g_KeysPausedRoll))
            g_KeysPausedMoved = true;
    }
    g_KeysPrevMode = m;
}

static bool Check_gui_keys(std::string* why)
{
    char buf[200];
    if (g_Game.mode != GameMode::PLAYING)
    {
        snprintf(buf, sizeof(buf), "esperava PLAYING no fim; modo = %d", (int)g_Game.mode);
        *why = buf;
        return false;
    }
    if (g_KeysMenuToPlaying != 1 || g_GuiMenuFrames < 20 || g_GuiMenuMaxS != 0.0f)
    {
        snprintf(buf, sizeof(buf), "Enter no MENU: iniciou %d vez(es) (1), quadros no menu %d (>= 20), s max %.3f (0)",
                 g_KeysMenuToPlaying, g_GuiMenuFrames, g_GuiMenuMaxS);
        *why = buf;
        return false;
    }
    if (g_KeysMenuMoved)
    {
        *why = "setas/WASD no MENU mexeram no jogador (faixa, altura ou rolamento)";
        return false;
    }
    if (g_KeysJumpOnStart)
    {
        *why = "o Enter que iniciou a corrida virou pulo/troca de faixa";
        return false;
    }
    if (g_KeysPausedMoved || g_GuiPauseDrift > 1e-6f)
    {
        snprintf(buf, sizeof(buf), "algo mexeu durante a pausa (jogador = %d, variacao = %g)",
                 g_KeysPausedMoved ? 1 : 0, g_GuiPauseDrift);
        *why = buf;
        return false;
    }
    if (g_KeysFreeToPaused != 2 || g_GuiPauseEntries != 3 || g_GuiPausedFrames < 60)
    {
        snprintf(buf, sizeof(buf), "camera livre -> pausa: %d (2), pausas %d (3), quadros pausado %d (>= 60)",
                 g_KeysFreeToPaused, g_GuiPauseEntries, g_GuiPausedFrames);
        *why = buf;
        return false;
    }
    if (g_Game.player.lane != 0 || g_Game.player.x != 0.0f || g_Game.player.s < 10.0f)
    {
        snprintf(buf, sizeof(buf), "no fim lane = %d, x = %.2f (0), s = %.1f (>= 10)",
                 g_Game.player.lane, g_Game.player.x, g_Game.player.s);
        *why = buf;
        return false;
    }
    return true;
}

// gui_buttons: ESC pausa -> "Reiniciar" (corrida volta ao início); ESC ->
// "Menu" -> "Jogar". Sem obstáculos.
static void Hook_GuiButtons()
{
    Hook_Gui();
}

static bool Check_gui_buttons(std::string* why)
{
    char buf[200];
    if (!CheckGuiCommon(3.2f, why))
        return false;
    if (!g_GuiSawReset || g_GuiMenuFrames < 5 || g_GuiPauseEntries != 2 || g_Game.player.s < 5.0f)
    {
        snprintf(buf, sizeof(buf), "reiniciou = %d, quadros no menu = %d, pausas = %d (2), s = %.1f",
                 (int)g_GuiSawReset, g_GuiMenuFrames, g_GuiPauseEntries, g_Game.player.s);
        *why = buf;
        return false;
    }
    return true;
}

// game_over_gui / game_over_menu: bate no trem móvel (moving_train_hit) e,
// na janela de fim de jogo, recomeça com Enter ou volta ao menu pelo botão
// "Menu" e clica em "Jogar".
static void Hook_GameOverGui()
{
    Hook_GameOver();
    Hook_Gui();
}

static bool CheckGameOverGui(bool via_menu, std::string* why)
{
    char buf[200];
    if (!g_SawGameOver)
    {
        *why = "nao houve game over";
        return false;
    }
    if (g_Game.mode != GameMode::PLAYING || g_Game.player.s > 10.0f || (via_menu && g_GuiMenuFrames < 3))
    {
        snprintf(buf, sizeof(buf), "esperava corrida nova em PLAYING: modo = %d, s = %.1f, quadros no menu = %d",
                 (int)g_Game.mode, g_Game.player.s, g_GuiMenuFrames);
        *why = buf;
        return false;
    }
    return true;
}

static bool Check_game_over_gui(std::string* why)  { return CheckGameOverGui(false, why); }
static bool Check_game_over_menu(std::string* why) { return CheckGameOverGui(true, why); }

// gui_small_window (Tarefa 7.1b): a tela "Controles" do menu não pode ser
// cortada em janelas com menos de 600 px de altura. O hook encolhe a janela
// para 640x400 (a GUI escala para 0.67x: a lista cabe e "Voltar" fica inteiro
// na tela; "Jogar" e "Sair" não aparecem nela) e, depois que o cenário clica
// em "Voltar", para 480x270 (abaixo da escala mínima: a janela do menu ganha
// rolagem e não passa da tela; "Jogar", no topo, continua clicável). No fim
// volta a 800x600.
static int  g_SwFrame       = 0;
static bool g_SwSized400    = false; // a janela chegou a 640x400
static bool g_SwAllOn400    = false; // tela Controles em 640x400: Voltar inteiro, sem Jogar/Sair
static bool g_SwSized270    = false;
static bool g_SwJogarOn270  = false;

static void Hook_SmallWindow()
{
    ++g_SwFrame;
    GLFWwindow* window = glfwGetCurrentContext();
    int w = 0, h = 0;
    glfwGetWindowSize(window, &w, &h);
    if (g_SwFrame == 1)
        glfwSetWindowSize(window, 640, 400);
    else if (g_SwFrame == 55 && w == 640 && h == 400)
    {
        g_SwSized400 = true;
        g_SwAllOn400 = Gui_ButtonOnScreen("Voltar") && !Gui_ButtonOnScreen("Jogar") &&
                       !Gui_ButtonOnScreen("Sair");
    }
    else if (g_SwFrame == 60)
        glfwSetWindowSize(window, 480, 270);
    else if (g_SwFrame == 115 && w == 480 && h == 270)
    {
        g_SwSized270 = true;
        g_SwJogarOn270 = Gui_ButtonOnScreen("Jogar");
    }
    else if (g_SwFrame == 170)
        glfwSetWindowSize(window, 800, 600);
}

static bool Check_gui_small_window(std::string* why)
{
    char buf[200];
    if (!g_SwSized400 || !g_SwSized270)
    {
        snprintf(buf, sizeof(buf), "a janela nao foi redimensionada (640x400: %d, 480x270: %d)",
                 (int)g_SwSized400, (int)g_SwSized270);
        *why = buf;
        return false;
    }
    if (!g_SwAllOn400 || !g_SwJogarOn270)
    {
        snprintf(buf, sizeof(buf), "botoes do menu: so Voltar inteiro em 640x400 = %d, Jogar em 480x270 = %d",
                 (int)g_SwAllOn400, (int)g_SwJogarOn270);
        *why = buf;
        return false;
    }
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "o clique em Jogar (janela pequena) nao iniciou a corrida";
        return false;
    }
    return true;
}


// perf_heavy (Tarefa 7.1b): cenário pesado para medir o desempenho (quadros
// por segundo de relógio, sem vsync): o jogador entra num túnel (abóbada,
// portais e luminárias), com trens nas faixas vizinhas, uma fileira de moedas
// na faixa dele (explosões de partículas), ímã ativo, o porco voador do
// cofrinho no céu e, a cada quadro, mais uma explosão de 80 partículas (o pool
// de 512 fica cheio). O hook mede o tempo de relógio de cada quadro (depois
// dos 30 primeiros) e o final_check imprime média, pior quadro e o 1% mais
// lento; só falha se o cenário não ficou pesado (porco, pool cheio, túnel).
static double g_PerfLast      = 0.0;
static int    g_PerfFrame     = 0;
static std::vector<float> g_PerfFrameMs;
static bool   g_PerfSawPig    = false;
static bool   g_PerfSawTunnel = false;
static int    g_PerfMaxAlive  = 0;

static void SetupPerfHeavy()
{
    SetupNoSpawn();
    const TrackSegment t = FindFirstSegment(&g_Game.track, true);
    const float s0 = t.s_start;
    g_Game.player.s = s0 - 50.0f;
    const float train_s[4] = {s0 - 20.0f, s0 + 12.0f, s0 + 30.0f, s0 + 52.0f};
    for (int i = 0; i < 4; ++i)
    {
        g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::TRAIN, -1, train_s[i]));
        g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::TRAIN, 1, train_s[i] + 6.0f));
    }
    for (float s = g_Game.player.s + 6.0f; s < s0 + 40.0f; s += COIN_SPACING)
    {
        Coin coin = {0, s, COIN_HEIGHT, false};
        g_Game.coins.push_back(coin);
    }
    g_Game.powerup_timer[(int)PowerUpType::MAGNET] = POWERUP_DURATION;
    g_Game.piggy_pickups = 1;
}

static void Hook_PerfHeavy()
{
    const double now = glfwGetTime();
    if (g_PerfFrame++ > 30 && g_PerfFrame * Autotest_FixedDt() < 5.8f) // a foto (5.9 s) fica fora
        g_PerfFrameMs.push_back((float)(1000.0 * (now - g_PerfLast)));
    g_PerfLast = now;
    const TrackFrame f = g_Game.track.FrameAt(g_Game.player.s);
    g_Particles.Emit(f.position + glm::vec4(0.0f, 1.5f, 0.0f, 0.0f), 80, glm::vec3(1.0f, 0.6f, 0.2f),
                     1000u + (unsigned)g_PerfFrame, f.position.y);
    g_PerfMaxAlive = std::max(g_PerfMaxAlive, (int)g_Particles.Alive().size());
    g_PerfSawPig = g_PerfSawPig || g_Game.pig.active;
    g_PerfSawTunnel = g_PerfSawTunnel || g_Game.track.IsTunnelAt(g_Game.player.s - g_Camera.Back());
}

// Limite da média de tempo de quadro do cenário perf_heavy (ms); vale também
// para o build Debug.
static const float PERF_MAX_AVG_MS = 16.0f;

// GL_RENDERER de um OpenGL por software (Mesa llvmpipe/softpipe sob Xvfb, o
// "Microsoft Basic Render Driver" do Windows sem driver de vídeo, ...): ali o
// tempo de quadro mede a CPU, não uma GPU, e o limite de PERF_MAX_AVG_MS vira
// só um aviso (Revisão final, item 10).
static bool IsSoftwareRenderer(const char* renderer)
{
    if (renderer == NULL)
        return false;
    static const char* const MARKERS[] = {"llvmpipe", "softpipe", "software", "microsoft basic render"};
    std::string r(renderer);
    for (size_t i = 0; i < r.size(); ++i)
        r[i] = (char)std::tolower((unsigned char)r[i]);
    for (size_t k = 0; k < sizeof(MARKERS) / sizeof(MARKERS[0]); ++k)
        if (r.find(MARKERS[k]) != std::string::npos)
            return true;
    return false;
}

static bool Check_perf_heavy(std::string* why)
{
    char buf[200];
    if (g_PerfFrameMs.size() < 60 || !g_PerfSawPig || !g_PerfSawTunnel || g_PerfMaxAlive < 500)
    {
        snprintf(buf, sizeof(buf), "cenario leve demais: quadros = %d, porco = %d, tunel = %d, particulas max = %d",
                 (int)g_PerfFrameMs.size(), (int)g_PerfSawPig, (int)g_PerfSawTunnel, g_PerfMaxAlive);
        *why = buf;
        return false;
    }
    std::vector<float> ms = g_PerfFrameMs;
    std::sort(ms.begin(), ms.end());
    double total = 0.0;
    for (size_t i = 0; i < ms.size(); ++i)
        total += ms[i];
    const float avg = (float)(total / ms.size());
    const float p99 = ms[(size_t)(0.99 * (ms.size() - 1))];
    printf("E2E perf_heavy: %d quadros, media %.2f ms (%.0f fps), 1%% mais lento %.2f ms (%.0f fps), "
           "pior %.2f ms, particulas max %d\n",
           (int)ms.size(), avg, 1000.0f / avg, p99, 1000.0f / p99, ms.back(), g_PerfMaxAlive);
    // Teste de regressão (Tarefa 7.2): mesmo no build Debug (o pior caso) a média
    // precisa ficar abaixo de 16 ms (~60 fps); medido ~6.7 ms numa GTX 1070.
    if (avg > PERF_MAX_AVG_MS)
    {
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        if (IsSoftwareRenderer(renderer))
        {
            printf("E2E perf_heavy: AVISO: media de %.2f ms acima de %.1f ms, mas o OpenGL e por software "
                   "(%s): limite ignorado\n", avg, PERF_MAX_AVG_MS, renderer);
            return true;
        }
        snprintf(buf, sizeof(buf), "media de %.2f ms por quadro acima do limite de %.1f ms (%.0f fps)",
                 avg, PERF_MAX_AVG_MS, 1000.0f / avg);
        *why = buf;
        return false;
    }
    return true;
}

// visual_audit (Tarefa 7.1b): varredura de Z-fighting e texturas esticadas.
// Sem spawn, com rampa + trem na faixa 0, barreiras e placa; C liga a câmera
// livre e o hook a posiciona (StartAt) em poses fixas, uma a cada
// AUDIT_POSE_TIME s, levando o jogador junto (o cenário só existe em torno
// dele): rasante sobre trilhos/dormentes e sobre a borda leito/terreno, rampa
// de lado e de frente, teto do trem rasante, portal de perto, chão e base da
// parede dentro do túnel, prédios e terreno numa curva e o portal de saída.
// As fotos (uma por pose) são para inspeção visual; o final_check confere que
// todas as poses rodaram e que a câmera livre seguiu ativa (sem erro de GL).
struct AuditPose
{
    const char* label;
    int   where;           // 0 = largada (rampa/trem), 1 = túnel, 2 = curva
    float player_ds;       // s do jogador relativo à âncora
    float cam_ds, cam_x, cam_y;    // câmera: (s relativo, lateral, altura)
    float look_ds, look_x, look_y; // ponto olhado
};
static const float AUDIT_POSE_TIME = 0.7f;
static const float AUDIT_START     = 0.2f;
// Âncoras: s = 0 na largada; túnel: entrada (s0) e saída (s1); curva: início.
static const AuditPose AUDIT_POSES[] = {
    {"rails_grazing",    0, 10.0f, 12.0f,  1.25f, 0.30f, 30.0f,  0.0f, 0.10f},
    {"bed_edge_grazing", 0, 10.0f, 12.0f,  4.6f,  0.25f, 40.0f,  4.6f, 0.00f},
    {"ramp_side",        0, 20.0f, 26.0f, -3.6f,  1.80f, 30.0f,  0.0f, 1.20f},
    {"ramp_front_low",   0, 15.0f, 20.0f,  0.6f,  0.60f, 30.0f,  0.0f, 1.60f},
    {"barrier_closeup",  0, 15.0f, 22.5f, -1.2f,  0.50f, 26.0f, -2.5f, 0.40f},
    {"train_roof",       0, 25.0f, 35.5f,  0.6f,  3.50f, 46.0f,  0.0f, 3.20f},
    {"portal_close",     1,-15.0f, -8.0f,  3.0f,  2.00f,  0.0f,  0.0f, 3.00f},
    {"tunnel_floor",     1, 20.0f, 15.0f,  1.25f, 0.50f, 35.0f,  1.25f, 0.00f},
    {"tunnel_wall_base", 1, 20.0f, 15.0f,  3.0f,  1.00f, 20.0f,  5.0f, 0.30f},
    {"curve_buildings",  2, 20.0f, 20.0f,  0.0f, 12.00f, 40.0f, -9.0f, 3.00f},
    {"curve_ground_in",  2, 10.0f,  5.0f, -5.0f,  0.40f, 25.0f, -5.0f, 0.00f},
    {"curve_ground_out", 2, 10.0f,  5.0f, 17.0f,  0.50f, 25.0f, 17.0f, 0.00f},
    {"exit_portal",      3, 10.0f, 22.0f, -2.0f,  2.00f,  0.0f,  0.0f, 3.00f},
    // Revisão final, item 3: muros de pedra e prédios de tijolo (na largada, o
    // prédio da esquerda em s = 0 e o da direita em s = 12 são de tijolo).
    {"wall_top_grazing", 0, 10.0f, 12.0f,  5.0f,  1.75f, 40.0f,  5.85f, 1.60f},
    {"wall_end_cap",     1,-20.0f, -0.8f,  4.3f,  2.20f, -4.0f,  6.20f, 0.60f},
    {"brick_facade",     0, 10.0f,  6.0f, -3.0f,  2.50f,  0.0f, -6.0f, 3.50f},
    {"wall_brick_gap",   0, 10.0f,  4.0f,  4.8f,  1.00f, 16.0f,  6.5f, 2.00f},
    {"vault_stone",      1, 20.0f, 14.0f, -1.0f,  2.00f, 26.0f,  4.0f, 5.50f},
};
static const int AUDIT_POSE_COUNT = (int)(sizeof(AUDIT_POSES) / sizeof(AUDIT_POSES[0]));
static TrackSegment g_AuditTunnel, g_AuditCurve;
static int  g_AuditFrame    = 0;
static int  g_AuditMaxPose  = -1;
static bool g_AuditLeftFree = false;

static void SetupVisualAudit()
{
    SetupNoSpawn();
    const Obstacle train = g_Game.MakeObstacle(ObstacleType::TRAIN, 0, 40.0f);
    g_Game.obstacles.push_back(g_Game.MakeRampFor(train));
    g_Game.obstacles.push_back(train);
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::BARRIER_LOW, -1, 26.0f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::SIGN, 1, 30.0f));
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::BARRIER_HIGH, 1, 55.0f));
    Track copy = g_Game.track;
    g_AuditTunnel = FindFirstSegment(&copy, true);
    g_AuditCurve  = FindFirstSegment(&copy, false);
    // A curva usada fica longe da largada (a primeira pode ser a da rampa).
    g_Game.track.EnsureGeneratedUntil(g_AuditTunnel.s_start + g_AuditTunnel.length + 400.0f);
}

static glm::vec4 AuditPoint(float s, float x, float y)
{
    const TrackFrame f = g_Game.track.FrameAt(s);
    return f.position + f.right * x + glm::vec4(0.0f, y, 0.0f, 0.0f);
}

static void Hook_VisualAudit()
{
    ++g_AuditFrame;
    if (g_Game.mode != GameMode::FREE_CAMERA)
    {
        if (g_AuditMaxPose >= 0)
            g_AuditLeftFree = true;
        return;
    }
    const float t = g_AuditFrame * Autotest_FixedDt();
    const int k = (int)std::floor((t - AUDIT_START) / AUDIT_POSE_TIME);
    if (k < 0 || k >= AUDIT_POSE_COUNT)
        return;
    g_AuditMaxPose = std::max(g_AuditMaxPose, k);
    const AuditPose& a = AUDIT_POSES[k];
    float anchor = 0.0f;
    if (a.where == 1) anchor = g_AuditTunnel.s_start;
    if (a.where == 2) anchor = g_AuditCurve.s_start;
    if (a.where == 3) anchor = g_AuditTunnel.s_start + g_AuditTunnel.length;
    // Curva para a direita: espelha a lateral (o lado de dentro troca).
    const float mirror = (a.where == 2 && g_AuditCurve.curvature < 0.0f) ? -1.0f : 1.0f;
    g_Game.player.s = anchor + a.player_ds;
    g_Game.track.EnsureGeneratedUntil(g_Game.player.s + SPAWN_AHEAD + 50.0f);
    const glm::vec4 c = AuditPoint(anchor + a.cam_ds, mirror * a.cam_x, a.cam_y);
    const glm::vec4 l = AuditPoint(anchor + a.look_ds, mirror * a.look_x, a.look_y);
    CameraPose pose;
    pose.position = c;
    pose.view_vector = l - c;
    pose.up = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    g_FreeCamera.StartAt(pose);
}

static bool Check_visual_audit(std::string* why)
{
    char buf[160];
    if (g_AuditMaxPose != AUDIT_POSE_COUNT - 1 || g_AuditLeftFree)
    {
        snprintf(buf, sizeof(buf), "poses percorridas: %d de %d; saiu da camera livre = %d",
                 g_AuditMaxPose + 1, AUDIT_POSE_COUNT, (int)g_AuditLeftFree);
        *why = buf;
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

    // powerups: ímã, multiplicador x2, tênis e cofrinho na faixa do jogador;
    // moedas nas faixas vizinhas só são coletadas pelo ímã e valem o dobro com o
    // x2; o pulo com tênis é bem mais alto que o comum, depois de expirar.
    {
        AutotestScenario s;
        s.name        = "powerups";
        s.duration    = 16.0f;
        s.final_check = Check_powerups;
        s.setup       = SetupPowerups;
        s.frame_hook  = Hook_Powerups;
        s.steps.push_back(AutotestStep{0.3f, 0, 0, "ahead"});
        s.steps.push_back(AutotestStep{1.9f, 0, 0, "picked"});
        s.steps.push_back(AutotestStep{2.4f, 0, 0, "magnet_pull"});
        s.steps.push_back(AutotestStep{2.8f, GLFW_KEY_UP, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.35f, 0, 0, "sneakers_jump"});
        s.steps.push_back(AutotestStep{5.0f, 0, 0, "timers_mid"});
        s.steps.push_back(AutotestStep{8.0f, 0, 0, "timers_low"});
        s.steps.push_back(AutotestStep{13.0f, GLFW_KEY_UP, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{13.3f, 0, 0, "expired_jump"});
        scenarios.push_back(s);
    }

    // particles: a moeda é pega em ~1 s e o power-up em ~2.4 s; capturas logo
    // depois de cada explosão e uma ~1 s depois (já apagadas).
    {
        AutotestScenario s;
        s.name        = "particles";
        s.duration    = 4.5f;
        s.final_check = Check_particles;
        s.setup       = SetupParticles;
        s.frame_hook  = Hook_Particles;
        s.steps.push_back(AutotestStep{0.3f, 0, 0, "before"});
        s.steps.push_back(AutotestStep{1.1f, 0, 0, "coin"});
        s.steps.push_back(AutotestStep{1.35f, 0, 0, "coin_late"});
        s.steps.push_back(AutotestStep{2.55f, 0, 0, "powerup"});
        s.steps.push_back(AutotestStep{2.8f, 0, 0, "powerup_late"});
        s.steps.push_back(AutotestStep{3.8f, 0, 0, "faded"});
        scenarios.push_back(s);
    }

    // piggy: cofrinho coletado em ~0.5 s; o porco voa por PIGGY_FLIGHT_TIME s.
    // Capturas em t ~ 0.25, 0.5 e 0.9 do voo (lançado em ~0.5 s) e no fim.
    {
        AutotestScenario s;
        s.name        = "piggy";
        s.duration    = 8.0f;
        s.final_check = Check_piggy;
        s.setup       = SetupPiggy;
        s.frame_hook  = Hook_Piggy;
        s.steps.push_back(AutotestStep{0.3f, 0, 0, "before"});
        s.steps.push_back(AutotestStep{1.5f, 0, 0, "t025"});
        s.steps.push_back(AutotestStep{2.5f, 0, 0, "t050"});
        s.steps.push_back(AutotestStep{4.1f, 0, 0, "t090"});
        s.steps.push_back(AutotestStep{7.0f, 0, 0, "after"});
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

    // ramp_train: sobe a rampa, corre no teto do trem e cai no fim (sem entrada).
    {
        AutotestScenario s;
        s.name        = "ramp_train";
        s.final_check = Check_ramp_train;
        s.setup       = SetupRampTrain;
        s.frame_hook  = Hook_Roof;
        s.steps.push_back(AutotestStep{TimeToRun(12.0f), 0, 0, "approach"});
        s.steps.push_back(AutotestStep{TimeToRun(29.0f), 0, 0, "ramp"});
        s.steps.push_back(AutotestStep{TimeToRun(40.0f), 0, 0, "roof"});
        s.steps.push_back(AutotestStep{TimeToRun(48.5f), 0, 0, "falling"});
        s.steps.push_back(AutotestStep{TimeToRun(60.0f), 0, 0, "landed"});
        s.duration = TimeToRun(62.0f);
        scenarios.push_back(s);
    }

    // roof_hop: do teto do trem da faixa 0 para o do vizinho (faixa 1); no fim
    // troca duas faixas para a esquerda (faixa -1, vazia) e cai.
    {
        AutotestScenario s;
        s.name        = "roof_hop";
        s.final_check = Check_roof_hop;
        s.setup       = SetupRoofHop;
        s.frame_hook  = Hook_Roof;
        s.steps.push_back(AutotestStep{TimeToRun(36.0f), GLFW_KEY_RIGHT, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{TimeToRun(37.3f), 0, 0, "hop"});
        s.steps.push_back(AutotestStep{TimeToRun(42.0f), 0, 0, "neighbor_roof"});
        s.steps.push_back(AutotestStep{TimeToRun(44.0f), GLFW_KEY_LEFT, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{TimeToRun(44.0f), GLFW_KEY_A, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{TimeToRun(47.5f), 0, 0, "drop"});
        s.steps.push_back(AutotestStep{TimeToRun(58.0f), 0, 0, "landed"});
        s.duration = TimeToRun(60.0f);
        scenarios.push_back(s);
    }

    // ramp_side_block: troca de faixa contra a lateral alta da rampa é desfeita.
    {
        AutotestScenario s;
        s.name        = "ramp_side_block";
        s.final_check = Check_ramp_side_block;
        s.setup       = SetupRampSide;
        s.frame_hook  = Hook_RampSide;
        s.steps.push_back(AutotestStep{TimeToRun(28.0f), GLFW_KEY_RIGHT, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{TimeToRun(29.1f), 0, 0, "blocked"});
        s.steps.push_back(AutotestStep{TimeToRun(31.0f), 0, 0, "back"});
        s.steps.push_back(AutotestStep{TimeToRun(40.0f), 0, 0, "alongside"});
        s.duration = TimeToRun(42.0f);
        scenarios.push_back(s);
    }

    // free_camera: câmera livre (C) com o jogo congelado; mouse e WASD/Espaço.
    {
        AutotestScenario s;
        s.name        = "free_camera";
        s.duration    = 6.2f;
        s.final_check = Check_free_camera;
        s.setup       = SetupFreeCamera;
        s.frame_hook  = Hook_FreeCamera;
        s.steps.push_back(AutotestStep{1.9f, 0, 0, "chase"});
        s.steps.push_back(AutotestStep{2.0f, GLFW_KEY_C, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{2.1f, GLFW_KEY_W, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.1f, GLFW_KEY_W, GLFW_RELEASE, NULL});
        s.steps.push_back(AutotestStep{3.2f, 0, 0, "ahead"});
        // Sobe 0,8 s com Espaço (~8 m) e, com o mouse, gira 1047 px * 0,003 rad/px
        // = 180 graus (passa a olhar para trás, para o jogador) e 260 px para baixo.
        s.steps.push_back(AutotestStep{3.3f, GLFW_KEY_SPACE, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{4.1f, GLFW_KEY_SPACE, GLFW_RELEASE, NULL});
        s.steps.push_back(AutotestStep{4.2f, 0, 0, NULL, 1047.0f, 260.0f});
        s.steps.push_back(AutotestStep{4.4f, 0, 0, "looking_back"});
        s.steps.push_back(AutotestStep{4.5f, 0, 0, NULL, -420.0f, -60.0f});
        s.steps.push_back(AutotestStep{4.7f, 0, 0, "above_side"});
        s.steps.push_back(AutotestStep{4.8f, GLFW_KEY_C, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{5.8f, 0, 0, "resumed"});
        scenarios.push_back(s);
    }

    // menu_pause (Tarefa 6.2): menu -> Controles -> Voltar -> Controles -> ESC
    // -> Jogar; P pausa e P volta;
    // ESC pausa (sem fechar a janela) e o botão "Continuar" volta.
    {
        AutotestScenario s;
        s.name          = "menu_pause";
        s.duration      = MENU_PAUSE_DURATION;
        s.final_check   = Check_menu_pause;
        s.frame_hook    = Hook_Gui;
        s.start_in_menu = true;
        s.steps.push_back(AutotestStep{0.3f, 0, 0, "menu"});
        s.steps.push_back(AutotestStep{0.4f, 0, 0, NULL, 0, 0, "Controles"});
        s.steps.push_back(AutotestStep{0.6f, 0, 0, "controls"});
        s.steps.push_back(AutotestStep{0.7f, 0, 0, NULL, 0, 0, "Voltar"});
        s.steps.push_back(AutotestStep{0.9f, 0, 0, NULL, 0, 0, "Controles"});
        s.steps.push_back(AutotestStep{1.1f, GLFW_KEY_ESCAPE, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{1.3f, 0, 0, NULL, 0, 0, "Jogar"});
        s.steps.push_back(AutotestStep{2.0f, GLFW_KEY_P, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{2.4f, 0, 0, "paused"});
        s.steps.push_back(AutotestStep{3.2f, GLFW_KEY_P, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.6f, GLFW_KEY_ESCAPE, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.8f, 0, 0, NULL, 0, 0, "Continuar"});
        s.steps.push_back(AutotestStep{4.8f, 0, 0, "playing"});
        scenarios.push_back(s);
    }

    // focus_pause (Revisão final, item 9): perda de foco/minimização pausa.
    {
        AutotestScenario s;
        s.name        = "focus_pause";
        s.duration    = FOCUS_PAUSE_DURATION;
        s.final_check = Check_focus_pause;
        s.setup       = SetupNoSpawn;
        s.frame_hook  = Hook_FocusPause;
        s.steps.push_back(AutotestStep{1.5f, 0, 0, "paused"});
        s.steps.push_back(AutotestStep{2.0f, GLFW_KEY_P, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.0f, GLFW_KEY_C, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.8f, 0, 0, "paused_from_free_camera"});
        s.steps.push_back(AutotestStep{4.0f, 0, 0, NULL, 0, 0, "Continuar"});
        s.steps.push_back(AutotestStep{5.0f, 0, 0, NULL, 0, 0, "Continuar"});
        scenarios.push_back(s);
    }

    // pause_freeze: pausa no meio do voo do porco, com partículas e ímã ativos.
    {
        AutotestScenario s;
        s.name        = "pause_freeze";
        s.duration    = PAUSE_FREEZE_DURATION;
        s.final_check = Check_pause_freeze;
        s.setup       = SetupPauseFreeze;
        s.frame_hook  = Hook_PauseFreeze;
        s.steps.push_back(AutotestStep{1.42f, GLFW_KEY_P, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{1.6f, 0, 0, "paused"});
        s.steps.push_back(AutotestStep{2.5f, 0, 0, "still_paused"});
        s.steps.push_back(AutotestStep{2.6f, GLFW_KEY_P, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{2.9f, 0, 0, "resumed"});
        scenarios.push_back(s);
    }

    // marathon (Tarefa 7.1a): 600 s de jogo com piloto automático e spawn ligado;
    // conferem-se os limites de memória (veja Check_marathon).
    {
        AutotestScenario s;
        s.name        = "marathon";
        s.duration    = MARATHON_DURATION;
        s.final_check = Check_marathon;
        s.frame_hook  = Hook_Marathon;
        s.steps.push_back(AutotestStep{1.0f, 0, 0, "start"});
        s.steps.push_back(AutotestStep{30.0f, 0, 0, "middle"});
        s.steps.push_back(AutotestStep{59.0f, 0, 0, "end"});
        scenarios.push_back(s);
    }

    // gui_keys (Tarefa 7.1a): setas/WASD/Espaço no menu e na pausa não mexem no
    // jogador; Enter inicia; P e ESC saem da câmera livre direto para a pausa.
    {
        AutotestScenario s;
        s.name          = "gui_keys";
        s.duration      = 5.2f;
        s.final_check   = Check_gui_keys;
        s.setup         = SetupNoSpawn;
        s.frame_hook    = Hook_GuiKeys;
        s.start_in_menu = true;
        // No MENU (Espaço e Enter iniciam a corrida, por isso só setas e WASD).
        const int movers[8] = {GLFW_KEY_LEFT, GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_RIGHT,
                               GLFW_KEY_A, GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_D};
        for (int i = 0; i < 8; ++i)
        {
            s.steps.push_back(AutotestStep{0.20f + 0.04f * i, movers[i], GLFW_PRESS, NULL});
            s.steps.push_back(AutotestStep{0.22f + 0.04f * i, movers[i], GLFW_RELEASE, NULL});
        }
        s.steps.push_back(AutotestStep{0.60f, 0, 0, "menu_after_keys"});
        s.steps.push_back(AutotestStep{0.70f, GLFW_KEY_ENTER, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{0.72f, GLFW_KEY_ENTER, GLFW_RELEASE, NULL});
        s.steps.push_back(AutotestStep{1.20f, 0, 0, "started"});
        // Pausa (P) e teclas de jogo durante ela, inclusive Espaço.
        s.steps.push_back(AutotestStep{1.50f, GLFW_KEY_P, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{1.52f, GLFW_KEY_P, GLFW_RELEASE, NULL});
        const int paused_keys[9] = {GLFW_KEY_SPACE, GLFW_KEY_UP, GLFW_KEY_LEFT, GLFW_KEY_RIGHT,
                                    GLFW_KEY_DOWN, GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D};
        for (int i = 0; i < 9; ++i)
        {
            s.steps.push_back(AutotestStep{1.70f + 0.04f * i, paused_keys[i], GLFW_PRESS, NULL});
            s.steps.push_back(AutotestStep{1.72f + 0.04f * i, paused_keys[i], GLFW_RELEASE, NULL});
        }
        s.steps.push_back(AutotestStep{2.20f, 0, 0, "paused_after_keys"});
        s.steps.push_back(AutotestStep{2.40f, GLFW_KEY_P, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{2.42f, GLFW_KEY_P, GLFW_RELEASE, NULL});
        // Câmera livre -> ESC -> pausa; ESC volta.
        s.steps.push_back(AutotestStep{2.80f, GLFW_KEY_C, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{2.82f, GLFW_KEY_C, GLFW_RELEASE, NULL});
        s.steps.push_back(AutotestStep{3.00f, GLFW_KEY_ESCAPE, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.02f, GLFW_KEY_ESCAPE, GLFW_RELEASE, NULL});
        s.steps.push_back(AutotestStep{3.20f, 0, 0, "pause_from_free_esc"});
        s.steps.push_back(AutotestStep{3.40f, GLFW_KEY_ESCAPE, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.42f, GLFW_KEY_ESCAPE, GLFW_RELEASE, NULL});
        // Câmera livre -> P -> pausa; P volta.
        s.steps.push_back(AutotestStep{3.80f, GLFW_KEY_C, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{3.82f, GLFW_KEY_C, GLFW_RELEASE, NULL});
        s.steps.push_back(AutotestStep{4.00f, GLFW_KEY_P, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{4.02f, GLFW_KEY_P, GLFW_RELEASE, NULL});
        s.steps.push_back(AutotestStep{4.20f, 0, 0, "pause_from_free_p"});
        s.steps.push_back(AutotestStep{4.40f, GLFW_KEY_P, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{4.42f, GLFW_KEY_P, GLFW_RELEASE, NULL});
        s.steps.push_back(AutotestStep{5.00f, 0, 0, "resumed"});
        scenarios.push_back(s);
    }

    // gui_buttons: botões "Reiniciar", "Menu" e "Jogar" com o mouse.
    {
        AutotestScenario s;
        s.name        = "gui_buttons";
        s.duration    = 3.2f;
        s.final_check = Check_gui_buttons;
        s.setup       = SetupNoSpawn;
        s.frame_hook  = Hook_GuiButtons;
        s.steps.push_back(AutotestStep{0.5f, GLFW_KEY_ESCAPE, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{0.7f, 0, 0, NULL, 0, 0, "Reiniciar"});
        s.steps.push_back(AutotestStep{1.5f, GLFW_KEY_ESCAPE, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{1.7f, 0, 0, NULL, 0, 0, "Menu"});
        s.steps.push_back(AutotestStep{2.0f, 0, 0, "menu_again"});
        s.steps.push_back(AutotestStep{2.1f, 0, 0, NULL, 0, 0, "Jogar"});
        scenarios.push_back(s);
    }

    // game_over_gui: bate no trem; janela de fim de jogo; Enter recomeça.
    {
        AutotestScenario s;
        s.name        = "game_over_gui";
        s.duration    = 7.5f;
        s.final_check = Check_game_over_gui;
        s.setup       = SetupMovingTrain;
        s.frame_hook  = Hook_GameOverGui;
        s.steps.push_back(AutotestStep{7.0f, 0, 0, "window"});
        s.steps.push_back(AutotestStep{7.1f, GLFW_KEY_ENTER, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{7.4f, 0, 0, "after"});
        scenarios.push_back(s);
    }

    // game_over_menu: idem, mas o botão "Menu" e depois "Jogar".
    {
        AutotestScenario s;
        s.name        = "game_over_menu";
        s.duration    = 7.7f;
        s.final_check = Check_game_over_menu;
        s.setup       = SetupMovingTrain;
        s.frame_hook  = Hook_GameOverGui;
        s.steps.push_back(AutotestStep{7.0f, 0, 0, NULL, 0, 0, "Menu"});
        s.steps.push_back(AutotestStep{7.2f, 0, 0, "menu"});
        s.steps.push_back(AutotestStep{7.3f, 0, 0, NULL, 0, 0, "Jogar"});
        scenarios.push_back(s);
    }

    // gui_small_window: tela "Controles" em 640x400 e menu em 480x270 (veja
    // Hook_SmallWindow); "Jogar" clicado na janela pequena.
    {
        AutotestScenario s;
        s.name          = "gui_small_window";
        s.duration      = 3.4f;
        s.final_check   = Check_gui_small_window;
        s.frame_hook    = Hook_SmallWindow;
        s.start_in_menu = true;
        s.steps.push_back(AutotestStep{0.4f, 0, 0, NULL, 0, 0, "Controles"});
        s.steps.push_back(AutotestStep{0.85f, 0, 0, "h400_controls"});
        s.steps.push_back(AutotestStep{0.9f, 0, 0, NULL, 0, 0, "Voltar"});
        s.steps.push_back(AutotestStep{1.85f, 0, 0, "h270_menu"});
        s.steps.push_back(AutotestStep{2.0f, 0, 0, NULL, 0, 0, "Jogar"});
        s.steps.push_back(AutotestStep{2.5f, 0, 0, "h270_playing"});
        s.steps.push_back(AutotestStep{3.3f, 0, 0, "h600_playing"});
        scenarios.push_back(s);
    }

    // visual_audit: câmera livre em poses fixas (veja Hook_VisualAudit); uma
    // foto por pose, perto do fim de cada uma (a rampa de escurecimento do
    // túnel, de 0.5 s, já terminou).
    {
        AutotestScenario s;
        s.name        = "visual_audit";
        s.duration    = AUDIT_START + AUDIT_POSE_COUNT * AUDIT_POSE_TIME + 0.1f;
        s.final_check = Check_visual_audit;
        s.setup       = SetupVisualAudit;
        s.frame_hook  = Hook_VisualAudit;
        s.steps.push_back(AutotestStep{0.1f, GLFW_KEY_C, GLFW_PRESS, NULL});
        s.steps.push_back(AutotestStep{0.15f, GLFW_KEY_C, GLFW_RELEASE, NULL});
        for (int k = 0; k < AUDIT_POSE_COUNT; ++k)
            s.steps.push_back(AutotestStep{AUDIT_START + (k + 0.85f) * AUDIT_POSE_TIME, 0, 0,
                                           AUDIT_POSES[k].label});
        scenarios.push_back(s);
    }

    // perf_heavy: túnel + trens + partículas (pool cheio) + porco + ímã; mede
    // o tempo de relógio por quadro (veja Hook_PerfHeavy). Uma foto no túnel.
    {
        AutotestScenario s;
        s.name        = "perf_heavy";
        s.duration    = 6.0f;
        s.final_check = Check_perf_heavy;
        s.setup       = SetupPerfHeavy;
        s.frame_hook  = Hook_PerfHeavy;
        s.steps.push_back(AutotestStep{5.9f, 0, 0, "tunnel"});
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
float Autotest_FixedDt()   { return AUTOTEST_DT; }

void Autotest_Start()
{
    if (g_Active && g_Scenario.setup != NULL)
        g_Scenario.setup();
    if (g_Active && g_Scenario.start_in_menu)
        g_Game.mode = GameMode::MENU;
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
        if (step.mouse_dx != 0.0f || step.mouse_dy != 0.0f)
            FreeCameraMouseLook(step.mouse_dx, step.mouse_dy);
        if (step.click != NULL && !Gui_ClickButton(step.click))
        {
            Finish(window, false, std::string("botao \"") + step.click + "\" nao esta na tela");
            return;
        }
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
