#ifndef _OBJECT_IDS_H
#define _OBJECT_IDS_H

// Identificadores de objeto e modos de UV compartilhados entre C++ e GLSL.
// ATENÇÃO: estes valores são espelhados no topo de src/shader_fragment.glsl;
// alterar um exige alterar o outro. Por isso são macros (#define), e não
// constantes C++, para que a lista possa ser copiada literalmente para o shader.

// ---- IDs de objeto (uniform "object_id") ----
#define PLAYER             0
#define GROUND             1
#define RAIL               2
#define BUILDING           3
#define BARRIER_LOW        4
#define BARRIER_HIGH       5
#define SIGN               6
#define TRAIN              7
#define RAMP               8
#define TUNNEL             9
#define COIN              10
#define POWERUP_MAGNET    11
#define POWERUP_SNEAKERS  12
#define POWERUP_MULTIPLIER 13
#define POWERUP_PIGGY     14
#define FLYING_PIG        15
#define PARTICLE          16

// ---- Modos de geração de coordenadas de textura (uniform "uv_mode") ----
#define UV_OBJ        0 // UV vindas do arquivo OBJ (atributo texcoords)
#define UV_PLANAR_XZ  1 // Projeção planar em XZ do mundo: chão/paredes sem esticar
#define UV_BBOX_XY    2 // Projeção planar em XY do modelo, normalizada pela AABB
#define UV_SPHERICAL  3 // Projeção esférica em coordenadas do modelo (esferas sem UV)

#endif // _OBJECT_IDS_H
