#ifndef _AUTOTEST_H
#define _AUTOTEST_H

// Harness de testes ponta a ponta (E2E): o executável real roda um cenário
// scriptado com dt fixo, teclas injetadas, screenshots e checagem de erros de
// GL. Uso: "main --autotest <cenario>" (a partir de bin/<plataforma>/) ou
// "main --autotest-list". Veja o topo de src/autotest.cpp para criar cenários.

#include <string>
#include <vector>

#include <GLFW/glfw3.h>

// Uma ação do script: no instante simulado 'time' (s) injeta a tecla 'key' com
// 'action' (GLFW_PRESS / GLFW_RELEASE) e/ou, se 'screenshot' != NULL, salva o
// quadro atual em e2e/<cenario>_<screenshot>.png. Use key = 0 para só tirar foto.
struct AutotestStep
{
    float       time;
    int         key;
    int         action;
    const char* screenshot;
};

struct AutotestScenario
{
    const char*                name;
    float                      duration;    // tempo simulado total (s)
    std::vector<AutotestStep>  steps;       // em ordem crescente de 'time'
    bool (*final_check)(std::string* why) = NULL;  // NULL = só "não crashou, sem erro de GL"
    // Opcional: configura o jogo antes do 1º quadro (ex.: desligar o spawn de
    // obstáculos). Chamado por Autotest_Start().
    void (*setup)() = NULL;
    // Opcional: chamado a cada quadro (antes das teclas do quadro), para
    // registrar estados intermediários que o final_check confere no fim.
    void (*frame_hook)() = NULL;
};

// Procura "--autotest <nome>" em argv. Retorna true se o cenário existe.
// Sem a opção, retorna false (modo normal). Com nome desconhecido imprime
// "E2E <nome> FAIL: ..." e encerra o processo com código 1; "--autotest-list"
// imprime os nomes dos cenários e encerra com 0. Chamar antes de glfwInit().
bool  Autotest_Begin(int argc, char* argv[]);
bool  Autotest_Active();
float Autotest_FixedDt();   // 1/60 s: no autotest o dt é fixo (execução determinística)

// Chamar uma vez, depois de criar/configurar o jogo e antes do 1º quadro:
// roda o 'setup' do cenário (se houver).
void  Autotest_Start();

// Chamar uma vez por quadro, DEPOIS de desenhar e ANTES de glfwSwapBuffers().
// 'sim_time' é o tempo simulado acumulado (soma dos dt). Injeta as teclas
// vencidas, tira screenshots, checa glGetError e, ao fim do cenário, imprime
// "E2E <nome> OK" / "E2E <nome> FAIL: <motivo>" e pede o fechamento da janela.
// Falha também se, no fim, sobrar algum passo não executado (tempo > duração).
void  Autotest_AfterFrame(GLFWwindow* window, float sim_time);

// Código de saída do processo (0 = OK, 1 = falha). Retornar de main().
// Se a janela fechou antes do fim do cenário, imprime
// "E2E <nome> FAIL: janela fechada antes do fim" e devolve 1.
int   Autotest_ExitCode();

#endif // _AUTOTEST_H
