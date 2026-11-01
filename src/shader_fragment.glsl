#version 330 core

// Atributos de fragmentos recebidos como entrada ("in") pelo Fragment Shader.
// Neste exemplo, este atributo foi gerado pelo rasterizador como a
// interpolação da posição global e a normal de cada vértice, definidas em
// "shader_vertex.glsl" e "main.cpp".
in vec4 position_world;
in vec4 normal;

// Posição do vértice atual no sistema de coordenadas local do modelo.
in vec4 position_model;

// Coordenadas de textura obtidas do arquivo OBJ (se existirem!)
in vec2 texcoords;

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Identificadores de objeto (uniform "object_id") e modos de UV (uniform
// "uv_mode"). ATENÇÃO: espelham include/object_ids.h; manter em sincronia.
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

#define UV_OBJ        0 // UV vindas do arquivo OBJ
#define UV_PLANAR_XZ  1 // Projeção planar em XZ do mundo
#define UV_BBOX_XY    2 // Projeção planar em XY do modelo, normalizada pela AABB
#define UV_SPHERICAL  3 // Projeção esférica em coordenadas do modelo

uniform int object_id;
uniform int uv_mode;
uniform float uv_scale; // Número de repetições da textura (multiplica U e V)

// Parâmetros da axis-aligned bounding box (AABB) do modelo
uniform vec4 bbox_min;
uniform vec4 bbox_max;

// Textura do objeto atual: sempre ligada na unidade 0 antes de cada desenho
// (veja DrawObject() em main.cpp).
uniform sampler2D TextureImage0;

// O valor de saída ("out") de um Fragment Shader é a cor final do fragmento.
out vec4 color;

// Constantes
#define M_PI   3.14159265358979323846
#define M_PI_2 1.57079632679489661923

void main()
{
    // Obtemos a posição da câmera utilizando a inversa da matriz que define o
    // sistema de coordenadas da câmera.
    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;

    // O fragmento atual é coberto por um ponto que percente à superfície de um
    // dos objetos virtuais da cena. Este ponto, p, possui uma posição no
    // sistema de coordenadas global (World coordinates). Esta posição é obtida
    // através da interpolação, feita pelo rasterizador, da posição de cada
    // vértice.
    vec4 p = position_world;

    // Normal do fragmento atual, interpolada pelo rasterizador a partir das
    // normais de cada vértice.
    vec4 n = normalize(normal);

    // Vetor que define o sentido da fonte de luz em relação ao ponto atual.
    vec4 l = normalize(vec4(1.0,1.0,0.0,0.0));

    // Vetor que define o sentido da câmera em relação ao ponto atual.
    vec4 v = normalize(camera_position - p);

    // Coordenadas de textura U e V
    float U = 0.0;
    float V = 0.0;

    if ( uv_mode == UV_SPHERICAL )
    {
        // Projeção esférica EM COORDENADAS DO MODELO, com a esfera de projeção
        // centrada em "bbox_center". Veja slides 134-150 do documento
        // Aula_20_Mapeamento_de_Texturas.pdf.
        vec4 bbox_center = (bbox_min + bbox_max) / 2.0;
        vec4 d = position_model - bbox_center;

        float rho   = length(d);
        float theta = atan(d.x,d.z);
        float phi   = asin(d.y / rho);

        U = (theta + M_PI) / 2.0 / M_PI;
        V = (phi + M_PI_2) / M_PI;
    }
    else if ( uv_mode == UV_BBOX_XY )
    {
        // Projeção planar XY em COORDENADAS DO MODELO, normalizada pela AABB
        // para ficar em [0,1]. Veja slides 99-104 e 158-160 do documento
        // Aula_20_Mapeamento_de_Texturas.pdf.
        U = (position_model.x - bbox_min.x) / (bbox_max.x - bbox_min.x);
        V = (position_model.y - bbox_min.y) / (bbox_max.y - bbox_min.y);
    }
    else if ( uv_mode == UV_PLANAR_XZ )
    {
        // Projeção planar em XZ de COORDENADAS DO MUNDO: 1 unidade de mundo =
        // 1 repetição da textura, então o chão nunca estica, não importa o
        // tamanho da malha.
        U = position_world.x;
        V = position_world.z;
    }
    else // UV_OBJ
    {
        // Coordenadas de textura obtidas do arquivo OBJ.
        U = texcoords.x;
        V = texcoords.y;
    }

    // Refletância difusa lida da textura do objeto (unidade 0). O sampler usa
    // GL_REPEAT, então uv_scale > 1 repete a imagem em vez de esticá-la.
    vec3 Kd0 = texture(TextureImage0, vec2(U,V) * uv_scale).rgb;

    // Equação de Iluminação
    float lambert = max(0,dot(n,l));

    color.rgb = Kd0 * (lambert + 0.01);

    // NOTE: Se você quiser fazer o rendering de objetos transparentes, é
    // necessário:
    // 1) Habilitar a operação de "blending" de OpenGL logo antes de realizar o
    //    desenho dos objetos transparentes, com os comandos abaixo no código C++:
    //      glEnable(GL_BLEND);
    //      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // 2) Realizar o desenho de todos objetos transparentes *após* ter desenhado
    //    todos os objetos opacos; e
    // 3) Realizar o desenho de objetos transparentes ordenados de acordo com
    //    suas distâncias para a câmera (desenhando primeiro objetos
    //    transparentes que estão mais longe da câmera).
    // Alpha default = 1 = 100% opaco = 0% transparente
    color.a = 1;

    // Cor final com correção gamma, considerando monitor sRGB.
    // Veja https://en.wikipedia.org/w/index.php?title=Gamma_correction&oldid=751281772#Windows.2C_Mac.2C_sRGB_and_TV.2Fvideo_standard_gammas
    color.rgb = pow(color.rgb, vec3(1.0,1.0,1.0)/2.2);
} 

