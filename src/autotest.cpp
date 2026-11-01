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

// Definida em main.cpp: o mesmo tratamento de teclas que o KeyCallback usa.
void HandleKey(GLFWwindow* window, int key, int action, int mods);

// ---------------------------------------------------------------------------
// Cenários
// ---------------------------------------------------------------------------

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
// Se a janela foi fechada antes do fim do cenário, conta como falha.
int   Autotest_ExitCode()  { return g_Finished ? g_ExitCode : 1; }

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

void Autotest_AfterFrame(GLFWwindow* window, float sim_time)
{
    if (!g_Active || g_Finished)
        return;

    std::string why;

    // Erro de GL gerado neste quadro (ou antes, desde o último glGetError).
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "erro de GL 0x%04X no tempo %.3f s", (unsigned)err, sim_time);
        Finish(window, false, buf);
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
        if (g_Scenario.final_check != NULL && !g_Scenario.final_check(&why))
        {
            Finish(window, false, why);
            return;
        }
        Finish(window, true, "");
    }
}
