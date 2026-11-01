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
#include "game.h"

// Definida em main.cpp: o mesmo tratamento de teclas que o KeyCallback usa.
void HandleKey(GLFWwindow* window, int key, int action, int mods);

// Estado do jogo, definido em main.cpp (semente fixa 12345 no autotest).
extern Game g_Game;

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
    g_Game.obstacles.push_back(g_Game.MakeObstacle(ObstacleType::TRAIN, -1, 30.0f));
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
