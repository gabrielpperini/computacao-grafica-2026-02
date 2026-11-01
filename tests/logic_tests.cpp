// Testes unitários dos módulos de lógica (sem OpenGL/GLFW). Cada módulo novo
// adiciona suas funções Test_*() e as chama em main().
// Os asserts precisam estar sempre ativos, mesmo em builds com NDEBUG.
#undef NDEBUG
#include <cassert>
#include <cstdio>

#include "game_config.h"

int main()
{
    printf("OK\n");
    return 0;
}
