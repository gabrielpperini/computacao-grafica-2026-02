#version 330 core

// Atributos de vértice recebidos como entrada ("in") pelo Vertex Shader.
// Veja a função BuildTrianglesAndAddToVirtualScene() em "main.cpp".
layout (location = 0) in vec4 model_coefficients;
layout (location = 1) in vec4 normal_coefficients;
layout (location = 2) in vec2 texture_coefficients;

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Atributos de vértice que serão gerados como saída ("out") pelo Vertex Shader.
// ** Estes serão interpolados pelo rasterizador! ** gerando, assim, valores
// para cada fragmento, os quais serão recebidos como entrada pelo Fragment
// Shader. Veja o arquivo "shader_fragment.glsl".
out vec4 position_world;
out vec4 position_model;
out vec4 normal;
out vec4 normal_model;
out vec2 texcoords;

// Iluminação por vértice (Gouraud), usada só quando shading_mode == 1 (moedas):
// o fragment shader multiplica o difuso pela textura e soma o especular.
out vec3 gouraud_diffuse;
out vec3 gouraud_specular;

// Modo de sombreamento do objeto atual: 0 = Phong (por fragmento), 1 = Gouraud
// (por vértice, calculado neste shader).
uniform int shading_mode;

// ---------------------------------------------------------------------------
// Iluminação Blinn-Phong: ambiente + difuso + especular, com um sol (luz
// direcional) e até MAX_POINT_LIGHTS luzes pontuais (lâmpadas dos postes e
// do teto dos túneis).
//   cor = Kd * (ambiente + Σ I·max(0,n·l)) + Ks * Σ I·max(0,n·h)^q,
//   h = normalize(l + v);  luzes pontuais atenuadas por 1/(1 + 0.09d + 0.032d²).
// Dentro de um túnel (SkyVisibility = 0) o sol não chega e o ambiente cai.
// ATENÇÃO: este bloco (até o fim de BlinnPhong) é duplicado em
// shader_vertex.glsl e shader_fragment.glsl (GLSL não tem include):
// manter idêntico ao shader_fragment.glsl.
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

void main()
{
    // A variável gl_Position define a posição final de cada vértice
    // OBRIGATORIAMENTE em "normalized device coordinates" (NDC), onde cada
    // coeficiente estará entre -1 e 1 após divisão por w.
    // Veja {+NDC2+}.
    //
    // O código em "main.cpp" define os vértices dos modelos em coordenadas
    // locais de cada modelo (array model_coefficients). Abaixo, utilizamos
    // operações de modelagem, definição da câmera, e projeção, para computar
    // as coordenadas finais em NDC (variável gl_Position). Após a execução
    // deste Vertex Shader, a placa de vídeo (GPU) fará a divisão por W. Veja
    // slides 41-67 e 69-86 do documento Aula_09_Projecoes.pdf.

    gl_Position = projection * view * model * model_coefficients;

    // Como as variáveis acima  (tipo vec4) são vetores com 4 coeficientes,
    // também é possível acessar e modificar cada coeficiente de maneira
    // independente. Esses são indexados pelos nomes x, y, z, e w (nessa
    // ordem, isto é, 'x' é o primeiro coeficiente, 'y' é o segundo, ...):
    //
    //     gl_Position.x = model_coefficients.x;
    //     gl_Position.y = model_coefficients.y;
    //     gl_Position.z = model_coefficients.z;
    //     gl_Position.w = model_coefficients.w;
    //

    // Agora definimos outros atributos dos vértices que serão interpolados pelo
    // rasterizador para gerar atributos únicos para cada fragmento gerado.

    // Posição do vértice atual no sistema de coordenadas global (World).
    position_world = model * model_coefficients;

    // Posição do vértice atual no sistema de coordenadas local do modelo.
    position_model = model_coefficients;

    // Normal do vértice atual no sistema de coordenadas global (World).
    // Veja slides 123-151 do documento Aula_07_Transformacoes_Geometricas_3D.pdf.
    normal = inverse(transpose(model)) * normal_coefficients;
    normal.w = 0.0;

    // Normal do vértice no sistema de coordenadas local do modelo (usada pela
    // projeção de textura UV_BOX em shader_fragment.glsl).
    normal_model = normal_coefficients;

    // Coordenadas de textura obtidas do arquivo OBJ (se existirem!)
    texcoords = texture_coefficients;

    // Gouraud: avalia o modelo de iluminação aqui, nos vértices; o rasterizador
    // interpola o resultado. No modo Phong nada é calculado (o fragment shader
    // refaz a conta por fragmento).
    gouraud_diffuse  = vec3(0.0);
    gouraud_specular = vec3(0.0);
    if ( shading_mode == 1 )
    {
        vec3 n = normal.xyz;
        float nl = length(n);
        n = (nl > 1e-6) ? n / nl : vec3(0.0, 1.0, 0.0);

        vec4 camera_position = inverse(view) * vec4(0.0, 0.0, 0.0, 1.0);
        vec3 v = camera_position.xyz - position_world.xyz;
        float vl = length(v);
        v = (vl > 1e-6) ? v / vl : vec3(0.0, 0.0, 1.0);

        BlinnPhong(position_world.xyz, n, v, gouraud_diffuse, gouraud_specular);
    }
}

