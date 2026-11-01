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

// lane_jump_roll: troca para a direita, pulo, rolamento e duas vezes para a
// esquerda; no fim o jogador está na faixa -1, no chão, ainda jogando.
static bool Check_lane_jump_roll(std::string* why)
{
    char buf[160];
    const Player& p = g_Game.player;
    if (g_Game.mode != GameMode::PLAYING)
    {
        *why = "modo de jogo diferente de PLAYING";
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
        s.steps.push_back(AutotestStep{30.0f, 0, 0, "t30"});
        s.steps.push_back(AutotestStep{59.5f, 0, 0, "t60"});
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
