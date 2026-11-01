//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//    INF01047 Computação Gráfica e Visualização I
//               Prof. Eduardo Gastal
//
//     CÓDIGO BASE PARA O TRABALHO FINAL
//

// Arquivos "headers" padrões de C podem ser incluídos em um
// programa C++, sendo necessário somente adicionar o caractere
// "c" antes de seu nome, e remover o sufixo ".h". Exemplo:
//    #include <stdio.h> // Em C
//  vira
//    #include <cstdio> // Em C++
//
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Headers abaixo são específicos de C++
#include <set>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"
#include "game_config.h"
#include "object_ids.h"
#include "autotest.h"
#include "game.h"
#include "camera.h"
#include "scenery.h"

// Estrutura que representa um modelo geométrico carregado a partir de um
// arquivo ".obj". Veja https://en.wikipedia.org/wiki/Wavefront_.obj_file .
struct ObjModel
{
    tinyobj::attrib_t                 attrib;
    std::vector<tinyobj::shape_t>     shapes;
    std::vector<tinyobj::material_t>  materials;

    // Este construtor lê o modelo de um arquivo utilizando a biblioteca tinyobjloader.
    // Veja: https://github.com/syoyo/tinyobjloader
    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true)
    {
        printf("Carregando objetos do arquivo \"%s\"...\n", filename);

        // Se basepath == NULL, então setamos basepath como o dirname do
        // filename, para que os arquivos MTL sejam corretamente carregados caso
        // estejam no mesmo diretório dos arquivos OBJ.
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL)
        {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos)
            {
                dirname = fullpath.substr(0, i+1);
                basepath = dirname.c_str();
            }
        }

        std::string warn;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        for (size_t shape = 0; shape < shapes.size(); ++shape)
        {
            if (shapes[shape].name.empty())
            {
                fprintf(stderr,
                        "*********************************************\n"
                        "Erro: Objeto sem nome dentro do arquivo '%s'.\n"
                        "Veja https://www.inf.ufrgs.br/~eslgastal/fcg-faq-etc.html#Modelos-3D-no-formato-OBJ .\n"
                        "*********************************************\n",
                    filename);
                throw std::runtime_error("Objeto sem nome.");
            }
            printf("- Objeto '%s'\n", shapes[shape].name.c_str());
        }

        printf("OK.\n");
    }
};


// Declaração de funções utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4& M);

// Calcula o Δt do quadro atual (em segundos), limitado a MAX_DT, e atualiza *last.
float ComputeFrameDt(double now, double* last);

// Declaração de várias funções utilizadas em main().  Essas estão definidas
// logo após a definição de main() neste arquivo.
void BuildTrianglesAndAddToVirtualScene(ObjModel*); // Constrói representação de um ObjModel como malha de triângulos para renderização
void ComputeNormals(ObjModel* model); // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles(); // Carrega os shaders de vértice e fragmento, criando um programa de GPU
GLuint LoadTextureImage(const char* filename); // Carrega uma imagem de textura e devolve o id da textura OpenGL
void DrawVirtualObject(const char* object_name); // Desenha um objeto armazenado em g_VirtualScene
// Desenha um objeto da cena com sua matriz de modelagem, id, textura (ligada na unidade 0) e modo de UV
void DrawObject(const char* name, const glm::mat4& model, int object_id, GLuint texture, int uv_mode, float uv_scale = 1.0f);
GLuint LoadShader_Vertex(const char* filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char* filename); // Carrega um fragment shader
void LoadShader(const char* filename, GLuint shader_id); // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU
void PrintObjModelInfo(ObjModel*); // Função para debugging
// Dimensões (bbox_max - bbox_min) de um objeto de g_VirtualScene, em coordenadas de modelo.
glm::vec3 ObjectSize(const char* name);
// Escala o objeto por (sx,sy,sz) com a base da sua AABB centrada na origem e
// apoiada em y = 0 (para posicionar "em pé" sobre a pista).
glm::mat4 Matrix_OnGround(const char* name, float sx, float sy, float sz);

// Declaração de funções auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funções estão definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow* window);
float TextRendering_CharWidth(GLFWwindow* window);
void TextRendering_PrintString(GLFWwindow* window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_SetColor(float r, float g, float b); // cor das próximas strings (padrão: preto)
void TextRendering_PrintMatrix(GLFWwindow* window, glm::mat4 M, float x, float y, float scale = 1.0f);
void TextRendering_PrintVector(GLFWwindow* window, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProduct(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductMoreDigits(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductDivW(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowModelViewProjection(GLFWwindow* window, glm::mat4 projection, glm::mat4 view, glm::mat4 model, glm::vec4 p_model);
void TextRendering_ShowRunInfo(GLFWwindow* window);
void TextRendering_ShowGameOver(GLFWwindow* window);
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
// Tratamento das teclas, separado do KeyCallback para que o harness E2E (autotest.cpp)
// possa injetar teclas sem passar pelo Correcao_KeyCallback.
void HandleKey(GLFWwindow* window, int key, int action, int mods);

// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject
{
    std::string  name;        // Nome do objeto
    size_t       first_index; // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    size_t       num_indices; // Número de índices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum       rendering_mode; // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint       vertex_array_object_id; // ID do VAO onde estão armazenados os atributos do modelo
    glm::vec3    bbox_min; // Axis-Aligned Bounding Box do objeto
    glm::vec3    bbox_max;
};

// Abaixo definimos variáveis globais utilizadas em várias funções do código.

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário
// (map).  Veja dentro da função BuildTrianglesAndAddToVirtualScene() como que são incluídos
// objetos dentro da variável g_VirtualScene, e veja na função main() como
// estes são acessados.
std::map<std::string, SceneObject> g_VirtualScene;

// Pilha que guardará as matrizes de modelagem.
std::stack<glm::mat4>  g_MatrixStack;

// Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
float g_ScreenRatio = 1.0f;

// Estado do jogo (sem OpenGL; veja include/game.h). Global e não-static para
// que o harness E2E (autotest.cpp) possa inspecioná-lo. A semente é fixa
// (determinística) no autotest; no jogo normal, main() a troca por uma
// semente baseada no relógio.
Game g_Game(12345);

// Câmera em 3ª pessoa que segue o jogador (veja include/camera.h).
ThirdPersonCamera g_Camera;

// Malha dinâmica de faixas (GL_TRIANGLE_STRIP): VAO e VBOs criados uma única
// vez com capacidade máxima (GL_DYNAMIC_DRAW) e reenviados a cada quadro com
// glBufferSubData. Os vértices já estão em coordenadas de mundo (model =
// identidade). Usada pelo chão e pelos trilhos, gerados da pista a cada quadro.
struct DynamicStripMesh
{
    GLuint vertex_array_object_id;
    GLuint vbo_positions;
    GLuint vbo_normals;
    GLuint vbo_uvs;
    size_t capacity;              // número máximo de vértices nos VBOs
    std::vector<GLint>   first;   // vértice inicial de cada faixa
    std::vector<GLsizei> count;   // número de vértices de cada faixa
};
void CreateDynamicStripMesh(DynamicStripMesh* mesh, size_t capacity);
void UpdateDynamicStripMesh(DynamicStripMesh* mesh, const StripBatch& batch);
void DrawDynamicStripMesh(const DynamicStripMesh& mesh, int object_id, GLuint texture, int uv_mode, float uv_scale = 1.0f);

DynamicStripMesh g_TrackGround; // leito da pista + terreno dos lados
DynamicStripMesh g_TrackRails;  // 6 trilhos (2 por faixa)

// Rotação em torno de Y que vira o coelho para o "forward" local -Z, isto é,
// de costas para a câmera.
const float PLAYER_MODEL_YAW = -1.5707963f;

// Obstáculos provisórios (até os modelos finais da Tarefa 3.2): o cubo
// unitário escalado para estas dimensões (m), na ordem de ObstacleType. As
// semi-extensões entregues ao Game (SetObstacleExtents) saem da AABB do modelo
// escalado, e o desenho escala o modelo a partir das caixas que o Game devolve
// (ObstacleParts): o que se vê é exatamente o que colide.
const glm::vec3 OBSTACLE_PLACEHOLDER_SIZE[OBSTACLE_TYPE_COUNT] = {
    glm::vec3(2.0f, 0.9f, 0.3f),  // BARRIER_LOW
    glm::vec3(2.0f, 1.9f, 0.3f),  // BARRIER_HIGH (viga; o vão de 1.1 m fica embaixo)
    glm::vec3(2.0f, 3.0f, 0.3f),  // SIGN
    glm::vec3(2.2f, 3.2f, 12.0f), // TRAIN
    glm::vec3(2.0f, 1.5f, 8.0f),  // RAMP (ainda não gerada)
};
const int OBSTACLE_OBJECT_ID[OBSTACLE_TYPE_COUNT] = {BARRIER_LOW, BARRIER_HIGH, SIGN, TRAIN, RAMP};
void ConfigureObstacleExtents();

// Moedas: a esfera achatada como um disco de pé, girando em torno do eixo
// vertical. O disco desenhado é um pouco menor que a esfera de coleta
// (COIN_RADIUS), para a coleta ser "generosa".
const float COIN_DRAW_DIAMETER  = 0.8f;  // m
const float COIN_DRAW_THICKNESS = 0.12f; // m
const float COIN_SPIN_SPEED     = 3.0f;  // rad/s
float g_CoinSpin = 0.0f;                 // ângulo atual (acumula dt)

// Recomeça a corrida (Enter/Espaço em GAME_OVER) e reposiciona a câmera.
void RestartRun();

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = true;

// Variáveis que definem um programa de GPU (shaders). Veja função LoadShadersFromFiles().
GLuint g_GpuProgramID = 0;
GLint g_model_uniform;
GLint g_view_uniform;
GLint g_projection_uniform;
GLint g_object_id_uniform;
GLint g_uv_mode_uniform;
GLint g_uv_scale_uniform;
GLint g_bbox_min_uniform;
GLint g_bbox_max_uniform;
// Iluminação (Blinn-Phong) e material do objeto atual. Veja shader_fragment.glsl.
GLint g_sun_direction_uniform;
GLint g_sun_color_uniform;
GLint g_ambient_color_uniform;
GLint g_num_point_lights_uniform;
GLint g_point_light_position_uniform;
GLint g_point_light_color_uniform;
GLint g_shading_mode_uniform;
GLint g_material_kd_tint_uniform;
GLint g_material_ks_uniform;
GLint g_material_q_uniform;
GLint g_material_emissive_uniform;

// Cena: sol (luz direcional) e luzes pontuais dos postes.
// SUN_DIRECTION aponta da superfície PARA o sol: de cima, um pouco de lado e
// de TRÁS do jogador (+Z com heading 0), para que as faces dos obstáculos
// voltadas para a câmera (normal +Z) também recebam luz direta.
const glm::vec4 SUN_DIRECTION    = glm::vec4(0.30f, 1.00f, 0.70f, 0.0f);
const glm::vec3 SUN_COLOR        = glm::vec3(0.95f, 0.92f, 0.82f);
const glm::vec3 AMBIENT_COLOR    = glm::vec3(0.22f, 0.25f, 0.32f); // céu azulado
const glm::vec3 LAMP_LIGHT_COLOR = glm::vec3(2.8f, 2.0f, 1.0f);   // luz quente, intensidade > 1

// Material de um tipo de objeto (veja MaterialFor()). Kd = textura * kd_tint;
// ks/q = especular de Blinn-Phong; emissive = cor somada sem depender das luzes.
struct Material
{
    glm::vec3 kd_tint;
    glm::vec3 ks;
    float     q;
    glm::vec3 emissive;
    int       shading_mode; // 0 = Phong (por fragmento), 1 = Gouraud (por vértice)
};

// Material (Kd/Ks/q/emissivo/modo de sombreamento) de cada tipo de objeto.
Material MaterialFor(int object_id);
// Envia o material do tipo de objeto para os uniforms do shader.
void ApplyMaterial(int object_id);

int main(int argc, char* argv[])
{
    // Modo de teste ponta a ponta: "--autotest <cenario>" (ou "--autotest-list").
    // Deve vir antes de qualquer outra coisa, pois pode encerrar o processo.
    bool autotest = Autotest_Begin(argc, argv);

    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
    // sistema operacional, onde poderemos renderizar com OpenGL.
    int success = glfwInit();
    if (!success)
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos o callback para impressão de erros da GLFW no terminal
    glfwSetErrorCallback(ErrorCallback);

    // Pedimos para utilizar OpenGL versão 3.3 (ou superior)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    // Pedimos para utilizar o perfil "core", isto é, utilizaremos somente as
    // funções modernas de OpenGL.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criamos uma janela do sistema operacional, com 800 colunas e 600 linhas
    // de pixels, e com título "INF01047 ...".
    GLFWwindow* window;
    // Multisampling 4x: suaviza as bordas finas (trilhos) ao longe, que de
    // outra forma "piscam" (aliasing) com a câmera em movimento.
    glfwWindowHint(GLFW_SAMPLES, 4);

    window = glfwCreateWindow(800, 600, "INF01047 - Seu Cartao - Seu Nome", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos a função de callback que será chamada sempre que o usuário
    // pressionar alguma tecla do teclado.
    glfwSetKeyCallback(window, KeyCallback);

    // Indicamos que as chamadas OpenGL deverão renderizar nesta janela
    glfwMakeContextCurrent(window);

    // Carregamento de todas funções definidas por OpenGL 3.3, utilizando a
    // biblioteca GLAD.
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    // Definimos a função de callback que será chamada sempre que a janela for
    // redimensionada, por consequência alterando o tamanho do "framebuffer"
    // (região de memória onde são armazenados os pixels da imagem).
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 800, 600); // Forçamos a chamada do callback acima, para definir g_ScreenRatio.

    // Imprimimos no terminal informações sobre a GPU do sistema
    const GLubyte *vendor      = glGetString(GL_VENDOR);
    const GLubyte *renderer    = glGetString(GL_RENDERER);
    const GLubyte *glversion   = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados
    // para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
    //
    LoadShadersFromFiles();

    // Carregamos as imagens de textura. Cada objeto escolhe a sua em DrawObject(),
    // que a liga na unidade de textura 0 antes de desenhar.
    GLuint tex_brick = LoadTextureImage("../../data/red_brick_diff_1k.jpg");
    GLuint tex_rock  = LoadTextureImage("../../data/rocky_terrain_02_diff_1k.jpg");
    GLuint tex_metal = LoadTextureImage("../../data/metal_plate_diff_1k.jpg");

    // Construímos a representação de objetos geométricos através de malhas de triângulos.
    // Coelho: jogador provisório. Cubo: prédios provisórios (instâncias).
    ObjModel bunnymodel("../../data/bunny.obj");
    ComputeNormals(&bunnymodel);
    BuildTrianglesAndAddToVirtualScene(&bunnymodel);

    ObjModel cubemodel("../../data/cube.obj");
    ComputeNormals(&cubemodel);
    BuildTrianglesAndAddToVirtualScene(&cubemodel);

    // Esfera: moedas provisórias (achatadas como disco).
    ObjModel spheremodel("../../data/sphere.obj");
    ComputeNormals(&spheremodel);
    BuildTrianglesAndAddToVirtualScene(&spheremodel);

    if ( argc > 1 && !autotest && argv[1][0] != '-' )
    {
        ObjModel model(argv[1]);
        BuildTrianglesAndAddToVirtualScene(&model);
    }

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();

    // Malhas dinâmicas do chão e dos trilhos, com capacidade para o trecho
    // visível inteiro (veja MaxVerticesPerStrip() em scenery.cpp).
    CreateDynamicStripMesh(&g_TrackGround, GROUND_STRIP_COUNT * MaxVerticesPerStrip());
    CreateDynamicStripMesh(&g_TrackRails, RAIL_STRIP_COUNT * MaxVerticesPerStrip());

    // Lotes de vértices das faixas e lista de prédios: reaproveitados a cada
    // quadro (sem realocar memória).
    StripBatch ground_batch;
    StripBatch rails_batch;
    std::vector<BuildingInstance> buildings;

    // Postes de luz: lista do trecho visível, posição de mundo da luz de cada
    // um e os índices dos mais próximos do jogador (reaproveitados por quadro).
    std::vector<LampInstance> lamps;
    std::vector<glm::vec3> lamp_positions;
    std::vector<int> picked_lamps;

    // Corrida: no autotest a semente fixa (12345) de g_Game é mantida para que
    // o cenário seja determinístico; no jogo normal, semente pelo relógio.
    if (!autotest)
        g_Game = Game((unsigned)time(NULL));

    // Dimensões dos obstáculos a partir dos modelos carregados (depois de
    // criar g_Game, que nasce com as dimensões padrão).
    ConfigureObstacleExtents();

    // Configuração do cenário E2E (ex.: desligar o spawn), antes do 1º quadro.
    if (autotest)
        Autotest_Start();

    g_Camera.Snap(g_Game.track.FrameAt(g_Game.player.s), g_Game.player.x,
                  g_Game.track.IsTunnelAt(g_Game.player.s));

    // Habilitamos o Z-buffer. Veja slides 104-116 do documento Aula_09_Projecoes.pdf.
    glEnable(GL_DEPTH_TEST);

    // Multisampling (pedido com GLFW_SAMPLES acima).
    glEnable(GL_MULTISAMPLE);

    // Habilitamos o Backface Culling. Veja slides 8-13 do documento Aula_02_Fundamentos_Matematicos.pdf, slides 23-34 do documento Aula_13_Clipping_and_Culling.pdf e slides 112-123 do documento Aula_14_Laboratorio_3_Revisao.pdf.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Instante do quadro anterior, usado para calcular o Δt de cada quadro.
    double last_frame_time = glfwGetTime();

    // No autotest o dt é fixo (determinístico) e o tempo simulado é acumulado.
    float autotest_time = 0.0f;
    if (autotest)
        glfwSwapInterval(0); // sem vsync: o cenário não precisa esperar o monitor

    // Ficamos em um loop infinito, renderizando, até que o usuário feche a janela
    while (!glfwWindowShouldClose(window))
    {
        // Δt do quadro, limitado a MAX_DT para que um quadro muito longo (janela
        // arrastada, breakpoint, carregamento) não "teleporte" a simulação.
        float dt = autotest ? Autotest_FixedDt()
                            : ComputeFrameDt(glfwGetTime(), &last_frame_time);

        // Atualizamos a simulação do jogo e a câmera que segue o jogador.
        g_Game.Update(dt);
        g_CoinSpin = std::fmod(g_CoinSpin + COIN_SPIN_SPEED * dt, 6.2831853f);

        const Player& player = g_Game.player;
        TrackFrame player_frame = g_Game.track.FrameAt(player.s);
        g_Camera.Update(player_frame, player.x, g_Game.track.IsTunnelAt(player.s), dt);

        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como azul-claro (céu).
        // Tal cor é definida como coeficientes RGBA: Red, Green, Blue, Alpha;
        // isto é: Vermelho, Verde, Azul, Alpha (valor de transparência).
        // Conversaremos sobre sistemas de cores nas aulas de Modelos de Iluminação.
        //
        //           R     G     B     A
        glClearColor(0.53f, 0.75f, 0.95f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
        // e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
        // os shaders de vértice e fragmentos).
        glUseProgram(g_GpuProgramID);

        // Câmera look-at em 3ª pessoa: posição, view vector e up vêm de
        // ThirdPersonCamera (camera.cpp). Veja slides 195-227 e 229-234 do
        // documento Aula_08_Sistemas_de_Coordenadas.pdf.
        CameraPose pose = g_Camera.Pose();

        // Computamos a matriz "View" utilizando os parâmetros da câmera para
        // definir o sistema de coordenadas da câmera.  Veja slides 2-14, 184-190 e 236-242 do documento Aula_08_Sistemas_de_Coordenadas.pdf.
        glm::mat4 view = Matrix_Camera_View(pose.position, pose.view_vector, pose.up);

        // Agora computamos a matriz de Projeção (perspectiva).
        // Note que, no sistema de coordenadas da câmera, os planos near e far
        // estão no sentido negativo! Veja slides 176-204 do documento Aula_09_Projecoes.pdf.
        // near = -0.3 (e não -0.1) dá mais precisão ao Z-buffer com o far
        // distante, evitando Z-fighting entre trilhos (y = 0.03) e chão.
        float nearplane = -0.3f;   // Posição do "near plane"
        float farplane  = -250.0f; // Posição do "far plane" (além do trecho gerado à frente)

        // Para definição do field of view (FOV), veja slides 205-215 do documento Aula_09_Projecoes.pdf.
        float field_of_view = 3.141592f / 3.0f;
        glm::mat4 projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);

        // Enviamos as matrizes "view" e "projection" para a placa de vídeo
        // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas são
        // efetivamente aplicadas em todos os pontos.
        glUniformMatrix4fv(g_view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));

        // Luzes: sol fixo + as MAX_POINT_LIGHTS lâmpadas de poste mais próximas
        // do jogador (as demais ficam longe demais para fazer diferença).
        ComputeLamps(player.s, &lamps);
        lamp_positions.clear();
        for (size_t i = 0; i < lamps.size(); ++i)
        {
            TrackFrame f = g_Game.track.FrameAt(lamps[i].s);
            glm::vec4 p = f.position + f.right * lamps[i].x;
            lamp_positions.push_back(glm::vec3(p.x, LAMP_HEIGHT, p.z));
        }
        glm::vec4 player_pos = player_frame.position + player_frame.right * player.x;
        PickNearestPoints(lamp_positions, glm::vec3(player_pos.x, player_pos.y, player_pos.z),
                          MAX_POINT_LIGHTS, &picked_lamps);
        float light_pos[4 * MAX_POINT_LIGHTS] = {0};
        float light_col[3 * MAX_POINT_LIGHTS] = {0};
        for (size_t i = 0; i < picked_lamps.size(); ++i)
        {
            const glm::vec3& lp = lamp_positions[picked_lamps[i]];
            light_pos[4 * i + 0] = lp.x;
            light_pos[4 * i + 1] = lp.y;
            light_pos[4 * i + 2] = lp.z;
            light_pos[4 * i + 3] = 1.0f;
            light_col[3 * i + 0] = LAMP_LIGHT_COLOR.x;
            light_col[3 * i + 1] = LAMP_LIGHT_COLOR.y;
            light_col[3 * i + 2] = LAMP_LIGHT_COLOR.z;
        }
        glUniform4f(g_sun_direction_uniform, SUN_DIRECTION.x, SUN_DIRECTION.y, SUN_DIRECTION.z, 0.0f);
        glUniform3f(g_sun_color_uniform, SUN_COLOR.x, SUN_COLOR.y, SUN_COLOR.z);
        glUniform3f(g_ambient_color_uniform, AMBIENT_COLOR.x, AMBIENT_COLOR.y, AMBIENT_COLOR.z);
        glUniform1i(g_num_point_lights_uniform, (GLint)picked_lamps.size());
        glUniform4fv(g_point_light_position_uniform, MAX_POINT_LIGHTS, light_pos);
        glUniform3fv(g_point_light_color_uniform, MAX_POINT_LIGHTS, light_col);

        // Chão e trilhos: faixas reconstruídas a partir da pista no trecho
        // visível em torno do jogador. As UV das faixas já estão em
        // "ladrilhos" de tamanho fixo em metros (sem esticar).
        BuildGroundStrips(g_Game.track, player.s, &ground_batch);
        UpdateDynamicStripMesh(&g_TrackGround, ground_batch);
        DrawDynamicStripMesh(g_TrackGround, GROUND, tex_rock, UV_OBJ);

        BuildRailStrips(g_Game.track, player.s, &rails_batch);
        UpdateDynamicStripMesh(&g_TrackRails, rails_batch);
        DrawDynamicStripMesh(g_TrackRails, RAIL, tex_metal, UV_OBJ);

        // Jogador (coelho provisório): matriz de pista
        //   T(posição na pista + right*x + up*y) * R_y(heading) * <modelo>,
        // com altura total PLAYER_HEIGHT e os pés em y = 0. Rolando, é
        // achatado na vertical para PLAYER_ROLL_HEIGHT.
        {
            glm::vec4 p = player_frame.position + player_frame.right * player.x
                        + glm::vec4(0.0f, player.y, 0.0f, 0.0f);
            float k = PLAYER_HEIGHT / ObjectSize("the_bunny").y;
            float squash = (player.roll_timer > 0.0f) ? PLAYER_ROLL_HEIGHT / PLAYER_HEIGHT : 1.0f;
            glm::mat4 model = Matrix_Translate(p.x, p.y, p.z)
                            * Matrix_Rotate_Y(player_frame.heading + PLAYER_MODEL_YAW)
                            * Matrix_OnGround("the_bunny", k, k * squash, k);
            DrawObject("the_bunny", model, PLAYER, tex_metal, UV_SPHERICAL);
        }

        // Prédios provisórios: o MESMO VAO do cubo desenhado várias vezes
        // (instâncias), cada um com sua matriz de modelagem. A textura usa
        // UV_BOX (em metros), para os tijolos não esticarem com a escala não
        // uniforme: uma repetição a cada 3 m.
        ComputeBuildings(player.s, &buildings);
        glm::vec3 cube_size = ObjectSize("the_cube");
        for (size_t i = 0; i < buildings.size(); ++i)
        {
            const BuildingInstance& b = buildings[i];
            TrackFrame f = g_Game.track.FrameAt(b.s);
            glm::vec4 p = f.position + f.right * b.x;
            glm::mat4 model = Matrix_Translate(p.x, p.y, p.z)
                            * Matrix_Rotate_Y(f.heading)
                            * Matrix_OnGround("the_cube",
                                              b.width  / cube_size.x,
                                              b.height / cube_size.y,
                                              b.depth  / cube_size.z);
            DrawObject("the_cube", model, BUILDING, tex_brick, UV_BOX, 1.0f / 3.0f);
        }

        // Postes de luz: mais duas instâncias do cubo por poste, a haste fina e
        // a cabeça emissiva (a luz pontual fica no centro da cabeça).
        for (size_t i = 0; i < lamps.size(); ++i)
        {
            TrackFrame f = g_Game.track.FrameAt(lamps[i].s);
            glm::vec4 p = f.position + f.right * lamps[i].x;
            const float head_bottom = LAMP_HEIGHT - 0.5f * LAMP_HEAD_HEIGHT;
            glm::mat4 pole = Matrix_Translate(p.x, p.y, p.z)
                           * Matrix_Rotate_Y(f.heading)
                           * Matrix_OnGround("the_cube",
                                             LAMP_POLE_WIDTH / cube_size.x,
                                             head_bottom     / cube_size.y,
                                             LAMP_POLE_WIDTH / cube_size.z);
            DrawObject("the_cube", pole, LAMP_POLE, tex_metal, UV_BOX, 0.5f);
            glm::mat4 head = Matrix_Translate(p.x, p.y + head_bottom, p.z)
                           * Matrix_Rotate_Y(f.heading)
                           * Matrix_OnGround("the_cube",
                                             LAMP_HEAD_SIZE   / cube_size.x,
                                             LAMP_HEAD_HEIGHT / cube_size.y,
                                             LAMP_HEAD_SIZE   / cube_size.z);
            DrawObject("the_cube", head, LAMP, tex_metal, UV_BOX, 0.5f);
        }

        // Obstáculos: instâncias do cubo (um VAO para todos), uma matriz de
        // modelagem por parte sólida. Cada parte é uma caixa em coordenadas de
        // pista (x lateral, y altura, z = s) vinda do Game; o cubo é escalado
        // para o tamanho dela, apoiado em y = part.min.y e orientado pelo
        // referencial da pista no centro da caixa. UV_BOX: textura em metros,
        // sem esticar (uma repetição a cada 2 m).
        AABB parts[3];
        for (size_t i = 0; i < g_Game.obstacles.size(); ++i)
        {
            const Obstacle& o = g_Game.obstacles[i];
            int n = ObstacleParts(o, parts);
            for (int k = 0; k < n; ++k)
            {
                glm::vec3 size = parts[k].max - parts[k].min;
                float cx = 0.5f * (parts[k].min.x + parts[k].max.x);
                float cs = 0.5f * (parts[k].min.z + parts[k].max.z);
                TrackFrame f = g_Game.track.FrameAt(cs);
                glm::vec4 p = f.position + f.right * cx + glm::vec4(0.0f, parts[k].min.y, 0.0f, 0.0f);
                glm::mat4 model = Matrix_Translate(p.x, p.y, p.z)
                                * Matrix_Rotate_Y(f.heading)
                                * Matrix_OnGround("the_cube",
                                                  size.x / cube_size.x,
                                                  size.y / cube_size.y,
                                                  size.z / cube_size.z);
                DrawObject("the_cube", model, OBSTACLE_OBJECT_ID[(int)o.type], tex_metal, UV_BOX, 0.5f);
            }
        }

        // Moedas: instâncias da esfera achatada (disco de pé), girando em
        // torno do eixo vertical com o ângulo acumulado por dt.
        {
            glm::vec3 sphere_size = ObjectSize("the_sphere");
            for (size_t i = 0; i < g_Game.coins.size(); ++i)
            {
                const Coin& c = g_Game.coins[i];
                if (c.collected)
                    continue;
                TrackFrame f = g_Game.track.FrameAt(c.s);
                glm::vec4 p = f.position + f.right * (c.lane * LANE_WIDTH)
                            + glm::vec4(0.0f, c.y - 0.5f * COIN_DRAW_DIAMETER, 0.0f, 0.0f);
                glm::mat4 model = Matrix_Translate(p.x, p.y, p.z)
                                * Matrix_Rotate_Y(f.heading + g_CoinSpin)
                                * Matrix_OnGround("the_sphere",
                                                  COIN_DRAW_DIAMETER  / sphere_size.x,
                                                  COIN_DRAW_DIAMETER  / sphere_size.y,
                                                  COIN_DRAW_THICKNESS / sphere_size.z);
                DrawObject("the_sphere", model, COIN, tex_metal, UV_SPHERICAL);
            }
        }

        // Placar (pontos, moedas, distância) e, no fim de jogo, a mensagem
        // de GAME OVER centralizada.
        TextRendering_ShowRunInfo(window);
        TextRendering_ShowGameOver(window);

        // Imprimimos na tela informação sobre o número de quadros renderizados
        // por segundo (frames per second).
        TextRendering_ShowFramesPerSecond(window);

        // Harness E2E: injeta teclas, tira screenshots e checa erros de GL
        // (precisa ser depois de desenhar e antes do swap).
        if (autotest)
        {
            autotest_time += dt;
            Autotest_AfterFrame(window, autotest_time);
        }

        // O framebuffer onde OpenGL executa as operações de renderização não
        // é o mesmo que está sendo mostrado para o usuário, caso contrário
        // seria possível ver artefatos conhecidos como "screen tearing". A
        // chamada abaixo faz a troca dos buffers, mostrando para o usuário
        // tudo que foi renderizado pelas funções acima.
        // Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
        glfwSwapBuffers(window);

        // Verificamos com o sistema operacional se houve alguma interação do
        // usuário (teclado, mouse, ...). Caso positivo, as funções de callback
        // definidas anteriormente usando glfwSet*Callback() serão chamadas
        // pela biblioteca GLFW.
        glfwPollEvents();
    }

    // Finalizamos o uso dos recursos do sistema operacional
    glfwTerminate();

    // Fim do programa
    return autotest ? Autotest_ExitCode() : 0;
}

// Retorna o tempo (s) decorrido desde o quadro anterior, limitado a MAX_DT, e
// guarda o instante atual em *last para o próximo quadro.
float ComputeFrameDt(double now, double* last)
{
    float dt = (float)(now - *last);
    *last = now;
    return std::min(dt, MAX_DT);
}

// Função que carrega uma imagem para ser utilizada como textura
GLuint LoadTextureImage(const char* filename)
{
    printf("Carregando imagem \"%s\"... ", filename);

    // Primeiro fazemos a leitura da imagem do disco
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);

    if ( data == NULL )
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }

    printf("OK (%dx%d).\n", width, height);

    // Agora criamos objetos na GPU com OpenGL para armazenar a textura
    GLuint texture_id;
    glGenTextures(1, &texture_id);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    // Todas as texturas são usadas pela unidade 0 (veja DrawObject()), então
    // os parâmetros de amostragem ficam guardados na própria textura, e não em
    // um sampler object preso a uma unidade. Veja slides 95-96 do documento
    // Aula_20_Mapeamento_de_Texturas.pdf
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // GL_REPEAT em S e T: permite repetir a textura (uv_scale > 1) em chão e paredes.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    return texture_id;
}

// Função que desenha um objeto da cena: envia a matriz de modelagem, o id do
// objeto, o modo de UV e a escala de UV para a GPU, liga a textura do objeto na
// unidade 0 e desenha a malha. Os valores dos ids e dos modos de UV estão em
// "object_ids.h" (espelhados em shader_fragment.glsl).
void DrawObject(const char* name, const glm::mat4& model, int object_id, GLuint texture, int uv_mode, float uv_scale)
{
    glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
    glUniform1i(g_object_id_uniform, object_id);
    glUniform1i(g_uv_mode_uniform, uv_mode);
    glUniform1f(g_uv_scale_uniform, uv_scale);
    ApplyMaterial(object_id);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    DrawVirtualObject(name);
}

// Material de cada tipo de objeto: tom sobre a textura (kd_tint), especular
// (ks, q), emissivo e modo de sombreamento. Concentra aqui o "visual" de cada
// tipo, em vez de ramificar por object_id no shader. Metais (barreiras, placas,
// trem, trilhos, moedas) têm Ks alto; chão e tijolo quase não brilham.
Material MaterialFor(int object_id)
{
    const glm::vec3 white(1.0f, 1.0f, 1.0f);
    const glm::vec3 none(0.0f, 0.0f, 0.0f);
    Material m = { white, glm::vec3(0.3f), 16.0f, none, 0 }; // padrão
    switch (object_id)
    {
    case PLAYER:      m = { white, glm::vec3(0.35f), 32.0f, none, 0 }; break;
    case GROUND:      m = { white, glm::vec3(0.04f),  8.0f, none, 0 }; break;
    case RAIL:        m = { white, glm::vec3(0.90f), 96.0f, none, 0 }; break;
    case BUILDING:    m = { white, glm::vec3(0.05f),  8.0f, none, 0 }; break;
    case BARRIER_LOW:
    case BARRIER_HIGH:m = { glm::vec3(1.0f, 0.55f, 0.15f), glm::vec3(0.70f), 128.0f, none, 0 }; break; // laranja de obra
    case SIGN:        m = { glm::vec3(0.35f, 0.55f, 1.0f), glm::vec3(0.70f), 128.0f, none, 0 }; break;  // placa azul
    case TRAIN:       m = { glm::vec3(0.75f, 0.25f, 0.20f), glm::vec3(0.80f), 128.0f, none, 0 }; break; // vagão vermelho
    case RAMP:        m = { white, glm::vec3(0.35f), 24.0f, none, 0 }; break;
    case COIN:        m = { glm::vec3(2.2f, 1.7f, 0.25f), glm::vec3(0.90f), 32.0f, none, 1 }; break;    // dourado; Gouraud
    case LAMP_POLE:   m = { glm::vec3(0.45f), glm::vec3(0.60f), 32.0f, none, 0 }; break;
    case LAMP:        m = { glm::vec3(0.10f), none, 1.0f, glm::vec3(1.5f, 1.15f, 0.65f), 0 }; break;    // lâmpada acesa
    default: break;
    }
    return m;
}

void ApplyMaterial(int object_id)
{
    Material m = MaterialFor(object_id);
    glUniform3f(g_material_kd_tint_uniform, m.kd_tint.x, m.kd_tint.y, m.kd_tint.z);
    glUniform3f(g_material_ks_uniform, m.ks.x, m.ks.y, m.ks.z);
    glUniform1f(g_material_q_uniform, m.q);
    glUniform3f(g_material_emissive_uniform, m.emissive.x, m.emissive.y, m.emissive.z);
    glUniform1i(g_shading_mode_uniform, m.shading_mode);
}

// Objeto de g_VirtualScene pelo nome. Ao contrário de g_VirtualScene[name],
// não insere silenciosamente um objeto vazio quando o nome não existe (o que
// desenharia nada, com AABB zerada): aborta com mensagem clara.
const SceneObject& FindSceneObject(const char* name)
{
    std::map<std::string, SceneObject>::const_iterator it = g_VirtualScene.find(name);
    if (it == g_VirtualScene.end())
    {
        fprintf(stderr, "ERROR: objeto \"%s\" nao existe em g_VirtualScene (nome do shape no .obj?).\n", name);
        std::exit(EXIT_FAILURE);
    }
    return it->second;
}

// Passa ao Game as semi-extensões de cada obstáculo, tiradas da AABB do modelo
// que o representa, escalado para OBSTACLE_PLACEHOLDER_SIZE (por enquanto,
// sempre o cubo; com os modelos finais muda só o modelo/escala aqui).
void ConfigureObstacleExtents()
{
    glm::vec3 cube_size = ObjectSize("the_cube");
    for (int t = 0; t < OBSTACLE_TYPE_COUNT; ++t)
    {
        glm::vec3 scale = OBSTACLE_PLACEHOLDER_SIZE[t] / cube_size;
        glm::vec3 half  = 0.5f * cube_size * scale;
        g_Game.SetObstacleExtents((ObstacleType)t, half);
    }
}

// Recomeça a corrida do zero e reposiciona a câmera atrás do jogador (sem a
// transição suave, que faria a câmera "voar" de volta pela pista).
void RestartRun()
{
    g_Game.Reset();
    g_Camera.Snap(g_Game.track.FrameAt(g_Game.player.s), g_Game.player.x,
                  g_Game.track.IsTunnelAt(g_Game.player.s));
}

// Dimensões da AABB de um objeto de g_VirtualScene (coordenadas de modelo).
glm::vec3 ObjectSize(const char* name)
{
    const SceneObject& obj = FindSceneObject(name);
    return obj.bbox_max - obj.bbox_min;
}

// Matriz que escala o objeto por (sx,sy,sz) depois de transladá-lo para que o
// centro da base da sua AABB fique na origem (objeto "em pé" sobre y = 0).
glm::mat4 Matrix_OnGround(const char* name, float sx, float sy, float sz)
{
    const SceneObject& obj = FindSceneObject(name);
    float cx = 0.5f * (obj.bbox_min.x + obj.bbox_max.x);
    float cz = 0.5f * (obj.bbox_min.z + obj.bbox_max.z);
    return Matrix_Scale(sx, sy, sz) * Matrix_Translate(-cx, -obj.bbox_min.y, -cz);
}

// Cria o VAO e os VBOs (posição vec4, normal vec4, UV vec2) de uma malha
// dinâmica de faixas, com espaço para 'capacity' vértices. Os dados são
// enviados depois, a cada quadro, por UpdateDynamicStripMesh().
void CreateDynamicStripMesh(DynamicStripMesh* mesh, size_t capacity)
{
    mesh->capacity = capacity;
    mesh->first.clear();
    mesh->count.clear();

    glGenVertexArrays(1, &mesh->vertex_array_object_id);
    glBindVertexArray(mesh->vertex_array_object_id);

    // Mesmos "locations" de BuildTrianglesAndAddToVirtualScene() e shader_vertex.glsl.
    const GLuint locations[3]  = {0, 1, 2};
    const GLint  dimensions[3] = {4, 4, 2};
    GLuint* vbos[3] = {&mesh->vbo_positions, &mesh->vbo_normals, &mesh->vbo_uvs};
    for (int i = 0; i < 3; ++i)
    {
        glGenBuffers(1, vbos[i]);
        glBindBuffer(GL_ARRAY_BUFFER, *vbos[i]);
        glBufferData(GL_ARRAY_BUFFER, capacity * dimensions[i] * sizeof(float), NULL, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(locations[i], dimensions[i], GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(locations[i]);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Reenvia os vértices de 'batch' para os VBOs já alocados (glBufferSubData;
// nenhum buffer é criado por quadro). Faixas que não caberiam na capacidade
// são descartadas (com aviso), em vez de escrever fora do buffer.
void UpdateDynamicStripMesh(DynamicStripMesh* mesh, const StripBatch& batch)
{
    mesh->first.clear();
    mesh->count.clear();

    size_t used = 0;
    for (size_t i = 0; i < batch.first.size(); ++i)
    {
        size_t end = (size_t)batch.first[i] + (size_t)batch.count[i];
        if (end > mesh->capacity)
        {
            static bool warned = false;
            if (!warned)
            {
                fprintf(stderr, "AVISO: faixa com fim no vertice %d nao cabe na malha dinamica (capacidade %d); descartada.\n",
                        (int)end, (int)mesh->capacity);
                warned = true;
            }
            continue;
        }
        mesh->first.push_back(batch.first[i]);
        mesh->count.push_back(batch.count[i]);
        used = std::max(used, end);
    }

    if (used == 0)
        return;

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_positions);
    glBufferSubData(GL_ARRAY_BUFFER, 0, used * 4 * sizeof(float), batch.positions.data());
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_normals);
    glBufferSubData(GL_ARRAY_BUFFER, 0, used * 4 * sizeof(float), batch.normals.data());
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo_uvs);
    glBufferSubData(GL_ARRAY_BUFFER, 0, used * 2 * sizeof(float), batch.uvs.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Desenha as faixas de uma malha dinâmica (GL_TRIANGLE_STRIP). Os vértices já
// estão em coordenadas de mundo, então model = identidade; os demais uniforms
// são os mesmos de DrawObject().
void DrawDynamicStripMesh(const DynamicStripMesh& mesh, int object_id, GLuint texture, int uv_mode, float uv_scale)
{
    glm::mat4 model = Matrix_Identity();
    glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
    glUniform1i(g_object_id_uniform, object_id);
    glUniform1i(g_uv_mode_uniform, uv_mode);
    glUniform1f(g_uv_scale_uniform, uv_scale);
    glUniform4f(g_bbox_min_uniform, 0.0f, 0.0f, 0.0f, 1.0f);
    glUniform4f(g_bbox_max_uniform, 0.0f, 0.0f, 0.0f, 1.0f);
    ApplyMaterial(object_id);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindVertexArray(mesh.vertex_array_object_id);
    for (size_t i = 0; i < mesh.first.size(); ++i)
        glDrawArrays(GL_TRIANGLE_STRIP, mesh.first[i], mesh.count[i]);
    glBindVertexArray(0);
}

// Função que desenha um objeto armazenado em g_VirtualScene. Veja definição
// dos objetos na função BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char* object_name)
{
    // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
    // vértices apontados pelo VAO criado pela função BuildTrianglesAndAddToVirtualScene(). Veja
    // comentários detalhados dentro da definição de BuildTrianglesAndAddToVirtualScene().
    const SceneObject& obj = FindSceneObject(object_name);
    glBindVertexArray(obj.vertex_array_object_id);

    // Setamos as variáveis "bbox_min" e "bbox_max" do fragment shader
    // com os parâmetros da axis-aligned bounding box (AABB) do modelo.
    glm::vec3 bbox_min = obj.bbox_min;
    glm::vec3 bbox_max = obj.bbox_max;
    glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

    // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
    // apontados pelo VAO como linhas. Veja a definição de
    // g_VirtualScene[""] dentro da função BuildTrianglesAndAddToVirtualScene(), e veja
    // a documentação da função glDrawElements() em
    // http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        obj.rendering_mode,
        obj.num_indices,
        GL_UNSIGNED_INT,
        (void*)(obj.first_index * sizeof(GLuint))
    );

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Função que carrega os shaders de vértices e de fragmentos que serão
// utilizados para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
//
void LoadShadersFromFiles()
{
    // Note que o caminho para os arquivos "shader_vertex.glsl" e
    // "shader_fragment.glsl" estão fixados, sendo que assumimos a existência
    // da seguinte estrutura no sistema de arquivos:
    //
    //    + FCG_Lab_01/
    //    |
    //    +--+ bin/
    //    |  |
    //    |  +--+ Release/  (ou Debug/ ou Linux/)
    //    |     |
    //    |     o-- main.exe
    //    |
    //    +--+ src/
    //       |
    //       o-- shader_vertex.glsl
    //       |
    //       o-- shader_fragment.glsl
    //
    GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");

    // Deletamos o programa de GPU anterior, caso ele exista.
    if ( g_GpuProgramID != 0 )
        glDeleteProgram(g_GpuProgramID);

    // Criamos um programa de GPU utilizando os shaders carregados acima.
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
    g_model_uniform      = glGetUniformLocation(g_GpuProgramID, "model"); // Variável da matriz "model"
    g_view_uniform       = glGetUniformLocation(g_GpuProgramID, "view"); // Variável da matriz "view" em shader_vertex.glsl
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    g_object_id_uniform  = glGetUniformLocation(g_GpuProgramID, "object_id"); // Variável "object_id" em shader_fragment.glsl
    g_uv_mode_uniform    = glGetUniformLocation(g_GpuProgramID, "uv_mode"); // Variável "uv_mode" em shader_fragment.glsl
    g_uv_scale_uniform   = glGetUniformLocation(g_GpuProgramID, "uv_scale"); // Variável "uv_scale" em shader_fragment.glsl
    g_bbox_min_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_min");
    g_bbox_max_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_max");
    g_sun_direction_uniform        = glGetUniformLocation(g_GpuProgramID, "sun_direction");
    g_sun_color_uniform            = glGetUniformLocation(g_GpuProgramID, "sun_color");
    g_ambient_color_uniform        = glGetUniformLocation(g_GpuProgramID, "ambient_color");
    g_num_point_lights_uniform     = glGetUniformLocation(g_GpuProgramID, "num_point_lights");
    g_point_light_position_uniform = glGetUniformLocation(g_GpuProgramID, "point_light_position");
    g_point_light_color_uniform    = glGetUniformLocation(g_GpuProgramID, "point_light_color");
    g_shading_mode_uniform         = glGetUniformLocation(g_GpuProgramID, "shading_mode");
    g_material_kd_tint_uniform     = glGetUniformLocation(g_GpuProgramID, "material_kd_tint");
    g_material_ks_uniform          = glGetUniformLocation(g_GpuProgramID, "material_ks");
    g_material_q_uniform           = glGetUniformLocation(g_GpuProgramID, "material_q");
    g_material_emissive_uniform    = glGetUniformLocation(g_GpuProgramID, "material_emissive");

    // Variável em "shader_fragment.glsl" para acesso da textura do objeto (unidade 0)
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUseProgram(0);
}

// Função que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M)
{
    g_MatrixStack.push(M);
}

// Função que remove a matriz atualmente no topo da pilha e armazena a mesma na variável M
void PopMatrix(glm::mat4& M)
{
    if ( g_MatrixStack.empty() )
    {
        M = Matrix_Identity();
    }
    else
    {
        M = g_MatrixStack.top();
        g_MatrixStack.pop();
    }
}

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel* model)
{
    if ( !model->attrib.normals.empty() )
        return;

    // Primeiro computamos as normais para todos os TRIÂNGULOS.
    // Segundo, computamos as normais dos VÉRTICES através do método proposto
    // por Gouraud, onde a normal de cada vértice vai ser a média das normais de
    // todas as faces que compartilham este vértice e que pertencem ao mesmo "smoothing group".

    // Obtemos a lista dos smoothing groups que existem no objeto
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        assert(model->shapes[shape].mesh.smoothing_group_ids.size() == num_triangles);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);
            unsigned int sgroup = model->shapes[shape].mesh.smoothing_group_ids[triangle];
            assert(sgroup >= 0);
            sgroup_ids.insert(sgroup);
        }
    }

    size_t num_vertices = model->attrib.vertices.size() / 3;
    model->attrib.normals.reserve( 3*num_vertices );

    // Processamos um smoothing group por vez
    for (const unsigned int & sgroup : sgroup_ids)
    {
        std::vector<int> num_triangles_per_vertex(num_vertices, 0);
        std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));

        // Acumulamos as normais dos vértices de todos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                glm::vec4  vertices[3];
                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                    const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                    const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                    vertices[vertex] = glm::vec4(vx,vy,vz,1.0);
                }

                const glm::vec4  a = vertices[0];
                const glm::vec4  b = vertices[1];
                const glm::vec4  c = vertices[2];

                const glm::vec4  n = crossproduct(b-a,c-a);

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    num_triangles_per_vertex[idx.vertex_index] += 1;
                    vertex_normals[idx.vertex_index] += n;
                }
            }
        }

        // Computamos a média das normais acumuladas
        std::vector<size_t> normal_indices(num_vertices, 0);

        for (size_t vertex_index = 0; vertex_index < vertex_normals.size(); ++vertex_index)
        {
            if (num_triangles_per_vertex[vertex_index] == 0)
                continue;

            glm::vec4 n = vertex_normals[vertex_index] / (float)num_triangles_per_vertex[vertex_index];
            n /= norm(n);

            model->attrib.normals.push_back( n.x );
            model->attrib.normals.push_back( n.y );
            model->attrib.normals.push_back( n.z );

            size_t normal_index = (model->attrib.normals.size() / 3) - 1;
            normal_indices[vertex_index] = normal_index;
        }

        // Escrevemos os índices das normais para os vértices dos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index =
                        normal_indices[ idx.vertex_index ];
                }
            }
        }

    }
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel* model)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float>  model_coefficients;
    std::vector<float>  normal_coefficients;
    std::vector<float>  texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        // Em 2026-05-18, corrigido bug encontrado pelo aluno Arthur Prediger:
        // std::numeric_limits<float>::min() retorna o menor valor positivo
        // normalizado representável, não o menor valor possível (negativo). Para
        // inicializar o limite máximo da bounding box com um valor "muito
        // pequeno", deve ser usado std::numeric_limits<float>::lowest()
        const float minval = std::numeric_limits<float>::lowest();
        const float maxval = std::numeric_limits<float>::max();

        glm::vec3 bbox_min = glm::vec3(maxval,maxval,maxval);
        glm::vec3 bbox_max = glm::vec3(minval,minval,minval);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];

                indices.push_back(first_index + 3*triangle + vertex);

                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                //printf("tri %d vert %d = (%.2f, %.2f, %.2f)\n", (int)triangle, (int)vertex, vx, vy, vz);
                model_coefficients.push_back( vx ); // X
                model_coefficients.push_back( vy ); // Y
                model_coefficients.push_back( vz ); // Z
                model_coefficients.push_back( 1.0f ); // W

                bbox_min.x = std::min(bbox_min.x, vx);
                bbox_min.y = std::min(bbox_min.y, vy);
                bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx);
                bbox_max.y = std::max(bbox_max.y, vy);
                bbox_max.z = std::max(bbox_max.z, vz);

                // Inspecionando o código da tinyobjloader, o aluno Bernardo
                // Sulzbach (2017/1) apontou que a maneira correta de testar se
                // existem normais e coordenadas de textura no ObjModel é
                // comparando se o índice retornado é -1. Fazemos isso abaixo.

                if ( idx.normal_index != -1 )
                {
                    const float nx = model->attrib.normals[3*idx.normal_index + 0];
                    const float ny = model->attrib.normals[3*idx.normal_index + 1];
                    const float nz = model->attrib.normals[3*idx.normal_index + 2];
                    normal_coefficients.push_back( nx ); // X
                    normal_coefficients.push_back( ny ); // Y
                    normal_coefficients.push_back( nz ); // Z
                    normal_coefficients.push_back( 0.0f ); // W
                }

                if ( idx.texcoord_index != -1 )
                {
                    const float u = model->attrib.texcoords[2*idx.texcoord_index + 0];
                    const float v = model->attrib.texcoords[2*idx.texcoord_index + 1];
                    texture_coefficients.push_back( u );
                    texture_coefficients.push_back( v );
                }
            }
        }

        size_t last_index = indices.size() - 1;

        SceneObject theobject;
        theobject.name           = model->shapes[shape].name;
        theobject.first_index    = first_index; // Primeiro índice
        theobject.num_indices    = last_index - first_index + 1; // Número de indices
        theobject.rendering_mode = GL_TRIANGLES;       // Índices correspondem ao tipo de rasterização GL_TRIANGLES.
        theobject.vertex_array_object_id = vertex_array_object_id;

        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;

        g_VirtualScene[model->shapes[shape].name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0; // "(location = 0)" em "shader_vertex.glsl"
    GLint  number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if ( !normal_coefficients.empty() )
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 1; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if ( !texture_coefficients.empty() )
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);

    // "Ligamos" o buffer. Note que o tipo agora é GL_ELEMENT_ARRAY_BUFFER.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!
    //

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos vértices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definição de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU de
// um arquivo GLSL e faz sua compilação.
void LoadShader(const char* filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela variável "filename"
    // e colocamos seu conteúdo em memória, apontado pela variável
    // "shader_string".
    std::ifstream file;
    try {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    } catch ( std::exception& e ) {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar* shader_string = str.c_str();
    const GLint   shader_string_length = static_cast<GLint>( str.length() );

    // Define o código do shader GLSL, contido na string "shader_string"
    glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

    // Compila o código do shader GLSL (em tempo de execução)
    glCompileShader(shader_id);

    // Verificamos se ocorreu algum erro ou "warning" durante a compilação
    GLint compiled_ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memória para guardar o log de compilação.
    // A chamada "new" em C++ é equivalente ao "malloc()" do C.
    GLchar* log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilação
    if ( log_length != 0 )
    {
        std::string  output;

        if ( !compiled_ok )
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
            output += "WARNING: OpenGL compilation of \"";
            output += filename;
            output += "\".\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }

        fprintf(stderr, "%s", output.c_str());
    }

    // A chamada "delete" em C++ é equivalente ao "free()" do C
    delete [] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    // Criamos um identificador (ID) para este programa de GPU
    GLuint program_id = glCreateProgram();

    // Definição dos dois shaders GLSL que devem ser executados pelo programa
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    // Linkagem dos shaders acima ao programa
    glLinkProgram(program_id);

    // Verificamos se ocorreu algum erro durante a linkagem
    GLint linked_ok = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

    // Imprime no terminal qualquer erro de linkagem
    if ( linked_ok == GL_FALSE )
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memória para guardar o log de compilação.
        // A chamada "new" em C++ é equivalente ao "malloc()" do C.
        GLchar* log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ é equivalente ao "free()" do C
        delete [] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para deleção após serem linkados 
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    // Retornamos o ID gerado acima
    return program_id;
}

// Definição da função que será chamada sempre que a janela do sistema
// operacional for redimensionada, por consequência alterando o tamanho do
// "framebuffer" (região de memória onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    // Indicamos que queremos renderizar em toda região do framebuffer. A
    // função "glViewport" define o mapeamento das "normalized device
    // coordinates" (NDC) para "pixel coordinates".  Essa é a operação de
    // "Screen Mapping" ou "Viewport Mapping" vista em aula ({+ViewportMapping2+}).
    glViewport(0, 0, width, height);

    // Atualizamos também a razão que define a proporção da janela (largura /
    // altura), a qual será utilizada na definição das matrizes de projeção,
    // tal que não ocorra distorções durante o processo de "Screen Mapping"
    // acima, quando NDC é mapeado para coordenadas de pixels. Veja slides 205-215 do documento Aula_09_Projecoes.pdf.
    //
    // O cast para float é necessário pois números inteiros são arredondados ao
    // serem divididos!
    g_ScreenRatio = (float)width / height;
}

void Correcao_KeyCallback(int key, int action, int mod);

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    // =======================
    // Não modifique esta chamada! Ela é utilizada para correção automatizada dos
    // laboratórios. Deve ser sempre o primeiro comando desta função KeyCallback().
    Correcao_KeyCallback(key, action, mod);
    // =======================

    HandleKey(window, key, action, mod);
}

// Tratamento das teclas (chamado pelo KeyCallback e pelo harness E2E).
void HandleKey(GLFWwindow* window, int key, int action, int mods)
{
    // Se o usuário pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // Fim de jogo: Enter (ou Espaço) recomeça a corrida. O return evita que
    // o mesmo Espaço vire um pulo na corrida nova.
    if (action == GLFW_PRESS && g_Game.mode == GameMode::GAME_OVER &&
        (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE))
    {
        RestartRun();
        return;
    }

    // Controles do jogador (só ao pressionar; a repetição automática da
    // tecla segurada é ignorada):
    //   setas esquerda/direita ou A/D -> troca de faixa
    //   seta para cima, W ou espaço   -> pulo
    //   seta para baixo ou S          -> rolamento
    if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case GLFW_KEY_LEFT:
        case GLFW_KEY_A:
            g_Game.HandleInput(Input::LEFT);
            break;
        case GLFW_KEY_RIGHT:
        case GLFW_KEY_D:
            g_Game.HandleInput(Input::RIGHT);
            break;
        case GLFW_KEY_UP:
        case GLFW_KEY_W:
        case GLFW_KEY_SPACE:
            g_Game.HandleInput(Input::JUMP);
            break;
        case GLFW_KEY_DOWN:
        case GLFW_KEY_S:
            g_Game.HandleInput(Input::ROLL);
            break;
        default:
            break;
        }
    }

    // Se o usuário apertar a tecla H, fazemos um "toggle" do texto informativo mostrado na tela.
    if (key == GLFW_KEY_H && action == GLFW_PRESS)
    {
        g_ShowInfoText = !g_ShowInfoText;
    }

    // Se o usuário apertar a tecla R, recarregamos os shaders dos arquivos "shader_fragment.glsl" e "shader_vertex.glsl".
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        LoadShadersFromFiles();
        fprintf(stdout,"Shaders recarregados!\n");
        fflush(stdout);
    }
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char* description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

// Esta função recebe um vértice com coordenadas de modelo p_model e passa o
// mesmo por todos os sistemas de coordenadas armazenados nas matrizes model,
// view, e projection; e escreve na tela as matrizes e pontos resultantes
// dessas transformações.
void TextRendering_ShowModelViewProjection(
    GLFWwindow* window,
    glm::mat4 projection,
    glm::mat4 view,
    glm::mat4 model,
    glm::vec4 p_model
)
{
    if ( !g_ShowInfoText )
        return;

    glm::vec4 p_world = model*p_model;
    glm::vec4 p_camera = view*p_world;
    glm::vec4 p_clip = projection*p_camera;
    glm::vec4 p_ndc = p_clip / p_clip.w;

    float pad = TextRendering_LineHeight(window);

    TextRendering_PrintString(window, " Model matrix             Model     In World Coords.", -1.0f, 1.0f-pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, model, p_model, -1.0f, 1.0f-2*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-6*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-7*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-8*pad, 1.0f);

    TextRendering_PrintString(window, " View matrix              World     In Camera Coords.", -1.0f, 1.0f-9*pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, view, p_world, -1.0f, 1.0f-10*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-14*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-15*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-16*pad, 1.0f);

    TextRendering_PrintString(window, " Projection matrix        Camera                    In NDC", -1.0f, 1.0f-17*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductDivW(window, projection, p_camera, -1.0f, 1.0f-18*pad, 1.0f);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glm::vec2 a = glm::vec2(-1, -1);
    glm::vec2 b = glm::vec2(+1, +1);
    glm::vec2 p = glm::vec2( 0,  0);
    glm::vec2 q = glm::vec2(width, height);

    glm::mat4 viewport_mapping = Matrix(
        (q.x - p.x)/(b.x-a.x), 0.0f, 0.0f, (b.x*p.x - a.x*q.x)/(b.x-a.x),
        0.0f, (q.y - p.y)/(b.y-a.y), 0.0f, (b.y*p.y - a.y*q.y)/(b.y-a.y),
        0.0f , 0.0f , 1.0f , 0.0f ,
        0.0f , 0.0f , 0.0f , 1.0f
    );

    TextRendering_PrintString(window, "                                                       |  ", -1.0f, 1.0f-22*pad, 1.0f);
    TextRendering_PrintString(window, "                            .--------------------------'  ", -1.0f, 1.0f-23*pad, 1.0f);
    TextRendering_PrintString(window, "                            V                           ", -1.0f, 1.0f-24*pad, 1.0f);

    TextRendering_PrintString(window, " Viewport matrix           NDC      In Pixel Coords.", -1.0f, 1.0f-25*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductMoreDigits(window, viewport_mapping, p_ndc, -1.0f, 1.0f-26*pad, 1.0f);
}

// Imprime 'str' na cor (r,g,b) com uma "sombra" preta deslocada 2 pixels
// para baixo e para a direita: legível sobre qualquer fundo.
static void PrintShadowed(GLFWwindow* window, const char* str, float x, float y, float scale,
                          float r, float g, float b)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    float dx = 2.0f * 2.0f / (float)width;  // 2 pixels em NDC
    float dy = 2.0f * 2.0f / (float)height;
    TextRendering_SetColor(0.0f, 0.0f, 0.0f);
    TextRendering_PrintString(window, str, x + dx, y - dy, scale);
    TextRendering_SetColor(r, g, b);
    TextRendering_PrintString(window, str, x, y, scale);
    TextRendering_SetColor(0.0f, 0.0f, 0.0f);
}

// Placar no canto superior esquerdo (sempre visível): pontos, moedas e
// distância. Com o texto informativo ligado (tecla H), também a velocidade e a
// faixa, no canto inferior esquerdo.
void TextRendering_ShowRunInfo(GLFWwindow* window)
{
    float lineheight = TextRendering_LineHeight(window);
    float charwidth  = TextRendering_CharWidth(window);
    const float hud_scale = 1.5f;

    char buffer[80];
    snprintf(buffer, sizeof(buffer), "Pontos: %d", (int)g_Game.score);
    PrintShadowed(window, buffer, -1.0f + charwidth, 1.0f - 1.5f * hud_scale * lineheight, hud_scale, 1.0f, 1.0f, 1.0f);
    snprintf(buffer, sizeof(buffer), "Moedas: %d", g_Game.coins_collected);
    PrintShadowed(window, buffer, -1.0f + charwidth, 1.0f - 2.5f * hud_scale * lineheight, hud_scale, 1.0f, 0.85f, 0.2f);
    snprintf(buffer, sizeof(buffer), "Distancia: %.0f m", g_Game.player.s);
    PrintShadowed(window, buffer, -1.0f + charwidth, 1.0f - 3.5f * hud_scale * lineheight, hud_scale, 1.0f, 1.0f, 1.0f);

    if ( !g_ShowInfoText )
        return;

    snprintf(buffer, sizeof(buffer), "Velocidade: %4.1f m/s   Faixa: %+d",
             g_Game.player.speed, g_Game.player.lane);
    TextRendering_PrintString(window, buffer, -1.0f+lineheight/10, -1.0f+2*lineheight/10, 1.0f);
}

// Imprime 'str' centralizado horizontalmente na altura y (NDC), com sombra.
static void PrintCentered(GLFWwindow* window, const char* str, float y, float scale,
                          float r, float g, float b)
{
    float width = (float)strlen(str) * TextRendering_CharWidth(window) * scale;
    PrintShadowed(window, str, -0.5f * width, y, scale, r, g, b);
}

// Mensagem de fim de jogo, no centro da tela.
void TextRendering_ShowGameOver(GLFWwindow* window)
{
    if (g_Game.mode != GameMode::GAME_OVER)
        return;

    float lineheight = TextRendering_LineHeight(window);
    char buffer[80];
    PrintCentered(window, "GAME OVER", 0.15f, 4.0f, 1.0f, 0.25f, 0.15f);
    snprintf(buffer, sizeof(buffer), "Pontos: %d   Moedas: %d", (int)g_Game.score, g_Game.coins_collected);
    PrintCentered(window, buffer, 0.15f - 2.5f * lineheight, 2.0f, 1.0f, 1.0f, 1.0f);
    PrintCentered(window, "Enter (ou Espaco) para reiniciar", 0.15f - 5.0f * lineheight, 2.0f, 1.0f, 1.0f, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    static float old_seconds = (float)glfwGetTime();
    static int   ellapsed_frames = 0;
    static char  buffer[20] = "?? fps";
    static int   numchars = 7;

    ellapsed_frames += 1;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    // Número de segundos desde o último cálculo do fps
    float ellapsed_seconds = seconds - old_seconds;

    if ( ellapsed_seconds > 1.0f )
    {
        numchars = snprintf(buffer, 20, "%.2f fps", ellapsed_frames / ellapsed_seconds);
    
        old_seconds = seconds;
        ellapsed_frames = 0;
    }

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
}

// Função para debugging: imprime no terminal todas informações de um modelo
// geométrico carregado de um arquivo ".obj".
// Veja: https://github.com/syoyo/tinyobjloader/blob/22883def8db9ef1f3ffb9b404318e7dd25fdbb51/loader_example.cc#L98
void PrintObjModelInfo(ObjModel* model)
{
  const tinyobj::attrib_t                & attrib    = model->attrib;
  const std::vector<tinyobj::shape_t>    & shapes    = model->shapes;
  const std::vector<tinyobj::material_t> & materials = model->materials;

  printf("# of vertices  : %d\n", (int)(attrib.vertices.size() / 3));
  printf("# of normals   : %d\n", (int)(attrib.normals.size() / 3));
  printf("# of texcoords : %d\n", (int)(attrib.texcoords.size() / 2));
  printf("# of shapes    : %d\n", (int)shapes.size());
  printf("# of materials : %d\n", (int)materials.size());

  for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
    printf("  v[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.vertices[3 * v + 0]),
           static_cast<const double>(attrib.vertices[3 * v + 1]),
           static_cast<const double>(attrib.vertices[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.normals.size() / 3; v++) {
    printf("  n[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.normals[3 * v + 0]),
           static_cast<const double>(attrib.normals[3 * v + 1]),
           static_cast<const double>(attrib.normals[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.texcoords.size() / 2; v++) {
    printf("  uv[%ld] = (%f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.texcoords[2 * v + 0]),
           static_cast<const double>(attrib.texcoords[2 * v + 1]));
  }

  // For each shape
  for (size_t i = 0; i < shapes.size(); i++) {
    printf("shape[%ld].name = %s\n", static_cast<long>(i),
           shapes[i].name.c_str());
    printf("Size of shape[%ld].indices: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.indices.size()));

    size_t index_offset = 0;

    assert(shapes[i].mesh.num_face_vertices.size() ==
           shapes[i].mesh.material_ids.size());

    printf("shape[%ld].num_faces: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.num_face_vertices.size()));

    // For each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];

      printf("  face[%ld].fnum = %ld\n", static_cast<long>(f),
             static_cast<unsigned long>(fnum));

      // For each vertex in the face
      for (size_t v = 0; v < fnum; v++) {
        tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
        printf("    face[%ld].v[%ld].idx = %d/%d/%d\n", static_cast<long>(f),
               static_cast<long>(v), idx.vertex_index, idx.normal_index,
               idx.texcoord_index);
      }

      printf("  face[%ld].material_id = %d\n", static_cast<long>(f),
             shapes[i].mesh.material_ids[f]);

      index_offset += fnum;
    }

    printf("shape[%ld].num_tags: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.tags.size()));
    for (size_t t = 0; t < shapes[i].mesh.tags.size(); t++) {
      printf("  tag[%ld] = %s ", static_cast<long>(t),
             shapes[i].mesh.tags[t].name.c_str());
      printf(" ints: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].intValues.size(); ++j) {
        printf("%ld", static_cast<long>(shapes[i].mesh.tags[t].intValues[j]));
        if (j < (shapes[i].mesh.tags[t].intValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" floats: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].floatValues.size(); ++j) {
        printf("%f", static_cast<const double>(
                         shapes[i].mesh.tags[t].floatValues[j]));
        if (j < (shapes[i].mesh.tags[t].floatValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" strings: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].stringValues.size(); ++j) {
        printf("%s", shapes[i].mesh.tags[t].stringValues[j].c_str());
        if (j < (shapes[i].mesh.tags[t].stringValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");
      printf("\n");
    }
  }

  for (size_t i = 0; i < materials.size(); i++) {
    printf("material[%ld].name = %s\n", static_cast<long>(i),
           materials[i].name.c_str());
    printf("  material.Ka = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].ambient[0]),
           static_cast<const double>(materials[i].ambient[1]),
           static_cast<const double>(materials[i].ambient[2]));
    printf("  material.Kd = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].diffuse[0]),
           static_cast<const double>(materials[i].diffuse[1]),
           static_cast<const double>(materials[i].diffuse[2]));
    printf("  material.Ks = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].specular[0]),
           static_cast<const double>(materials[i].specular[1]),
           static_cast<const double>(materials[i].specular[2]));
    printf("  material.Tr = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].transmittance[0]),
           static_cast<const double>(materials[i].transmittance[1]),
           static_cast<const double>(materials[i].transmittance[2]));
    printf("  material.Ke = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].emission[0]),
           static_cast<const double>(materials[i].emission[1]),
           static_cast<const double>(materials[i].emission[2]));
    printf("  material.Ns = %f\n",
           static_cast<const double>(materials[i].shininess));
    printf("  material.Ni = %f\n", static_cast<const double>(materials[i].ior));
    printf("  material.dissolve = %f\n",
           static_cast<const double>(materials[i].dissolve));
    printf("  material.illum = %d\n", materials[i].illum);
    printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
    printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
    printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
    printf("  material.map_Ns = %s\n",
           materials[i].specular_highlight_texname.c_str());
    printf("  material.map_bump = %s\n", materials[i].bump_texname.c_str());
    printf("  material.map_d = %s\n", materials[i].alpha_texname.c_str());
    printf("  material.disp = %s\n", materials[i].displacement_texname.c_str());
    printf("  <<PBR>>\n");
    printf("  material.Pr     = %f\n", materials[i].roughness);
    printf("  material.Pm     = %f\n", materials[i].metallic);
    printf("  material.Ps     = %f\n", materials[i].sheen);
    printf("  material.Pc     = %f\n", materials[i].clearcoat_thickness);
    printf("  material.Pcr    = %f\n", materials[i].clearcoat_thickness);
    printf("  material.aniso  = %f\n", materials[i].anisotropy);
    printf("  material.anisor = %f\n", materials[i].anisotropy_rotation);
    printf("  material.map_Ke = %s\n", materials[i].emissive_texname.c_str());
    printf("  material.map_Pr = %s\n", materials[i].roughness_texname.c_str());
    printf("  material.map_Pm = %s\n", materials[i].metallic_texname.c_str());
    printf("  material.map_Ps = %s\n", materials[i].sheen_texname.c_str());
    printf("  material.norm   = %s\n", materials[i].normal_texname.c_str());
    std::map<std::string, std::string>::const_iterator it(
        materials[i].unknown_parameter.begin());
    std::map<std::string, std::string>::const_iterator itEnd(
        materials[i].unknown_parameter.end());

    for (; it != itEnd; it++) {
      printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
    }
    printf("\n");
  }
}

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :

