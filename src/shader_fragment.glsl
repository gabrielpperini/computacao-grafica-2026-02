#version 330 core

// Atributos de fragmentos recebidos como entrada ("in") pelo Fragment Shader.
// Neste exemplo, este atributo foi gerado pelo rasterizador como a
// interpolação da posição global e a normal de cada vértice, definidas em
// "shader_vertex.glsl" e "main.cpp".
in vec4 position_world;
in vec4 normal;

// Normal do vértice no sistema de coordenadas local do modelo.
in vec4 normal_model;

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

const int UV_OBJ        = 0; // UV vindas do arquivo OBJ
const int UV_PLANAR_XZ  = 1; // Projeção planar em XZ do mundo
const int UV_BBOX_XY    = 2; // Projeção planar em XY do modelo, normalizada pela AABB
const int UV_SPHERICAL  = 3; // Projeção esférica em coordenadas do modelo
const int UV_BOX        = 4; // Projeção "caixa" em metros do objeto

uniform int object_id;
uniform int uv_mode;
uniform float uv_scale; // Número de repetições da textura (multiplica U e V)

// Parâmetros da axis-aligned bounding box (AABB) do modelo
uniform vec4 bbox_min;
uniform vec4 bbox_max;

// Textura do objeto atual: sempre ligada na unidade 0 antes de cada desenho
// (veja DrawObject() em main.cpp).
uniform sampler2D TextureImage0;

// Iluminação por vértice (Gouraud), vinda de shader_vertex.glsl.
in vec3 gouraud_diffuse;
in vec3 gouraud_specular;

// Modo de sombreamento do objeto atual: 0 = Phong, 1 = Gouraud.
uniform int shading_mode;

// Material do objeto atual (ver MaterialFor() em main.cpp): multiplicador da
// cor da textura (Kd = textura * material_kd_tint) e cor emissiva (lâmpadas).
uniform vec3 material_kd_tint;
uniform vec3 material_emissive;

// ---------------------------------------------------------------------------
// Iluminação Blinn-Phong: ambiente + difuso + especular, com um sol (luz
// direcional) e até MAX_POINT_LIGHTS luzes pontuais (lâmpadas dos postes e
// do teto dos túneis).
//   cor = Kd * (ambiente + Σ I·max(0,n·l)) + Ks * Σ I·max(0,n·h)^q,
//   h = normalize(l + v);  luzes pontuais atenuadas por 1/(1 + 0.09d + 0.032d²).
// Dentro de um túnel (SkyVisibility = 0) o sol não chega e o ambiente cai.
// ATENÇÃO: este bloco (até o fim de BlinnPhong) é duplicado em
// shader_vertex.glsl e shader_fragment.glsl (GLSL não tem include):
// manter idêntico ao shader_vertex.glsl.
// ---------------------------------------------------------------------------
// MAX_POINT_LIGHTS e MAX_TUNNELS NÃO são definidos aqui: LoadShader() (main.cpp)
// insere os #define logo depois do #version, com os valores do C++
// (MAX_POINT_LIGHTS em scenery.h e MAX_SHADER_TUNNELS em main.cpp): uma fonte só.

uniform vec4 sun_direction;   // sentido do ponto da superfície PARA o sol (w = 0)
uniform vec3 sun_color;
uniform vec3 ambient_color;
uniform int  num_point_lights;
uniform vec4 point_light_position[MAX_POINT_LIGHTS]; // coordenadas de mundo (w = 1)
uniform vec3 point_light_color[MAX_POINT_LIGHTS];
uniform vec3  material_ks;    // refletância especular Ks do objeto atual
uniform float material_q;     // expoente especular q do objeto atual

// Túneis visíveis (Tarefa 4.2; veja main.cpp e include/scenery.h). Cada um é
// um trecho reto: começa em tunnel_start[i].xyz (ponto da pista, no chão, na
// face de fora do portal de entrada), segue tunnel_forward[i].xyz (unitário,
// horizontal) por tunnel_start[i].w metros (até a face de fora do portal de
// saída: o vão dos portais também fica na sombra, Tarefa 7.1b). Seção:
// tunnel_shape.x = meia largura e
// tunnel_shape.z = raio da abóbada, ambos medidos até o MEIO da parede (a face
// interna fica dentro, a externa fora); tunnel_shape.y = altura das paredes
// retas; tunnel_shape.w = fração do ambiente que resta lá dentro.
uniform int  num_tunnels;
uniform vec4 tunnel_start[MAX_TUNNELS];
uniform vec4 tunnel_forward[MAX_TUNNELS];
uniform vec4 tunnel_shape;

// 1 = ponto ao ar livre; 0 = ponto dentro do volume de algum túnel (o sol e o
// céu não o alcançam: é a "sombra" do túnel, inclusive vista de fora).
float SkyVisibility(vec3 p)
{
    for ( int i = 0; i < num_tunnels && i < MAX_TUNNELS; ++i )
    {
        vec3 d = p - tunnel_start[i].xyz;
        vec3 f = tunnel_forward[i].xyz;
        float u = dot(d, f);                 // distância ao longo do túnel
        if ( u < 0.0 || u > tunnel_start[i].w )
            continue;
        float x = d.z * f.x - d.x * f.z;     // lateral: right = (-f.z, 0, f.x)
        float y = d.y;
        bool inside = (y < tunnel_shape.y) ? (abs(x) < tunnel_shape.x)
                                           : (length(vec2(x, y - tunnel_shape.y)) < tunnel_shape.z);
        if ( inside )
            return 0.0;
    }
    return 1.0;
}

// Soma a contribuição de uma luz (l = sentido PARA a luz, normalizado; I = cor
// já atenuada) aos termos difuso e especular. O especular só existe quando a
// luz ilumina a face (n·l > 0), e h só é normalizado se não for degenerado
// (l = -v daria h = 0 e NaN).
void AddLight(vec3 l, vec3 I, vec3 n, vec3 v, inout vec3 diffuse, inout vec3 specular)
{
    float ndotl = dot(n, l);
    if ( ndotl <= 0.0 )
        return;
    diffuse += I * ndotl;
    vec3 h = l + v;
    float hl = length(h);
    if ( hl > 1e-5 )
        specular += material_ks * I * pow(max(0.0, dot(n, h / hl)), material_q);
}

// p, n, v em coordenadas de mundo (n e v normalizados). diffuse já inclui o
// termo ambiente; specular já inclui Ks.
void BlinnPhong(vec3 p, vec3 n, vec3 v, out vec3 diffuse, out vec3 specular)
{
    float sky = SkyVisibility(p);
    diffuse  = ambient_color * mix(tunnel_shape.w, 1.0, sky);
    specular = vec3(0.0);

    AddLight(normalize(sun_direction.xyz), sun_color * sky, n, v, diffuse, specular);

    for ( int i = 0; i < num_point_lights && i < MAX_POINT_LIGHTS; ++i )
    {
        vec3 d = point_light_position[i].xyz - p;
        float dist = length(d);
        if ( dist < 1e-4 )
            continue;
        float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
        AddLight(d / dist, point_light_color[i] * attenuation, n, v, diffuse, specular);
    }
}

// O valor de saída ("out") de um Fragment Shader é a cor final do fragmento.
out vec4 color;

// Constantes
#define M_PI   3.14159265358979323846
#define M_PI_2 1.57079632679489661923

// Partículas de coleta (Tarefa 6.1; object_id == PARTICLE): cor (uniform
// material_kd_tint) e alfa global (particle_alpha = life/max_life da partícula).
uniform float particle_alpha;

// Névoa por distância (Tarefa 7.1b): o cenário só existe até ~150 m à frente;
// a névoa leva a cor do objeto para a do céu (fog_color = cor de fundo do
// glClearColor, já em espaço de exibição) entre fog_params.x e fog_params.y
// metros da câmera, escondendo o fim do cenário. fog_params.z = rampa da câmera
// dentro do túnel (g_TunnelDimming): com ela lá dentro, os pontos também dentro
// do túnel não recebem névoa (o ar entre eles é o do túnel, escuro).
uniform vec3 fog_color;
uniform vec3 fog_params;

void main()
{
    // Partículas: sprite redondo PROCEDURAL (textura gerada aqui, sem imagem),
    // desenhado com blending aditivo (GL_SRC_ALPHA, GL_ONE) pelo main.cpp. O
    // quad tem UV em [0,1]^2; d = distância ao centro em [0,1] na borda do
    // círculo inscrito. O brilho cai suavemente (smoothstep) até 0 na borda, com
    // um núcleo mais quente no meio. Não é iluminado: partículas brilham sozinhas.
    if ( object_id == PARTICLE )
    {
        float d = length(texcoords - vec2(0.5)) * 2.0;
        float glow = 1.0 - smoothstep(0.25, 1.0, d);   // disco com borda suave
        float core = 1.0 - smoothstep(0.0, 0.55, d);   // centro mais quente
        vec3 c = material_kd_tint * (0.9 + 0.8 * core);
        color.rgb = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
        color.a = glow * particle_alpha;
        return;
    }

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
    vec4 n = normal;

    // Vetor que define o sentido da câmera em relação ao ponto atual.
    vec4 v = camera_position - p;
    float vlen = length(v);
    v = (vlen > 1e-6) ? v / vlen : vec4(0.0, 0.0, 1.0, 0.0);

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
    else if ( uv_mode == UV_BOX )
    {
        // Projeção "caixa" (box mapping) em METROS no referencial do objeto.
        // A escala de cada eixo é o comprimento da coluna correspondente da
        // matriz model (model = T*R*S*...), então uma caixa escalada de forma
        // não uniforme (prédio 6x12x10 m) recebe a textura com a mesma
        // densidade em todas as faces, sem esticar. A face é escolhida pelo
        // eixo dominante da normal em coordenadas do modelo; V parte da base
        // da AABB (bbox_min). uv_scale = repetições por metro.
        vec3 scale = vec3(length(model[0].xyz), length(model[1].xyz), length(model[2].xyz));
        vec3 q = (position_model.xyz - bbox_min.xyz) * scale;
        vec3 an = abs(normal_model.xyz);
        if ( an.x >= an.y && an.x >= an.z )
        {
            U = q.z; V = q.y; // faces laterais (normal ±X)
        }
        else if ( an.z >= an.y )
        {
            U = q.x; V = q.y; // faces da frente/trás (normal ±Z)
        }
        else
        {
            U = q.x; V = q.z; // topo/base (normal ±Y)
        }
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

    // Kd do objeto: textura modulada pelo tom do material.
    vec3 Kd = Kd0 * material_kd_tint;

    // Equação de iluminação (Blinn-Phong). Phong: ambiente, difuso e especular
    // calculados aqui, por fragmento, com a normal interpolada. Gouraud: o
    // vertex shader já calculou ambiente+difuso e especular por vértice.
    vec3 diffuse;
    vec3 specular;
    if ( shading_mode == 1 )
    {
        diffuse  = gouraud_diffuse;
        specular = gouraud_specular;
    }
    else
    {
        vec3 nn = n.xyz;
        float nl = length(nn);
        nn = (nl > 1e-6) ? nn / nl : vec3(0.0, 1.0, 0.0);
        BlinnPhong(p.xyz, nn, v.xyz, diffuse, specular);
    }

    color.rgb = Kd * diffuse + specular + material_emissive;

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
    color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(1.0,1.0,1.0)/2.2);

    // Névoa (depois da correção gamma: fog_color está no mesmo espaço do
    // glClearColor, então no fim da rampa o objeto some exatamente no céu).
    float fog = smoothstep(fog_params.x, fog_params.y, vlen);
    if ( fog > 0.0 && fog_params.z > 0.0 )
        fog *= mix(1.0, SkyVisibility(p.xyz), fog_params.z);
    color.rgb = mix(color.rgb, fog_color, fog);
} 

