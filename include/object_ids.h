#ifndef _OBJECT_IDS_H
#define _OBJECT_IDS_H

// Identificadores de objeto e modos de UV compartilhados entre C++ e GLSL.
// ATENÇÃO: estes valores são espelhados no topo de src/shader_fragment.glsl;
// alterar um exige alterar o outro. "const int X = n;" tem a mesma sintaxe em
// C++ e em GLSL, então a lista é copiada literalmente para o shader. (Não são
// macros #define porque BARRIER_LOW, SIGN, TRAIN etc. também são nomes de
// ObstacleType em game.h, e uma macro os substituiria dentro do enum.)

// ---- IDs de objeto (uniform "object_id") ----
const int PLAYER             = 0;
const int GROUND             = 1;
const int RAIL               = 2;
const int BUILDING           = 3;
const int BARRIER_LOW        = 4;
const int BARRIER_HIGH       = 5;
const int SIGN               = 6;
const int TRAIN              = 7;
const int RAMP               = 8;
const int TUNNEL             = 9;
const int COIN              = 10;
const int POWERUP_MAGNET    = 11;
const int POWERUP_SNEAKERS  = 12;
const int POWERUP_MULTIPLIER = 13;
const int POWERUP_PIGGY     = 14;
const int FLYING_PIG        = 15;
const int PARTICLE          = 16;
const int LAMP              = 17; // cabeça emissiva do poste de luz
const int LAMP_POLE         = 18; // haste do poste de luz
const int SLEEPER           = 19; // dormentes de madeira sob os trilhos
const int WALL              = 20; // muros de pedra e paredes dos prédios de tijolo

// ---- Modos de geração de coordenadas de textura (uniform "uv_mode") ----
const int UV_OBJ        = 0; // UV vindas do arquivo OBJ (atributo texcoords)
const int UV_PLANAR_XZ  = 1; // Projeção planar em XZ do mundo: chão/paredes sem esticar
const int UV_BBOX_XY    = 2; // Projeção planar em XY do modelo, normalizada pela AABB
const int UV_SPHERICAL  = 3; // Projeção esférica em coordenadas do modelo (esferas sem UV)
const int UV_BOX        = 4; // Projeção "caixa" em metros do objeto: caixas escaladas sem esticar

#endif // _OBJECT_IDS_H
