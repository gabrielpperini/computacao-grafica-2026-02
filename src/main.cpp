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
#include <cfloat>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/common.hpp>   // glm::min/max de vetores (AABBs)

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

// Interface gráfica (menu, pausa, game over, HUD): Dear ImGui, em third_party/imgui.
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"
#include "game_config.h"
#include "object_ids.h"
#include "particles.h"
#include "autotest.h"
#include "game.h"
#include "camera.h"
#include "collisions.h"
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
// Constrói representação de um ObjModel como malha de triângulos para
// renderização. Com merge_as != NULL, todos os shapes do arquivo viram UM
// objeto de g_VirtualScene com esse nome (modelos da Kenney repetem o nome do
// grupo e usam nomes genéricos como "wheels-front" em vários arquivos).
void BuildTrianglesAndAddToVirtualScene(ObjModel*, const char* merge_as = NULL);
// Carrega um OBJ (com normais calculadas se faltarem) para g_VirtualScene.
// max_y: achata (em coordenadas do modelo, só na memória: o .obj não muda)
// todo vértice acima dessa altura; padrão = não mexe.
void LoadObjToScene(const char* filename, const char* merge_as = NULL,
                    float max_y = std::numeric_limits<float>::max());
void ComputeNormals(ObjModel* model); // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles(); // Carrega os shaders de vértice e fragmento, criando um programa de GPU
GLuint LoadTextureImage(const char* filename); // Carrega uma imagem de textura e devolve o id da textura OpenGL
void DrawVirtualObject(const char* object_name); // Desenha um objeto armazenado em g_VirtualScene
// Desenha um objeto da cena com sua matriz de modelagem, id, textura (ligada
// na unidade 0), modo de UV e um tom extra (multiplica o Kd do material).
void DrawObject(const char* name, const glm::mat4& model, int object_id, GLuint texture, int uv_mode,
                float uv_scale = 1.0f, const glm::vec3& tint = glm::vec3(1.0f, 1.0f, 1.0f));
GLuint LoadShader_Vertex(const char* filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char* filename); // Carrega um fragment shader
void LoadShader(const char* filename, GLuint shader_id); // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU
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

// Texto de depuração (tecla H). Definidas após main().
void TextRendering_ShowRunInfo(GLFWwindow* window);
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
// Tratamento das teclas, separado do KeyCallback para que o harness E2E (autotest.cpp)
// possa injetar teclas sem passar pelo Correcao_KeyCallback.
void HandleKey(GLFWwindow* window, int key, int action, int mods);
// Callback do mouse (câmera livre) e o tratamento do deslocamento em pixels,
// separado para que o harness E2E possa injetar movimento do mouse.
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void FreeCameraMouseLook(float dx_pixels, float dy_pixels);
// Direção de movimento (direita, cima, frente) da câmera livre a partir das
// teclas seguradas.
glm::vec3 FreeCameraInputDirection();

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

// Câmera livre (tecla C, com o jogo pausado; veja include/camera.h). Global e
// não-static para o harness E2E conferir o deslocamento.
FreeCamera g_FreeCamera;

// Teclas atualmente seguradas (atualizada por HandleKey em PRESS/RELEASE, tanto
// no jogo real quanto no E2E): a câmera livre se move a cada quadro, com dt,
// enquanto WASD/Espaço/Shift estão segurados, em vez de depender da repetição
// automática de teclas do sistema operacional.
bool g_KeyHeld[GLFW_KEY_LAST + 1] = {false};

// Último ponto do cursor visto (para calcular o deslocamento do mouse). Ao
// capturar o cursor ele "salta" para o centro da janela; por isso o 1o evento
// após a captura só recalibra o ponto, sem girar a câmera.
double g_LastCursorX = 0.0, g_LastCursorY = 0.0;
bool   g_CursorPointValid = false;

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
// Desenha as faixas [first_strip, first_strip + strip_count) da malha
// (strip_count < 0 = até o fim) com um único glMultiDrawArrays.
void DrawDynamicStripMesh(const DynamicStripMesh& mesh, int object_id, GLuint texture, int uv_mode,
                          float uv_scale = 1.0f, const glm::vec3& tint = glm::vec3(1.0f, 1.0f, 1.0f),
                          int first_strip = 0, int strip_count = -1);

DynamicStripMesh g_TrackGround;   // leito da pista (faixa 0) + terreno dos lados
DynamicStripMesh g_TrackRails;    // 6 trilhos (2 por faixa), topo e laterais
DynamicStripMesh g_TrackSleepers; // dormentes de madeira (uma faixa de 4 vértices por face)

// Imagens de textura carregadas em main() (veja data/FONTES.txt).
struct Textures
{
    GLuint terrain;  // rocky_terrain_02 (Poly Haven): terreno dos lados
    GLuint gravel;   // gravel_stones (Poly Haven): brita do leito da pista
    GLuint wood;     // rough_wood (Poly Haven): dormentes
    GLuint metal;    // metal_plate (Poly Haven): trilhos
    GLuint atlas;    // data/procedural/atlas.png: barreiras, placa, moeda, power-ups
    GLuint train;    // Kenney Train Kit (colormap)
    GLuint player;   // Kenney Blocky Characters (texture-b)
    GLuint city;     // Kenney City Kit Commercial (colormap): prédios
    GLuint roads;    // Kenney City Kit Roads (colormap): postes de luz
    GLuint brick;    // red_brick (Poly Haven): túneis (abóbada e portais)
};
Textures g_Tex;

// Personagem (Kenney Blocky Characters, "character-b"): seis partes nomeadas
// no OBJ, desenhadas com uma hierarquia de matrizes (corpo -> braços/pernas
// girando em torno do ombro/quadril). O modelo olha para +Z; o jogador corre
// para -Z local (forward com heading 0), então gira 180 graus.
const float PLAYER_MODEL_YAW    = 3.14159265f;
const float RUN_CYCLE_PER_METER = 0.75f; // rad de fase da passada por metro corrido
const float LEG_SWING           = 0.75f; // rad (amplitude das pernas)
const float ARM_SWING           = 0.65f; // rad (amplitude dos braços)
float g_RunPhase = 0.0f;                 // fase da passada (acumula speed*dt)
void DrawPlayer(const Player& player, const TrackFrame& frame);

// Obstáculos: cada tipo é um modelo escalado para que a sua AABB coincida com
// a caixa de colisão (a união de ObstacleParts), cujas medidas vêm da tabela
// única OBSTACLE_SIZE (game.h). O trem são TRAIN_CARS carros em fila.
const int TRAIN_CARS = 3;
const char* const TRAIN_CAR_MODELS[TRAIN_CARS] = {"train_front", "train_middle", "train_rear"};
// Altura do teto plano dos carros do Train Kit, em coordenadas do modelo. O
// topo da AABB (1.571196) é o de nervuras pequenas (0.1 x 0.4) sobre o teto;
// escalando pela AABB, o jogador em pé no topo da caixa de colisão flutuaria
// 0.2 m acima do teto desenhado.
const float TRAIN_MODEL_ROOF_Y = 1.471196f;
// As nervuras (8 vértices por carro, uma caixa que não é um objeto separado no
// OBJ) são achatadas ao carregar (Tarefa 7.1b): viram uma placa a
// TRAIN_ROOF_RIB_HEIGHT acima do teto (~1 cm no mundo, sem Z-fighting com ele).
// Sem isso o pé do jogador que anda no teto entrava 0.2 m nelas.
const float TRAIN_ROOF_RIB_HEIGHT = 0.004f;
const int OBSTACLE_OBJECT_ID[OBSTACLE_TYPE_COUNT] = {BARRIER_LOW, BARRIER_HIGH, SIGN, TRAIN, RAMP};
// Tom das tábuas da rampa sobre rough_wood (madeira envelhecida, como os dormentes).
const glm::vec3 RAMP_DECK_TINT(0.85f, 0.70f, 0.55f);
void DrawObstacle(const Obstacle& o);

// Moedas: modelo de moeda (data/procedural/coin.obj) em pé, girando em torno
// do eixo vertical. O disco desenhado é um pouco menor que a esfera de coleta
// (COIN_RADIUS), para a coleta ser "generosa".
const float COIN_DRAW_DIAMETER  = 0.8f;  // m
const float COIN_SPIN_SPEED     = 3.0f;  // rad/s
float g_CoinSpin = 0.0f;                 // ângulo atual (acumula dt)
void DrawCoin(const Coin& c);

// Prédios (Kenney City Kit Commercial): nomes em g_VirtualScene e tons de
// pintura (pastéis) que multiplicam a textura, para variar a rua.
const char* const BUILDING_MODELS[BUILDING_VARIANT_COUNT] = {
    "building_a", "building_d", "building_f", "building_h", "building_skyscraper_a", "building_skyscraper_e"};
const glm::vec3 BUILDING_TINTS[BUILDING_TINT_COUNT] = {
    glm::vec3(0.90f, 0.90f, 0.90f), glm::vec3(1.00f, 0.76f, 0.60f),
    glm::vec3(0.70f, 0.84f, 1.00f), glm::vec3(0.78f, 0.95f, 0.74f)};
void DrawBuilding(const Track& track, const BuildingInstance& b);

// Postes de luz (Kenney City Kit Roads, "light-curved"): haste com braço
// curvo para o lado da pista. No modelo, a haste fica em x = z = 0 e a cabeça
// da luminária é a caixa centrada em LAMP_MODEL_HEAD (unidades do modelo).
// A escala põe o centro da cabeça em LAMP_HEIGHT; ali fica também a luz pontual
// e o bloco emissivo (LAMP) que a representa acesa.
const glm::vec3 LAMP_MODEL_HEAD = glm::vec3(0.0f, 0.6639f, -0.175f);
glm::vec4 LampHeadPosition(const Track& track, const LampInstance& l); // centro da cabeça (mundo)
void DrawLamp(const Track& track, const LampInstance& l);

// Túneis (Tarefa 4.2): módulos de abóbada e portais (data/procedural/
// tunnel_*.obj, com as medidas TUNNEL_* de scenery.h) desenhados como
// instâncias ao longo dos segmentos de túnel, e as luminárias do teto: blocos
// emissivos (LAMP) de TUNNEL_LAMP_BOX_* m, com a face de cima rente ao teto.
const float TUNNEL_LAMP_BOX_WIDTH  = 0.5f;  // m (lateral)
const float TUNNEL_LAMP_BOX_HEIGHT = 0.12f; // m
const float TUNNEL_LAMP_BOX_LENGTH = 1.6f;  // m (ao longo da pista)
const float TUNNEL_LAMP_BOX_TOP    = 6.96f; // m (o teto, no centro, fica a 7 m)
void DrawTunnelPiece(const Track& track, const TunnelPiece& piece);
void DrawTunnelLamp(const Track& track, float s);
// Escurecimento do sol e do céu com a câmera dentro do túnel (0..1, rampa
// de TUNNEL_DIM_TIME s; veja StepTunnelDimming() em scenery.cpp).
float g_TunnelDimming = 0.0f;

// Power-ups (Tarefa 5.2): o modelo de cada tipo flutua (sobe e desce) e gira
// em torno do eixo vertical, com o ângulo e a fase acumulados por dt. O maior
// lado do modelo vira POWERUP_DRAW_SIZE (um pouco menor que a esfera de coleta,
// de diâmetro 2 * POWERUP_RADIUS).
const float POWERUP_DRAW_SIZE  = 1.2f;  // m
const float POWERUP_SPIN_SPEED = 2.0f;  // rad/s
const float POWERUP_BOB_SPEED  = 3.0f;  // rad/s (fase da flutuação)
const float POWERUP_BOB_HEIGHT = 0.12f; // m (amplitude)
float g_PowerUpSpin = 0.0f;             // ângulo atual (acumula dt)
float g_PowerUpBob  = 0.0f;             // fase da flutuação (acumula dt)
void DrawPowerUp(const PowerUp& p);

// Porco voador (Tarefa 5.3): comprimento do corpo na tela (o modelo tem ~0.85 m,
// pequeno demais para ser visto lá no alto), batida das asas (frequência e
// amplitude) e raiz de cada asa no modelo.
const float PIG_DRAW_LENGTH   = 3.5f;  // m
const float PIG_FLAP_FREQ     = 2.5f;  // batidas por segundo
const float PIG_FLAP_AMPLITUDE = 0.6f; // rad
// Raiz de cada asa no modelo flying_pig (x = ±PIG_WING_ROOT_X): os mesmos
// números de model_flying_pig() em tools/gen_models.py (raiz nas costas).
const float PIG_WING_ROOT_X = 0.16f;
const float PIG_WING_ROOT_Y = 0.50f;
const float PIG_WING_ROOT_Z = 0.02f;
void DrawFlyingPig(const Game& game);

// Partículas de coleta (Tarefa 6.1): sistema sem GL (include/particles.h) + um
// quad unitário com UV, desenhado uma vez por partícula como billboard
// (voltado para a câmera) com blending aditivo, depois de todos os opacos.
// g_Particles é global e não-static para o harness E2E conferi-lo.
ParticleSystem g_Particles(PARTICLE_CAPACITY);
unsigned g_ParticleSeed = 1;          // semente da próxima explosão (incrementa a cada evento)
GLuint g_ParticleQuadVAO = 0;
const int PARTICLES_PER_COIN    = 30;
const int PARTICLES_PER_POWERUP = 80;
const glm::vec3 PARTICLE_COIN_COLOR = glm::vec3(1.00f, 0.78f, 0.20f); // dourado
void CreateParticleQuad();
glm::vec3 PowerUpParticleColor(PowerUpType type);
void EmitCollectParticles(Game& game);
void DrawParticles(const glm::mat4& view);

// Recomeça a corrida (Enter/Espaço em GAME_OVER) e reposiciona a câmera.
void RestartRun();

// Interface gráfica com Dear ImGui (Tarefa 6.2): tela de menu, pausa, fim de
// jogo e o HUD da corrida. A cena 3D continua sendo desenhada (congelada)
// atrás das janelas.
void Gui_Init(GLFWwindow* window);     // depois de instalar os nossos callbacks
void Gui_Frame(GLFWwindow* window);    // monta e desenha a GUI (depois da cena 3D)
void Gui_Shutdown();
// Ações dos botões, também usadas pelos atalhos de teclado.
void StartRunFromMenu();               // MENU -> PLAYING (corrida nova)
void PauseRun(GLFWwindow* window);     // PLAYING/FREE_CAMERA -> PAUSED
void ResumeRun();                      // PAUSED -> PLAYING
void GoToMenu();                       // PAUSED/GAME_OVER -> MENU (corrida nova ao fundo)
void EnterFreeCamera(GLFWwindow* window);
void LeaveFreeCamera(GLFWwindow* window);
// Harness E2E: agenda um clique de mouse de verdade (pela fila de eventos do
// ImGui) no centro do botão com o rótulo dado, desenhado no último quadro.
// Devolve false se esse botão não estava na tela.
bool Gui_ClickButton(const char* label);
// Para o E2E: o botão com esse rótulo foi desenhado no último quadro e está
// inteiro dentro da janela (e da área visível da janela do ImGui que o contém).
bool Gui_ButtonOnScreen(const char* label);

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
GLint g_particle_alpha_uniform;
GLint g_num_tunnels_uniform;
GLint g_tunnel_start_uniform;
GLint g_tunnel_forward_uniform;
GLint g_tunnel_shape_uniform;
GLint g_fog_color_uniform;
GLint g_fog_params_uniform;

// Cena: sol (luz direcional) e luzes pontuais dos postes.
// SUN_DIRECTION aponta da superfície PARA o sol: de cima, um pouco de lado e
// de TRÁS do jogador (+Z com heading 0), para que as faces dos obstáculos
// voltadas para a câmera (normal +Z) também recebam luz direta.
const glm::vec4 SUN_DIRECTION    = glm::vec4(0.30f, 1.00f, 0.70f, 0.0f);
const glm::vec3 SUN_COLOR        = glm::vec3(0.95f, 0.92f, 0.82f);
const glm::vec3 AMBIENT_COLOR    = glm::vec3(0.22f, 0.25f, 0.32f); // céu azulado
const glm::vec3 LAMP_LIGHT_COLOR = glm::vec3(2.8f, 2.0f, 1.0f);   // luz quente, intensidade > 1
// Luminárias do teto do túnel: luz mais branca e mais fraca que a dos postes
// (são várias, a cada 10 m, e as paredes ficam perto delas: com a mesma
// intensidade o túnel ficaria tão claro quanto o dia lá fora).
const glm::vec3 TUNNEL_LAMP_LIGHT_COLOR = glm::vec3(0.9f, 0.82f, 0.65f);
// Fração do ambiente (céu) que sobra nos pontos dentro do túnel (o sol some).
const float TUNNEL_INSIDE_AMBIENT = 0.35f;
// Túneis enviados ao shader. É também o MAX_TUNNELS dos shaders: LoadShader()
// insere "#define MAX_TUNNELS" (e MAX_POINT_LIGHTS, de scenery.h) no código
// GLSL ao carregá-lo, então o valor só existe aqui.
const int MAX_SHADER_TUNNELS = 4;
// A sombra do túnel no shader (SkyVisibility) vai da face de fora de um portal
// à do outro, menos uma folga: a fachada do portal (em s0 - DEPTH) fica fora do
// volume, iluminada pelo sol, sem "piscar" por erro de arredondamento; o vão
// dos portais (antes um anel claro de 1 m) fica na sombra.
const float TUNNEL_SHADOW_PORTAL_EXTENSION = TUNNEL_PORTAL_DEPTH - 0.05f; // m
// Cor do céu: fundo (glClearColor) e cor da névoa, em espaço de exibição.
const glm::vec3 SKY_COLOR = glm::vec3(0.53f, 0.75f, 0.95f);
// Névoa por distância da câmera: começa em FOG_START e cobre tudo em FOG_END,
// antes do fim do cenário gerado (SPAWN_AHEAD = 150 m à frente do jogador).
const float FOG_START = 90.0f;  // m
const float FOG_END   = 145.0f; // m
const glm::vec3 GRAVEL_TINT      = glm::vec3(0.95f, 0.90f, 0.85f); // brita um pouco mais quente

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
// Envia o material do tipo de objeto (com o Kd multiplicado por 'tint') para
// os uniforms do shader.
void ApplyMaterial(int object_id, const glm::vec3& tint);

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

    window = glfwCreateWindow(800, 600, "Rail Rush - INF01047", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos a função de callback que será chamada sempre que o usuário
    // pressionar alguma tecla do teclado.
    glfwSetKeyCallback(window, KeyCallback);

    // Mouse: usado na câmera livre (olhar ao redor).
    glfwSetCursorPosCallback(window, CursorPosCallback);

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

    // Carregamos as imagens de textura (fontes e licenças em data/FONTES.txt).
    // Cada objeto escolhe a sua em DrawObject(), que a liga na unidade de
    // textura 0 antes de desenhar.
    g_Tex.terrain = LoadTextureImage("../../data/rocky_terrain_02_diff_1k.jpg");
    g_Tex.gravel  = LoadTextureImage("../../data/gravel_stones_diff_1k.jpg");
    g_Tex.wood    = LoadTextureImage("../../data/rough_wood_diff_1k.jpg");
    g_Tex.metal   = LoadTextureImage("../../data/metal_plate_diff_1k.jpg");
    g_Tex.atlas   = LoadTextureImage("../../data/procedural/atlas.png");
    g_Tex.train   = LoadTextureImage("../../data/kenney/train-kit/Textures/colormap.png");
    g_Tex.player  = LoadTextureImage("../../data/kenney/blocky-characters/Textures/texture-b.png");
    g_Tex.city    = LoadTextureImage("../../data/kenney/city-kit-commercial/Textures/colormap.png");
    g_Tex.roads   = LoadTextureImage("../../data/kenney/city-kit-roads/Textures/colormap.png");
    g_Tex.brick   = LoadTextureImage("../../data/red_brick_diff_1k.jpg");

    // Construímos a representação de objetos geométricos através de malhas de
    // triângulos. Cada objeto é desenhado várias vezes (instâncias) com
    // matrizes de modelagem diferentes, reaproveitando o mesmo VAO.
    //
    // Personagem: partes "leg-left", "leg-right", "torso", "arm-left",
    // "arm-right" e "head" (nomes do OBJ da Kenney), desenhadas separadas.
    LoadObjToScene("../../data/kenney/blocky-characters/character-b.obj");
    // Trem: três carros de metrô (cabine da frente, meio, cabine de trás).
    // As nervuras do teto são achatadas (veja TRAIN_ROOF_RIB_HEIGHT).
    const float train_max_y = TRAIN_MODEL_ROOF_Y + TRAIN_ROOF_RIB_HEIGHT;
    LoadObjToScene("../../data/kenney/train-kit/train-electric-subway-a.obj", "train_front", train_max_y);
    LoadObjToScene("../../data/kenney/train-kit/train-electric-subway-b.obj", "train_middle", train_max_y);
    LoadObjToScene("../../data/kenney/train-kit/train-electric-subway-c.obj", "train_rear", train_max_y);
    // Prédios e poste de luz.
    LoadObjToScene("../../data/kenney/city-kit-commercial/building-a.obj", "building_a");
    LoadObjToScene("../../data/kenney/city-kit-commercial/building-d.obj", "building_d");
    LoadObjToScene("../../data/kenney/city-kit-commercial/building-f.obj", "building_f");
    LoadObjToScene("../../data/kenney/city-kit-commercial/building-h.obj", "building_h");
    LoadObjToScene("../../data/kenney/city-kit-commercial/building-skyscraper-a.obj", "building_skyscraper_a");
    LoadObjToScene("../../data/kenney/city-kit-commercial/building-skyscraper-e.obj", "building_skyscraper_e");
    LoadObjToScene("../../data/kenney/city-kit-roads/light-curved.obj", "lamp_post");
    // Malhas procedurais (tools/gen_models.py), com a textura atlas.png.
    LoadObjToScene("../../data/procedural/barrier_low.obj");
    LoadObjToScene("../../data/procedural/barrier_high.obj");
    LoadObjToScene("../../data/procedural/sign_board.obj");
    LoadObjToScene("../../data/procedural/coin.obj");
    LoadObjToScene("../../data/procedural/powerup_magnet.obj");
    LoadObjToScene("../../data/procedural/powerup_sneaker.obj");
    LoadObjToScene("../../data/procedural/powerup_multiplier.obj");
    LoadObjToScene("../../data/procedural/piggy_bank.obj");
    LoadObjToScene("../../data/procedural/flying_pig.obj"); // corpo + 2 asas (objetos separados)
    // Túnel: módulo de abóbada (4 m) e portal, com UV em metros (tijolos).
    LoadObjToScene("../../data/procedural/tunnel_section.obj");
    LoadObjToScene("../../data/procedural/tunnel_portal.obj");
    LoadObjToScene("../../data/procedural/ramp.obj"); // ramp_frame + ramp_deck (Tarefa 4.4)
    // Cubo: cabeça emissiva dos postes e luminárias do teto dos túneis.
    LoadObjToScene("../../data/cube.obj");

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();

    // GUI (ImGui). Inicializada DEPOIS de glfwSetKeyCallback/CursorPosCallback:
    // o backend do ImGui guarda os nossos callbacks e os chama antes dos dele
    // (o KeyCallback, com o Correcao_KeyCallback em primeiro, continua valendo).
    Gui_Init(window);

    // Quad unitário das partículas (criado em código, sem arquivo .obj).
    CreateParticleQuad();

    // Malhas dinâmicas do chão, dos trilhos e dos dormentes, com capacidade
    // para o trecho visível inteiro (veja MaxVerticesPerStrip() e
    // MaxSleeperVertices() em scenery.cpp).
    CreateDynamicStripMesh(&g_TrackGround, GROUND_STRIP_COUNT * MaxVerticesPerStrip());
    CreateDynamicStripMesh(&g_TrackRails, RAIL_STRIP_COUNT * MaxVerticesPerStrip());
    CreateDynamicStripMesh(&g_TrackSleepers, MaxSleeperVertices());

    // Lotes de vértices das faixas e lista de prédios: reaproveitados a cada
    // quadro (sem realocar memória).
    StripBatch ground_batch;
    StripBatch rails_batch;
    StripBatch sleepers_batch;
    std::vector<BuildingInstance> buildings;

    // Postes de luz: lista do trecho visível, posição de mundo da luz de cada
    // um, os índices dos mais próximos do jogador e o buffer de trabalho da
    // escolha (todos reaproveitados por quadro, sem alocar).
    std::vector<LampInstance> lamps;
    std::vector<glm::vec3> lamp_positions;
    std::vector<glm::vec3> lamp_colors;   // cor de cada luz de lamp_positions
    std::vector<int> picked_lamps;
    std::vector<PointDistance> pick_scratch;

    // Túneis do trecho visível: peças (módulos e portais), lâmpadas do teto
    // (s de cada uma) e trechos [s0, s1] enviados ao shader.
    std::vector<TunnelPiece> tunnel_pieces;
    std::vector<float> tunnel_lamps;
    std::vector<TunnelSpan> tunnel_spans;

    // Corrida: no autotest a semente fixa (12345) de g_Game é mantida para que
    // o cenário seja determinístico; no jogo normal, semente pelo relógio.
    // As medidas dos obstáculos vêm de OBSTACLE_SIZE (game.h), a mesma tabela
    // em que os modelos são escalados no desenho.
    if (!autotest)
        g_Game = Game((unsigned)time(NULL));

    // O jogo abre no menu. No autotest a corrida já começa em PLAYING (o
    // cenário que quer o menu pede start_in_menu; veja Autotest_Start()).
    if (!autotest)
        g_Game.mode = GameMode::MENU;

    // Configuração do cenário E2E (ex.: desligar o spawn), antes do 1º quadro.
    if (autotest)
        Autotest_Start();

    g_Camera.Snap(g_Game.track, g_Game.player.s, g_Game.player.x,
                  g_Game.track.IsTunnelAt(g_Game.player.s), g_Game.CameraTargetHeight());
    g_TunnelDimming = g_Game.track.IsTunnelAt(g_Game.player.s - g_Camera.Back()) ? 1.0f : 0.0f;

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
        // Partículas da coleta: as já existentes andam primeiro e só então
        // nascem as novas (a explosão aparece inteira no quadro da coleta). Só
        // avançam em PLAYING (dt = 0 fora dele): congelam na câmera livre, na
        // pausa e no game over, como o resto do jogo.
        g_Particles.Update(g_Game.mode == GameMode::PLAYING ? dt : 0.0f);
        EmitCollectParticles(g_Game);
        // Câmera livre e pausa: o jogo fica congelado (nada se anima, nem as
        // moedas). No menu e no game over as moedas seguem girando ao fundo.
        const bool free_camera = g_Game.mode == GameMode::FREE_CAMERA;
        const bool paused = g_Game.mode == GameMode::PAUSED;
        if (!free_camera && !paused)
        {
            g_CoinSpin = std::fmod(g_CoinSpin + COIN_SPIN_SPEED * dt, 6.2831853f);
            g_PowerUpSpin = std::fmod(g_PowerUpSpin + POWERUP_SPIN_SPEED * dt, 6.2831853f);
            g_PowerUpBob = std::fmod(g_PowerUpBob + POWERUP_BOB_SPEED * dt, 6.2831853f);
        }
        // Passada do personagem: a fase avança com a distância corrida (para
        // de mexer as pernas no GAME_OVER).
        if (g_Game.mode == GameMode::PLAYING)
            g_RunPhase = std::fmod(g_RunPhase + RUN_CYCLE_PER_METER * g_Game.player.speed * dt,
                                   6.2831853f);

        const Player& player = g_Game.player;
        TrackFrame player_frame = g_Game.track.FrameAt(player.s);
        // Câmera: recua/abaixa (suavemente) enquanto o JOGADOR está no túnel,
        // e sobe junto com o chão sob ele (rampa/teto de trem, não o pulo).
        // Na câmera livre a 3ª pessoa fica parada e a câmera livre anda com
        // as teclas seguradas (WASD, Espaço sobe, Shift desce), com dt.
        bool camera_in_tunnel;
        if (free_camera)
        {
            g_FreeCamera.Move(FreeCameraInputDirection(), dt);
            // Câmera livre dentro do volume do túnel (collisions.cpp), não só "sobre" ele.
            const glm::vec4 cp = g_FreeCamera.Pose().position;
            camera_in_tunnel = PointInsideTunnel(g_Game.track, glm::vec3(cp.x, cp.y, cp.z));
        }
        else
        {
            // Na pausa a câmera também fica parada (nem termina a suavização).
            if (!paused)
                g_Camera.Update(g_Game.track, player.s, player.x, g_Game.track.IsTunnelAt(player.s), dt,
                                g_Game.CameraTargetHeight());
            camera_in_tunnel = g_Game.track.IsTunnelAt(player.s - g_Camera.Back());
        }
        // Sol e céu escurecem (rampa de TUNNEL_DIM_TIME s) enquanto a própria
        // CÂMERA (em s - recuo, ou a câmera livre) está dentro do túnel.
        // Na pausa a rampa também congela (dt = 0): sem isto o céu seguiria
        // escurecendo/clareando com o jogo parado.
        g_TunnelDimming = StepTunnelDimming(g_TunnelDimming, camera_in_tunnel, paused ? 0.0f : dt);

        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como azul-claro (céu).
        // Tal cor é definida como coeficientes RGBA: Red, Green, Blue, Alpha;
        // isto é: Vermelho, Verde, Azul, Alpha (valor de transparência).
        // Conversaremos sobre sistemas de cores nas aulas de Modelos de Iluminação.
        //
        //           R     G     B     A
        glClearColor(SKY_COLOR.x, SKY_COLOR.y, SKY_COLOR.z, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
        // e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
        // os shaders de vértice e fragmentos).
        glUseProgram(g_GpuProgramID);

        // Duas câmeras look-at distintas: 3ª pessoa seguindo o jogador
        // (ThirdPersonCamera) ou a câmera livre (FreeCamera), ambas em
        // camera.cpp; cada uma devolve posição, view vector e up. Veja slides
        // 195-227 e 229-234 do documento Aula_08_Sistemas_de_Coordenadas.pdf.
        CameraPose pose = free_camera ? g_FreeCamera.Pose() : g_Camera.Pose();

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

        // Luzes: sol + as MAX_POINT_LIGHTS lâmpadas mais próximas do jogador
        // (as demais ficam longe demais para fazer diferença), entre as dos
        // postes de rua (centro da cabeça do poste desenhado, LampHeadPosition)
        // e as do teto dos túneis (TUNNEL_LAMP_HEIGHT acima da pista).
        ComputeLamps(g_Game.track, player.s, &lamps);
        ComputeTunnelLamps(g_Game.track, player.s, &tunnel_lamps);
        lamp_positions.clear();
        lamp_colors.clear();
        for (size_t i = 0; i < lamps.size(); ++i)
        {
            glm::vec4 h = LampHeadPosition(g_Game.track, lamps[i]);
            lamp_positions.push_back(glm::vec3(h.x, h.y, h.z));
            lamp_colors.push_back(LAMP_LIGHT_COLOR);
        }
        for (size_t i = 0; i < tunnel_lamps.size(); ++i)
        {
            glm::vec4 c = g_Game.track.FrameAt(tunnel_lamps[i]).position;
            lamp_positions.push_back(glm::vec3(c.x, c.y + TUNNEL_LAMP_HEIGHT, c.z));
            lamp_colors.push_back(TUNNEL_LAMP_LIGHT_COLOR);
        }
        glm::vec4 player_pos = player_frame.position + player_frame.right * player.x;
        PickNearestPoints(lamp_positions, glm::vec3(player_pos.x, player_pos.y, player_pos.z),
                          MAX_POINT_LIGHTS, &pick_scratch, &picked_lamps);
        float light_pos[4 * MAX_POINT_LIGHTS] = {0};
        float light_col[3 * MAX_POINT_LIGHTS] = {0};
        for (size_t i = 0; i < picked_lamps.size(); ++i)
        {
            const glm::vec3& lp = lamp_positions[picked_lamps[i]];
            const glm::vec3& lc = lamp_colors[picked_lamps[i]];
            light_pos[4 * i + 0] = lp.x;
            light_pos[4 * i + 1] = lp.y;
            light_pos[4 * i + 2] = lp.z;
            light_pos[4 * i + 3] = 1.0f;
            light_col[3 * i + 0] = lc.x;
            light_col[3 * i + 1] = lc.y;
            light_col[3 * i + 2] = lc.z;
        }
        // Câmera dentro do túnel: sol x TUNNEL_SUN_FACTOR e ambiente x
        // TUNNEL_AMBIENT_FACTOR, interpolados pela rampa g_TunnelDimming.
        const float sun_k = 1.0f + (TUNNEL_SUN_FACTOR - 1.0f) * g_TunnelDimming;
        const float amb_k = 1.0f + (TUNNEL_AMBIENT_FACTOR - 1.0f) * g_TunnelDimming;
        glUniform4f(g_sun_direction_uniform, SUN_DIRECTION.x, SUN_DIRECTION.y, SUN_DIRECTION.z, 0.0f);
        glUniform3f(g_sun_color_uniform, SUN_COLOR.x * sun_k, SUN_COLOR.y * sun_k, SUN_COLOR.z * sun_k);
        glUniform3f(g_ambient_color_uniform, AMBIENT_COLOR.x * amb_k, AMBIENT_COLOR.y * amb_k,
                    AMBIENT_COLOR.z * amb_k);
        glUniform1i(g_num_point_lights_uniform, (GLint)picked_lamps.size());
        glUniform4fv(g_point_light_position_uniform, MAX_POINT_LIGHTS, light_pos);
        glUniform3fv(g_point_light_color_uniform, MAX_POINT_LIGHTS, light_col);

        // Volumes dos túneis visíveis (o shader corta o sol e o céu dentro
        // deles): início, sentido e comprimento de cada um, e o perfil da
        // seção medido até o meio da parede.
        ComputeVisibleTunnels(g_Game.track, player.s, &tunnel_spans);
        float tun_start[4 * MAX_SHADER_TUNNELS] = {0};
        float tun_fwd[4 * MAX_SHADER_TUNNELS] = {0};
        const int num_tunnels = std::min((int)tunnel_spans.size(), MAX_SHADER_TUNNELS);
        for (int i = 0; i < num_tunnels; ++i)
        {
            // Volume da sombra: do portal de entrada ao de saída (o túnel fica
            // numa reta, então o referencial em s0 - extensão é o da entrada).
            const float ext = TUNNEL_SHADOW_PORTAL_EXTENSION;
            TrackFrame f = g_Game.track.FrameAt(tunnel_spans[i].s0 - ext);
            tun_start[4 * i + 0] = f.position.x;
            tun_start[4 * i + 1] = f.position.y;
            tun_start[4 * i + 2] = f.position.z;
            tun_start[4 * i + 3] = tunnel_spans[i].s1 - tunnel_spans[i].s0 + 2.0f * ext;
            tun_fwd[4 * i + 0] = f.forward.x;
            tun_fwd[4 * i + 1] = f.forward.y;
            tun_fwd[4 * i + 2] = f.forward.z;
        }
        const float mid_wall = TUNNEL_INNER_HALF_WIDTH + 0.5f * TUNNEL_WALL_THICKNESS;
        glUniform1i(g_num_tunnels_uniform, num_tunnels);
        glUniform4fv(g_tunnel_start_uniform, MAX_SHADER_TUNNELS, tun_start);
        glUniform4fv(g_tunnel_forward_uniform, MAX_SHADER_TUNNELS, tun_fwd);
        glUniform4f(g_tunnel_shape_uniform, mid_wall, TUNNEL_SPRING_HEIGHT, mid_wall, TUNNEL_INSIDE_AMBIENT);

        // Névoa: cor do céu, de FOG_START a FOG_END m da câmera (com a câmera
        // no túnel, os pontos dentro dele ficam sem névoa; veja o shader).
        glUniform3f(g_fog_color_uniform, SKY_COLOR.x, SKY_COLOR.y, SKY_COLOR.z);
        glUniform3f(g_fog_params_uniform, FOG_START, FOG_END, g_TunnelDimming);

        // Túneis: instâncias do módulo de abóbada (a cada 4 m) e dos dois
        // portais, mais as luminárias emissivas do teto. São desenhados ANTES
        // do chão, dos trilhos e dos demais objetos opacos: a abóbada, grande
        // e perto da câmera, enche o z-buffer primeiro (early-Z) e o fragment
        // shader (caro) deixa de rodar para o que fica atrás dela.
        ComputeTunnelPieces(g_Game.track, player.s, &tunnel_pieces);
        for (size_t i = 0; i < tunnel_pieces.size(); ++i)
            DrawTunnelPiece(g_Game.track, tunnel_pieces[i]);
        for (size_t i = 0; i < tunnel_lamps.size(); ++i)
            DrawTunnelLamp(g_Game.track, tunnel_lamps[i]);

        // Chão, dormentes e trilhos: faixas reconstruídas a partir da pista no
        // trecho visível em torno do jogador. As UV das faixas já estão em
        // "ladrilhos" de tamanho fixo em metros (sem esticar). O chão tem duas
        // texturas: brita no leito da pista (faixa 0) e terra nos lados.
        BuildGroundStrips(g_Game.track, player.s, &ground_batch);
        UpdateDynamicStripMesh(&g_TrackGround, ground_batch);
        DrawDynamicStripMesh(g_TrackGround, GROUND, g_Tex.gravel, UV_OBJ, 1.0f,
                             GRAVEL_TINT, 0, 1);
        DrawDynamicStripMesh(g_TrackGround, GROUND, g_Tex.terrain, UV_OBJ, 1.0f,
                             glm::vec3(1.0f, 1.0f, 1.0f), 1, -1);

        BuildSleeperStrips(g_Game.track, player.s, &sleepers_batch);
        UpdateDynamicStripMesh(&g_TrackSleepers, sleepers_batch);
        DrawDynamicStripMesh(g_TrackSleepers, SLEEPER, g_Tex.wood, UV_OBJ);

        BuildRailStrips(g_Game.track, player.s, &rails_batch);
        UpdateDynamicStripMesh(&g_TrackRails, rails_batch);
        DrawDynamicStripMesh(g_TrackRails, RAIL, g_Tex.metal, UV_OBJ);

        // Jogador: personagem articulado (veja DrawPlayer()).
        DrawPlayer(player, player_frame);

        // Prédios: instâncias dos modelos de prédio, um por posição.
        ComputeBuildings(g_Game.track, player.s, &buildings);
        for (size_t i = 0; i < buildings.size(); ++i)
            DrawBuilding(g_Game.track, buildings[i]);

        // Postes de luz: modelo do poste + cabeça emissiva.
        for (size_t i = 0; i < lamps.size(); ++i)
            DrawLamp(g_Game.track, lamps[i]);

        // Obstáculos: cada um é o seu modelo escalado para a caixa de colisão.
        for (size_t i = 0; i < g_Game.obstacles.size(); ++i)
            DrawObstacle(g_Game.obstacles[i]);

        // Moedas: instâncias do modelo da moeda, girando (ângulo acumulado por dt).
        for (size_t i = 0; i < g_Game.coins.size(); ++i)
            if (!g_Game.coins[i].collected)
                DrawCoin(g_Game.coins[i]);

        // Power-ups ainda não coletados: o modelo do tipo, flutuando e girando.
        for (size_t i = 0; i < g_Game.powerups.size(); ++i)
            if (!g_Game.powerups[i].collected)
                DrawPowerUp(g_Game.powerups[i]);

        // Porco voador do cofrinho, voando na curva de Bézier no céu.
        if (g_Game.pig.active)
            DrawFlyingPig(g_Game);

        // Partículas da coleta: por último entre os objetos 3D (translúcidas,
        // aditivas, sem escrever no z-buffer).
        DrawParticles(view);

        // Texto de depuração (tecla H): velocidade, faixa e quadros por segundo.
        TextRendering_ShowRunInfo(window);
        TextRendering_ShowFramesPerSecond(window);

        // GUI por cima de tudo: HUD da corrida, aviso da câmera livre e as
        // janelas de menu, pausa e fim de jogo (veja Gui_Frame()).
        Gui_Frame(window);

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

    // Finalizamos a GUI e o uso dos recursos do sistema operacional
    Gui_Shutdown();
    glfwTerminate();

    // Fim do programa
    return autotest ? Autotest_ExitCode() : 0;
}

// Retorna o tempo (s) decorrido desde o quadro anterior, limitado a [0, MAX_DT], e
// guarda o instante atual em *last para o próximo quadro.
float ComputeFrameDt(double now, double* last)
{
    float dt = (float)(now - *last);
    *last = now;
    return ClampFrameDt(dt); // NaN/negativo (relógio que recua) viram 0
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
void DrawObject(const char* name, const glm::mat4& model, int object_id, GLuint texture, int uv_mode,
                float uv_scale, const glm::vec3& tint)
{
    glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
    glUniform1i(g_object_id_uniform, object_id);
    glUniform1i(g_uv_mode_uniform, uv_mode);
    glUniform1f(g_uv_scale_uniform, uv_scale);
    ApplyMaterial(object_id, tint);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    DrawVirtualObject(name);
}

// Material de cada tipo de objeto: tom sobre a textura (kd_tint), especular
// (ks, q), emissivo e modo de sombreamento. Concentra aqui o "visual" de cada
// tipo, em vez de ramificar por object_id no shader. As cores vêm das
// texturas (kd_tint quase sempre branco); metais (trilhos, moedas, trem) têm
// Ks alto, chão, madeira e tecido quase não brilham.
Material MaterialFor(int object_id)
{
    const glm::vec3 white(1.0f, 1.0f, 1.0f);
    const glm::vec3 none(0.0f, 0.0f, 0.0f);
    Material m = { white, glm::vec3(0.3f), 16.0f, none, 0 }; // padrão
    switch (object_id)
    {
    case PLAYER:      m = { white, glm::vec3(0.12f), 16.0f, none, 0 }; break;  // roupa/pele: fosco
    case GROUND:      m = { white, glm::vec3(0.04f),  8.0f, none, 0 }; break;
    case RAIL:        m = { white, glm::vec3(0.90f), 96.0f, none, 0 }; break;
    case SLEEPER:     m = { glm::vec3(0.80f, 0.62f, 0.46f), glm::vec3(0.04f), 8.0f, none, 0 }; break; // madeira
    case BUILDING:    m = { white, glm::vec3(0.20f), 32.0f, none, 0 }; break;  // janelas com algum brilho
    case BARRIER_LOW:
    case BARRIER_HIGH:m = { white, glm::vec3(0.45f), 64.0f, none, 0 }; break;  // tinta esmaltada
    case SIGN:        m = { white, glm::vec3(0.35f), 64.0f, none, 0 }; break;
    case TRAIN:       m = { white, glm::vec3(0.55f), 64.0f, none, 0 }; break;  // lataria pintada
    case RAMP:        m = { white, glm::vec3(0.35f), 24.0f, none, 0 }; break;
    case TUNNEL:      m = { white, glm::vec3(0.10f), 12.0f, none, 0 }; break;  // tijolo: quase fosco
    case COIN:        m = { glm::vec3(1.25f, 1.1f, 0.85f), glm::vec3(0.90f), 32.0f, none, 1 }; break; // ouro; Gouraud
    // Power-ups: plástico brilhante com um leve brilho próprio (emissivo) na cor
    // do tipo, para se destacarem na pista (e no túnel escuro).
    case POWERUP_MAGNET:     m = { white, glm::vec3(0.50f), 48.0f, glm::vec3(0.22f, 0.04f, 0.04f), 0 }; break;
    case POWERUP_SNEAKERS:   m = { white, glm::vec3(0.50f), 48.0f, glm::vec3(0.04f, 0.12f, 0.22f), 0 }; break;
    case POWERUP_MULTIPLIER: m = { white, glm::vec3(0.50f), 48.0f, glm::vec3(0.22f, 0.17f, 0.03f), 0 }; break;
    case POWERUP_PIGGY:      m = { white, glm::vec3(0.50f), 48.0f, glm::vec3(0.22f, 0.07f, 0.10f), 0 }; break;
    case FLYING_PIG:  m = { white, glm::vec3(0.50f), 48.0f, none, 0 }; break;  // plástico brilhante
    case LAMP_POLE:   m = { white, glm::vec3(0.60f), 32.0f, none, 0 }; break;
    case LAMP:        m = { glm::vec3(0.10f), none, 1.0f, glm::vec3(1.5f, 1.15f, 0.65f), 0 }; break; // lâmpada acesa
    default: break;
    }
    return m;
}

void ApplyMaterial(int object_id, const glm::vec3& tint)
{
    Material m = MaterialFor(object_id);
    m.kd_tint = m.kd_tint * tint;
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

// Matriz de algo apoiado na pista: T(posição na pista + right*x + up*y) * R_y(heading + yaw).
static glm::mat4 TrackPlacement(const TrackFrame& f, float x, float y, float yaw)
{
    glm::vec4 p = f.position + f.right * x + glm::vec4(0.0f, y, 0.0f, 0.0f);
    return Matrix_Translate(p.x, p.y, p.z) * Matrix_Rotate_Y(f.heading + yaw);
}

// Personagem articulado: o corpo (tronco e cabeça) recebe a matriz do jogador
// na pista, e cada braço/perna é filho dele, girando em torno do ombro/quadril
// (topo da AABB da parte) com o ângulo da passada. Pilha de matrizes:
// PushMatrix(corpo) -> parte = corpo * T(pivô) * R_x(ângulo) * T(-pivô) -> PopMatrix.
void DrawPlayer(const Player& player, const TrackFrame& frame)
{
    static const char* const PARTS[6] = {"torso", "head", "leg-left", "leg-right", "arm-left", "arm-right"};

    // AABB do personagem inteiro (união das partes), em coordenadas do modelo.
    glm::vec3 mn(std::numeric_limits<float>::max());
    glm::vec3 mx(std::numeric_limits<float>::lowest());
    for (int i = 0; i < 6; ++i)
    {
        const SceneObject& o = FindSceneObject(PARTS[i]);
        mn = glm::min(mn, o.bbox_min);
        mx = glm::max(mx, o.bbox_max);
    }

    // Altura total PLAYER_HEIGHT com os pés em y = 0; rolando, é achatado na
    // vertical para PLAYER_ROLL_HEIGHT.
    const bool rolling = player.roll_timer > 0.0f;
    const float k = PLAYER_HEIGHT / (mx.y - mn.y);
    const float squash = rolling ? PLAYER_ROLL_HEIGHT / PLAYER_HEIGHT : 1.0f;
    glm::mat4 body = TrackPlacement(frame, player.x, player.y, PLAYER_MODEL_YAW)
                   * Matrix_Scale(k, k * squash, k)
                   * Matrix_Translate(-0.5f * (mn.x + mx.x), -mn.y, -0.5f * (mn.z + mx.z));

    // Ângulos (rad, em torno de X do modelo; negativo = para a frente).
    // Correndo: pernas e braços em oposição de fase. No ar: pose de salto.
    // Rolando: encolhido.
    float swing = std::sin(g_RunPhase);
    float angle[6] = {0.0f, 0.0f, LEG_SWING * swing, -LEG_SWING * swing,
                      -ARM_SWING * swing, ARM_SWING * swing};
    if (rolling)
    {
        angle[2] = angle[3] = -1.2f;
        angle[4] = angle[5] = -1.0f;
    }
    else if (!player.grounded)
    {
        angle[2] = -0.7f;
        angle[3] = 0.4f;
        angle[4] = angle[5] = -2.6f; // braços para cima
    }

    glm::mat4 model = body;
    for (int i = 0; i < 6; ++i)
    {
        const SceneObject& o = FindSceneObject(PARTS[i]);
        PushMatrix(model); // guarda a matriz do corpo
        if (angle[i] != 0.0f)
        {
            glm::vec3 pivot(0.5f * (o.bbox_min.x + o.bbox_max.x), o.bbox_max.y,
                            0.5f * (o.bbox_min.z + o.bbox_max.z));
            model = model * Matrix_Translate(pivot.x, pivot.y, pivot.z)
                          * Matrix_Rotate_X(angle[i])
                          * Matrix_Translate(-pivot.x, -pivot.y, -pivot.z);
        }
        DrawObject(PARTS[i], model, PLAYER, g_Tex.player, UV_OBJ);
        PopMatrix(model); // volta para a matriz do corpo
    }
}

// Desenha um obstáculo: o modelo do tipo escalado para a união das partes
// sólidas (ObstacleParts), isto é, a própria caixa de colisão (no trem, até o
// teto plano dos carros; veja TRAIN_MODEL_ROOF_Y). Os modelos
// procedurais já têm as medidas de OBSTACLE_SIZE (escala ~1). O trem são
// TRAIN_CARS carros em fila, cada um com o referencial da pista no seu centro
// (seguem a pista), com a cabine da frente voltada para o jogador.
void DrawObstacle(const Obstacle& o)
{
    AABB parts[3];
    int n = ObstacleParts(o, parts);
    AABB box = parts[0];
    for (int k = 1; k < n; ++k)
    {
        box.min = glm::min(box.min, parts[k].min);
        box.max = glm::max(box.max, parts[k].max);
    }
    const glm::vec3 size = box.max - box.min;
    const float cx = 0.5f * (box.min.x + box.max.x);
    const int id = OBSTACLE_OBJECT_ID[(int)o.type];

    if (o.type == ObstacleType::TRAIN)
    {
        // Largura pela AABB do carro do meio; as cabines, que têm retrovisores
        // um pouco para fora da caixa, são afinadas (escala lateral própria,
        // Tarefa 7.1a) até o ponto mais largo caber em size.x: assim nada
        // do trem sai da caixa de colisão e o jogador não atravessa um
        // retrovisor antes de morrer. Altura: o
        // teto PLANO do carro (TRAIN_MODEL_ROOF_Y) vai para o topo da caixa
        // de colisão, que é onde o jogador anda e onde a rampa termina
        // (Tarefa 4.4); só as nervuras do teto passam dele.
        const SceneObject& mid = FindSceneObject("train_middle");
        glm::vec3 msize = mid.bbox_max - mid.bbox_min;
        msize.y = TRAIN_MODEL_ROOF_Y - mid.bbox_min.y;
        const float car_len = size.z / TRAIN_CARS;
        for (int i = 0; i < TRAIN_CARS; ++i)
        {
            const SceneObject& car = FindSceneObject(TRAIN_CAR_MODELS[i]);
            const float cs = box.min.z + (i + 0.5f) * car_len;
            const float car_cz = 0.5f * (car.bbox_min.z + car.bbox_max.z);
            const float widest = std::max(std::fabs(car.bbox_min.x), std::fabs(car.bbox_max.x));
            const float sx = std::min(size.x / msize.x, 0.5f * size.x / widest);
            glm::mat4 model = TrackPlacement(g_Game.track.FrameAt(cs), cx, box.min.y, 0.0f)
                            * Matrix_Scale(sx, size.y / msize.y, car_len / msize.z)
                            * Matrix_Translate(0.0f, -car.bbox_min.y, -car_cz);
            DrawObject(TRAIN_CAR_MODELS[i], model, id, g_Tex.train, UV_OBJ);
        }
        return;
    }

    const float cs = 0.5f * (box.min.z + box.max.z);

    if (o.type == ObstacleType::RAMP)
    {
        // Rampa (Tarefa 4.4): dois objetos do mesmo OBJ, cada um com a sua
        // textura, escalados pela AABB de "ramp_frame" (a da rampa inteira),
        // para que as tábuas fiquem no lugar certo sobre a estrutura. O piso
        // do modelo é a mesma reta de RampHeightAt: os pés do jogador ficam
        // exatamente sobre as tábuas.
        const glm::vec3 msize = ObjectSize("ramp_frame");
        glm::mat4 model = TrackPlacement(g_Game.track.FrameAt(cs), cx, box.min.y, 0.0f)
                        * Matrix_OnGround("ramp_frame", size.x / msize.x, size.y / msize.y, size.z / msize.z);
        DrawObject("ramp_frame", model, id, g_Tex.metal, UV_OBJ);
        DrawObject("ramp_deck", model, id, g_Tex.wood, UV_OBJ, 1.0f, RAMP_DECK_TINT);
        return;
    }

    const char* name = "the_cube";
    if (o.type == ObstacleType::BARRIER_LOW)       name = "barrier_low";
    else if (o.type == ObstacleType::BARRIER_HIGH) name = "barrier_high";
    else if (o.type == ObstacleType::SIGN)         name = "sign_board";
    const glm::vec3 msize = ObjectSize(name);
    glm::mat4 model = TrackPlacement(g_Game.track.FrameAt(cs), cx, box.min.y, 0.0f)
                    * Matrix_OnGround(name, size.x / msize.x, size.y / msize.y, size.z / msize.z);
    DrawObject(name, model, id, g_Tex.atlas, UV_OBJ);
}

// Moeda de pé, centrada na altura c.y, girando em torno do eixo vertical. Uma
// moeda puxada pelo ímã (Tarefa 5.2) está em posição livre: CoinCenter.
void DrawCoin(const Coin& c)
{
    const float k = COIN_DRAW_DIAMETER / ObjectSize("coin").y;
    const glm::vec3 center = CoinCenter(c);
    glm::mat4 model = TrackPlacement(g_Game.track.FrameAt(center.z), center.x,
                                     center.y - 0.5f * COIN_DRAW_DIAMETER, g_CoinSpin)
                    * Matrix_OnGround("coin", k, k, k);
    DrawObject("coin", model, COIN, g_Tex.atlas, UV_OBJ);
}

// Prédio com a fachada (+Z do modelo) voltada para a pista, escala uniforme
// para caber em BUILDING_WIDTH x BUILDING_WIDTH (a altura vem do modelo).
void DrawBuilding(const Track& track, const BuildingInstance& b)
{
    const char* name = BUILDING_MODELS[b.variant];
    const glm::vec3 ms = ObjectSize(name);
    const float k = BUILDING_WIDTH / std::max(ms.x, ms.z);
    const float yaw = (b.x < 0.0f) ? 1.5707963f : -1.5707963f; // +Z do modelo -> centro da pista
    glm::mat4 model = TrackPlacement(track.FrameAt(b.s), b.x, 0.0f, yaw)
                    * Matrix_OnGround(name, k, k, k);
    DrawObject(name, model, BUILDING, g_Tex.city, UV_OBJ, 1.0f, BUILDING_TINTS[b.tint]);
}

// Rotação do poste em torno de Y: o braço (-Z do modelo) aponta para o centro
// da pista.
static float LampYaw(const LampInstance& l)
{
    return (l.x < 0.0f) ? -1.5707963f : 1.5707963f;
}

// Centro da cabeça do poste em coordenadas de mundo: a base do poste no chão
// da pista + a cabeça do modelo escalada (k = LAMP_HEIGHT / altura da cabeça).
// É a mesma conta da matriz de DrawLamp, então luz e malha coincidem.
glm::vec4 LampHeadPosition(const Track& track, const LampInstance& l)
{
    const float k = LAMP_HEIGHT / LAMP_MODEL_HEAD.y;
    const glm::mat4 model = TrackPlacement(track.FrameAt(l.s), l.x, 0.0f, LampYaw(l))
                          * Matrix_Scale(k, k, k);
    return model * glm::vec4(LAMP_MODEL_HEAD.x, LAMP_MODEL_HEAD.y, LAMP_MODEL_HEAD.z, 1.0f);
}

// Poste (haste em x = z = 0 do modelo, base em y = 0) e, na cabeça, o bloco
// emissivo LAMP do tamanho da luminária.
void DrawLamp(const Track& track, const LampInstance& l)
{
    const float k = LAMP_HEIGHT / LAMP_MODEL_HEAD.y;
    const TrackFrame f = track.FrameAt(l.s);
    glm::mat4 pole = TrackPlacement(f, l.x, 0.0f, LampYaw(l)) * Matrix_Scale(k, k, k);
    DrawObject("lamp_post", pole, LAMP_POLE, g_Tex.roads, UV_OBJ);

    const glm::vec4 h = LampHeadPosition(track, l);
    const glm::vec3 cube = ObjectSize("the_cube");
    glm::mat4 head = Matrix_Translate(h.x, h.y - 0.5f * LAMP_HEAD_HEIGHT, h.z)
                   * Matrix_Rotate_Y(f.heading)
                   * Matrix_OnGround("the_cube", LAMP_HEAD_SIZE / cube.x, LAMP_HEAD_HEIGHT / cube.y,
                                     LAMP_HEAD_SIZE / cube.z);
    DrawObject("the_cube", head, LAMP, g_Tex.metal, UV_BOX, 0.5f);
}

// Peça do túnel na pista, nas coordenadas do próprio modelo (sem recentrar):
// o módulo está centrado em z = 0 e o portal tem o corpo centrado em z = 0 e
// a frente em +Z (voltada para quem chega). O portal de saída é girado 180°
// (frente para quem sai). O portal tem um tom de pedra mais clara que a
// abóbada, para destacar a entrada.
void DrawTunnelPiece(const Track& track, const TunnelPiece& piece)
{
    const TrackFrame f = track.FrameAt(piece.s);
    if (piece.kind == TUNNEL_PIECE_SECTION)
    {
        DrawObject("tunnel_section", TrackPlacement(f, 0.0f, 0.0f, 0.0f), TUNNEL, g_Tex.brick, UV_OBJ);
        return;
    }
    const float yaw = (piece.kind == TUNNEL_PIECE_EXIT) ? 3.1415927f : 0.0f;
    DrawObject("tunnel_portal", TrackPlacement(f, 0.0f, 0.0f, yaw), TUNNEL, g_Tex.brick, UV_OBJ,
               1.0f, glm::vec3(0.95f, 0.85f, 0.80f));
}

// Luminária do teto do túnel em s: bloco emissivo no centro da pista, com a
// face de cima rente ao teto (TUNNEL_LAMP_BOX_TOP); a luz pontual
// correspondente fica logo abaixo, em TUNNEL_LAMP_HEIGHT.
void DrawTunnelLamp(const Track& track, float s)
{
    const TrackFrame f = track.FrameAt(s);
    const glm::vec3 cube = ObjectSize("the_cube");
    glm::mat4 model = TrackPlacement(f, 0.0f, TUNNEL_LAMP_BOX_TOP - TUNNEL_LAMP_BOX_HEIGHT, 0.0f)
                    * Matrix_OnGround("the_cube", TUNNEL_LAMP_BOX_WIDTH / cube.x,
                                      TUNNEL_LAMP_BOX_HEIGHT / cube.y, TUNNEL_LAMP_BOX_LENGTH / cube.z);
    DrawObject("the_cube", model, LAMP, g_Tex.metal, UV_BOX, 0.5f);
}

// Power-up na pista: o modelo do tipo, com o centro na altura POWERUP_HEIGHT
// (mais a flutuação), girando. Cada tipo tem seu objeto (e material) próprio.
void DrawPowerUp(const PowerUp& p)
{
    static const char* const NAMES[POWERUP_TYPE_COUNT] = {"powerup_magnet", "powerup_sneaker",
                                                           "powerup_multiplier", "piggy_bank"};
    static const int IDS[POWERUP_TYPE_COUNT] = {POWERUP_MAGNET, POWERUP_SNEAKERS, POWERUP_MULTIPLIER,
                                                POWERUP_PIGGY};
    const char* name = NAMES[(int)p.type];
    const glm::vec3 size = ObjectSize(name);
    const float k = POWERUP_DRAW_SIZE / std::max(size.x, std::max(size.y, size.z));
    const float bob = POWERUP_BOB_HEIGHT * std::sin(g_PowerUpBob + 0.7f * p.s); // fase por power-up
    glm::mat4 model = TrackPlacement(g_Game.track.FrameAt(p.s), p.lane * LANE_WIDTH,
                                     POWERUP_HEIGHT + bob - 0.5f * k * size.y, g_PowerUpSpin)
                    * Matrix_OnGround(name, k, k, k);
    DrawObject(name, model, IDS[(int)p.type], g_Tex.atlas, UV_OBJ);
}

// Porco voador: posicionado em (x, y, s) pela curva de Bézier (relativa ao
// jogador) e orientado pela velocidade, no referencial da pista no s dele:
// guinada para onde ele vai e arfagem pela componente vertical. O modelo tem a
// cabeça em +Z; no referencial da pista "para frente" é -Z do modelo, por isso
// o meia-volta (pi). As asas batem em torno da raiz com a fase em função de t
// (que anda com dt), uma para cada lado.
void DrawFlyingPig(const Game& game)
{
    const glm::vec3 pos = game.PigTrackPosition();
    const glm::vec3 vel = game.PigTrackVelocity();
    const float yaw = std::atan2(-vel.x, vel.z);                       // relativo à pista
    const float pitch = std::atan2(vel.y, std::sqrt(vel.x * vel.x + vel.z * vel.z));

    const SceneObject& body = FindSceneObject("flying_pig_body");
    const glm::vec3 c = 0.5f * (body.bbox_min + body.bbox_max);
    const float k = PIG_DRAW_LENGTH / (body.bbox_max.z - body.bbox_min.z);

    const glm::mat4 base = TrackPlacement(game.track.FrameAt(pos.z), pos.x, pos.y, yaw + 3.1415927f)
                         * Matrix_Rotate_X(-pitch) * Matrix_Scale(k, k, k) * Matrix_Translate(-c.x, -c.y, -c.z);
    DrawObject("flying_pig_body", base, FLYING_PIG, g_Tex.atlas, UV_OBJ);

    const float flap = PIG_FLAP_AMPLITUDE *
                       std::sin(6.2831853f * PIG_FLAP_FREQ * game.pig.t * PIGGY_FLIGHT_TIME);
    const char* const names[2] = {"flying_pig_wing_left", "flying_pig_wing_right"};
    for (int i = 0; i < 2; ++i)
    {
        const float side = (i == 0) ? -1.0f : 1.0f;
        const float rx = side * PIG_WING_ROOT_X;
        const glm::mat4 wing = base * Matrix_Translate(rx, PIG_WING_ROOT_Y, PIG_WING_ROOT_Z)
                             * Matrix_Rotate_Z(side * flap)
                             * Matrix_Translate(-rx, -PIG_WING_ROOT_Y, -PIG_WING_ROOT_Z);
        DrawObject(names[i], wing, FLYING_PIG, g_Tex.atlas, UV_OBJ);
    }
}

// Quad unitário [-0.5, 0.5]^2 no plano XY, normal +Z, UV em [0,1]^2, como
// GL_TRIANGLE_STRIP de 4 vértices (CCW visto de +Z). Mesmos "locations" de
// shader_vertex.glsl (0 = posição, 1 = normal, 2 = UV).
void CreateParticleQuad()
{
    const float positions[16] = {-0.5f, -0.5f, 0.0f, 1.0f,   0.5f, -0.5f, 0.0f, 1.0f,
                                 -0.5f,  0.5f, 0.0f, 1.0f,   0.5f,  0.5f, 0.0f, 1.0f};
    const float normals[16]   = {0.0f, 0.0f, 1.0f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f};
    const float uvs[8]        = {0.0f, 0.0f,   1.0f, 0.0f,   0.0f, 1.0f,   1.0f, 1.0f};
    const float* data[3]      = {positions, normals, uvs};
    const GLint  dims[3]      = {4, 4, 2};

    glGenVertexArrays(1, &g_ParticleQuadVAO);
    glBindVertexArray(g_ParticleQuadVAO);
    for (GLuint i = 0; i < 3; ++i)
    {
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, 4 * dims[i] * sizeof(float), data[i], GL_STATIC_DRAW);
        glVertexAttribPointer(i, dims[i], GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(i);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Cor das faíscas de cada power-up (brilhantes, para o blending aditivo).
glm::vec3 PowerUpParticleColor(PowerUpType type)
{
    switch (type)
    {
    case PowerUpType::MAGNET:     return glm::vec3(1.00f, 0.30f, 0.25f); // vermelho
    case PowerUpType::SNEAKERS:   return glm::vec3(0.30f, 0.65f, 1.00f); // azul
    case PowerUpType::MULTIPLIER: return glm::vec3(0.45f, 1.00f, 0.40f); // verde
    case PowerUpType::PIGGY:      return glm::vec3(1.00f, 0.50f, 0.75f); // rosa
    }
    return glm::vec3(1.0f);
}

// Consome os eventos de coleta do Game (posição em coordenadas de pista:
// x lateral, y altura, z = s): converte cada um para o MUNDO com o referencial
// da pista em s e dispara a explosão (moeda: PARTICLES_PER_COIN douradas;
// power-up: PARTICLES_PER_POWERUP na cor do tipo). As partículas ficam onde
// nasceram (não acompanham o jogador) e quicam no chão da pista (f.position.y);
// sobre um trem, as que caem no teto somem dentro dele (z-buffer) e as que
// saem pela lateral caem até o chão. Esvazia a lista, sem alocar.
void EmitCollectParticles(Game& game)
{
    std::vector<CollectEvent>& events = game.events;
    for (size_t i = 0; i < events.size(); ++i)
    {
        const CollectEvent& e = events[i];
        const TrackFrame f = game.track.FrameAt(e.position.z);
        const glm::vec4 world = f.position + f.right * e.position.x
                              + glm::vec4(0.0f, e.position.y, 0.0f, 0.0f);
        if (e.kind == CollectKind::COIN)
            g_Particles.Emit(world, PARTICLES_PER_COIN, PARTICLE_COIN_COLOR, g_ParticleSeed++, f.position.y);
        else
            g_Particles.Emit(world, PARTICLES_PER_POWERUP, PowerUpParticleColor(e.powerup), g_ParticleSeed++,
                             f.position.y);
    }
    events.clear();
}

// Desenha as partículas vivas como billboards: o quad unitário com a matriz de
// modelagem montada A MAO (sem glm::rotate/lookAt) a partir dos eixos da câmera.
// As linhas da matriz de visão são os eixos da câmera em coordenadas de mundo
// (view[coluna][linha]): linha 0 = direita, linha 1 = cima, linha 2 = "trás"
// (de onde a câmera olha, ou seja, a normal do quad voltada para ela). A matriz
// tem as colunas (direita*size, cima*size, trás, posição). Tamanho e alfa
// caem com life/max_life. Blending aditivo (GL_SRC_ALPHA, GL_ONE): sobreposição
// soma brilho e a ordem de desenho não importa (não precisa ordenar). O
// z-buffer é lido (partículas atrás de prédios ficam escondidas) mas não
// escrito, e o culling é desligado. Tudo é restaurado ao fim.
void DrawParticles(const glm::mat4& view)
{
    const std::vector<Particle>& alive = g_Particles.Alive();
    if (alive.empty())
        return;

    const glm::vec4 right(view[0][0], view[1][0], view[2][0], 0.0f);
    const glm::vec4 up(view[0][1], view[1][1], view[2][1], 0.0f);
    const glm::vec4 back(view[0][2], view[1][2], view[2][2], 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glUniform1i(g_object_id_uniform, PARTICLE);
    glUniform1i(g_uv_mode_uniform, UV_OBJ);
    glUniform1f(g_uv_scale_uniform, 1.0f);
    glUniform1i(g_shading_mode_uniform, 0);
    glBindVertexArray(g_ParticleQuadVAO);
    for (size_t i = 0; i < alive.size(); ++i)
    {
        const Particle& p = alive[i];
        const float fade = std::max(p.life / p.max_life, 0.0f);   // 1 -> 0 ao longo da vida
        const float size = p.size * fade;
        glm::mat4 model(1.0f);
        model[0] = right * size;
        model[1] = up * size;
        model[2] = back;
        model[3] = p.position;
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(g_material_kd_tint_uniform, p.color.x, p.color.y, p.color.z);
        glUniform1f(g_particle_alpha_uniform, fade);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// Recomeça a corrida do zero e reposiciona a câmera atrás do jogador (sem a
// transição suave, que faria a câmera "voar" de volta pela pista).
void RestartRun()
{
    g_Game.Reset();
    g_Particles.Clear();
    g_Camera.Snap(g_Game.track, g_Game.player.s, g_Game.player.x,
                  g_Game.track.IsTunnelAt(g_Game.player.s), g_Game.CameraTargetHeight());
    g_TunnelDimming = g_Game.track.IsTunnelAt(g_Game.player.s - g_Camera.Back()) ? 1.0f : 0.0f;
}

// ---------------------------------------------------------------------------
// Estados da corrida usados pela GUI e pelos atalhos de teclado
// ---------------------------------------------------------------------------

// PLAYING -> FREE_CAMERA: a câmera livre começa exatamente onde a 3ª pessoa
// estava; o cursor é capturado para o mouse girar o olhar sem limite.
void EnterFreeCamera(GLFWwindow* window)
{
    g_Game.mode = GameMode::FREE_CAMERA;
    g_FreeCamera.StartAt(g_Camera.Pose());
    g_CursorPointValid = false;
    if (!Autotest_Active()) // no E2E o cursor do desktop fica quieto
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

// FREE_CAMERA -> PLAYING: volta à corrida de onde parou, com a 3ª pessoa de
// novo no lugar certo (sem suavizar desde a posição da câmera livre), e
// devolve o cursor normal.
void LeaveFreeCamera(GLFWwindow* window)
{
    g_Game.mode = GameMode::PLAYING;
    g_Camera.Snap(g_Game.track, g_Game.player.s, g_Game.player.x,
                  g_Game.track.IsTunnelAt(g_Game.player.s), g_Game.CameraTargetHeight());
    if (!Autotest_Active())
    {
        // Desfaz exatamente o que EnterFreeCamera() fez (movimento bruto e captura).
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void StartRunFromMenu()
{
    RestartRun(); // corrida nova, em PLAYING
}

void PauseRun(GLFWwindow* window)
{
    if (g_Game.mode == GameMode::FREE_CAMERA)
        LeaveFreeCamera(window); // devolve o cursor para usar os botões
    if (g_Game.mode == GameMode::PLAYING)
        g_Game.mode = GameMode::PAUSED;
}

void ResumeRun()
{
    if (g_Game.mode == GameMode::PAUSED)
        g_Game.mode = GameMode::PLAYING;
}

// Lista de controles aberta na janela do menu (botão "Controles")?
bool g_GuiShowControls = false;

void GoToMenu()
{
    RestartRun(); // ao fundo do menu fica a largada de uma corrida nova
    g_Game.mode = GameMode::MENU;
    g_GuiShowControls = false;
}

// ---------------------------------------------------------------------------
// GUI (Dear ImGui)
// ---------------------------------------------------------------------------

// Retângulo (coordenadas de janela do ImGui) de cada botão desenhado no
// último quadro, pelo rótulo. Usado pelo harness E2E para clicar no botão.
struct GuiRect { float x0, y0, x1, y1; };
std::map<std::string, GuiRect> g_GuiButtons;
// Clique pendente do E2E: 1 = apertar no próximo quadro, 2 = soltar, 0 = nada.
int     g_GuiClickPhase = 0;
GuiRect g_GuiClickRect;

// Estilo de referência (tamanhos para uma janela de 600 pixels de altura) e
// a escala aplicada a ele no momento (acompanha a altura da janela).
ImGuiStyle g_GuiBaseStyle;
float      g_GuiScale = 0.0f;

// Cores da GUI.
const ImVec4 GUI_ACCENT    = ImVec4(1.00f, 0.62f, 0.15f, 1.00f); // laranja (título, botões)
const ImVec4 GUI_GOLD      = ImVec4(1.00f, 0.84f, 0.25f, 1.00f); // moedas
const ImVec4 GUI_DANGER    = ImVec4(1.00f, 0.32f, 0.25f, 1.00f); // GAME OVER
const ImVec4 GUI_TEXT_DIM  = ImVec4(0.72f, 0.75f, 0.82f, 1.00f); // dicas e rótulos

void Gui_Init(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL; // não grava imgui.ini ao lado do executável
    io.LogFilename = NULL;

    // Fonte vetorial embutida no ImGui: continua nítida quando a GUI é escalada.
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = 18.0f;
    io.Fonts->AddFontDefaultVector();

    ImGui::StyleColorsDark();
    style.WindowRounding    = 12.0f;
    style.FrameRounding     = 8.0f;
    style.ChildRounding     = 8.0f;
    style.WindowBorderSize  = 0.0f;
    style.FrameBorderSize   = 0.0f;
    style.WindowPadding     = ImVec2(22.0f, 18.0f);
    style.FramePadding      = ImVec2(12.0f, 6.0f);
    style.ItemSpacing       = ImVec2(10.0f, 9.0f);
    style.CellPadding       = ImVec2(8.0f, 3.0f);
    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]         = ImVec4(0.07f, 0.08f, 0.12f, 0.92f);
    c[ImGuiCol_Text]             = ImVec4(0.96f, 0.96f, 0.98f, 1.00f);
    c[ImGuiCol_Separator]        = ImVec4(1.00f, 0.62f, 0.15f, 0.45f);
    c[ImGuiCol_Button]           = ImVec4(0.90f, 0.50f, 0.10f, 1.00f);
    c[ImGuiCol_ButtonHovered]    = ImVec4(1.00f, 0.64f, 0.22f, 1.00f);
    c[ImGuiCol_ButtonActive]     = ImVec4(0.78f, 0.40f, 0.06f, 1.00f);
    c[ImGuiCol_FrameBg]          = ImVec4(0.18f, 0.20f, 0.26f, 0.90f);
    c[ImGuiCol_PlotHistogram]    = GUI_ACCENT;
    c[ImGuiCol_TableRowBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]    = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
    c[ImGuiCol_TableBorderLight] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    g_GuiBaseStyle = style;

    // Backends GLFW e OpenGL 3.3. Com install_callbacks = true o ImGui
    // instala os callbacks dele e encadeia os que já registramos (os nossos
    // rodam primeiro), por isso esta chamada vem depois dos glfwSet*Callback.
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void Gui_Shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

bool Gui_ClickButton(const char* label)
{
    std::map<std::string, GuiRect>::const_iterator it = g_GuiButtons.find(label);
    if (it == g_GuiButtons.end() || !Gui_ButtonOnScreen(label))
        return false;
    g_GuiClickRect  = it->second;
    g_GuiClickPhase = 1;
    return true;
}

// Clique do E2E: posição + botão esquerdo entram na fila de eventos do ImGui
// DEPOIS da posição real do cursor que o backend GLFW acabou de enfileirar
// (por isso é chamado entre ImGui_ImplGlfw_NewFrame() e ImGui::NewFrame()).
// Apertar e soltar vão em quadros seguidos, como um clique de verdade.
static void Gui_InjectPendingClick()
{
    if (g_GuiClickPhase == 0)
        return;
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(0.5f * (g_GuiClickRect.x0 + g_GuiClickRect.x1),
                        0.5f * (g_GuiClickRect.y0 + g_GuiClickRect.y1));
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, g_GuiClickPhase == 1);
    g_GuiClickPhase = (g_GuiClickPhase == 1) ? 2 : 0;
}

// Botão da largura da janela; guarda o retângulo dele para o E2E (cortado
// pela área visível da janela, se ela estiver rolada).
static bool GuiButton(const char* label)
{
    const bool pressed = ImGui::Button(label, ImVec2(-FLT_MIN, 1.9f * ImGui::GetFontSize()));
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    const ImVec2 wa = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    GuiRect r = {a.x, a.y, b.x, b.y};
    // Parte do botão escondida pela rolagem da janela: fora da área visível.
    if (a.y < wa.y || b.y > wa.y + ws.y)
        r.y0 = r.y1 = -1.0f;
    g_GuiButtons[label] = r;
    return pressed;
}

bool Gui_ButtonOnScreen(const char* label)
{
    std::map<std::string, GuiRect>::const_iterator it = g_GuiButtons.find(label);
    if (it == g_GuiButtons.end())
        return false;
    const GuiRect& r = it->second;
    const ImVec2 d = ImGui::GetIO().DisplaySize;
    return r.y1 > r.y0 && r.x0 >= 0.0f && r.y0 >= 0.0f && r.x1 <= d.x && r.y1 <= d.y;
}

// Texto centralizado na largura disponível da janela.
static void GuiTextCentered(const char* text, const ImVec4& color)
{
    const float w = ImGui::CalcTextSize(text).x;
    const float avail = ImGui::GetContentRegionAvail().x;
    if (avail > w)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 0.5f * (avail - w));
    ImGui::TextColored(color, "%s", text);
}

// Título grande e centralizado.
static void GuiTitle(const char* text, const ImVec4& color, float size_factor)
{
    ImGui::PushFont(NULL, ImGui::GetStyle().FontSizeBase * size_factor);
    GuiTextCentered(text, color);
    ImGui::PopFont();
}

// Começa uma janela centralizada na tela, sem barra de título, com largura
// mínima de 'min_width_em' vezes o tamanho da fonte. Nunca passa do tamanho da
// tela (Tarefa 7.1b): se o conteúdo não couber mesmo com a escala mínima da GUI
// (janela muito baixa), aparece uma barra de rolagem (e a roda do mouse rola).
static void GuiBeginCentered(const char* id, float min_width_em)
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 0.5f * vp->Size.x, vp->Pos.y + 0.5f * vp->Size.y),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(min_width_em * ImGui::GetFontSize(), vp->Size.x), 0.0f),
                                        ImVec2(vp->Size.x, vp->Size.y));
    ImGui::Begin(id, NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoNav);
}

// Escurece a cena 3D atrás das janelas de menu, pausa e fim de jogo.
static void GuiDimBackground()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), IM_COL32(5, 8, 20, 120));
}

// Lista de controles (botão "Controles" do menu).
static void GuiControlsTable()
{
    static const char* const ROWS[][2] = {
        {"Esquerda/Direita, A/D", "trocar de faixa"},
        {"Cima, W ou Espaço",     "pular"},
        {"Baixo ou S",            "rolar"},
        {"P ou Esc",              "pausar / continuar"},
        {"C",                     "câmera livre"},
        {"",                      "(WASD, Espaço, Shift, mouse)"},
        {"H",                     "texto de depuração (FPS)"},
        {"R",                     "recarregar os shaders"},
    };
    if (ImGui::BeginTable("controles", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        for (size_t i = 0; i < sizeof(ROWS) / sizeof(ROWS[0]); ++i)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(GUI_ACCENT, "%s", ROWS[i][0]);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(ROWS[i][1]);
        }
        ImGui::EndTable();
    }
}

static void Gui_Menu(GLFWwindow* window)
{
    GuiDimBackground();
    GuiBeginCentered("##menu", 17.0f);
    GuiTitle("RAIL RUSH", GUI_ACCENT, 3.0f);
    GuiTextCentered("Corra pelos trilhos, desvie dos trens", GUI_TEXT_DIM);
    GuiTextCentered("e junte moedas!", GUI_TEXT_DIM);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (GuiButton("Jogar"))
        StartRunFromMenu();
    if (GuiButton("Controles"))
        g_GuiShowControls = !g_GuiShowControls;
    if (g_GuiShowControls)
        GuiControlsTable();
    if (GuiButton("Sair"))
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    ImGui::Spacing();
    GuiTextCentered("Enter: jogar", GUI_TEXT_DIM);
    ImGui::End();
}

static void Gui_Pause()
{
    GuiDimBackground();
    GuiBeginCentered("##pausa", 14.0f);
    GuiTitle("PAUSADO", GUI_ACCENT, 2.2f);
    ImGui::Spacing();
    if (GuiButton("Continuar"))
        ResumeRun();
    if (GuiButton("Reiniciar"))
        RestartRun();
    if (GuiButton("Menu"))
        GoToMenu();
    ImGui::Spacing();
    GuiTextCentered("P ou Esc: continuar", GUI_TEXT_DIM);
    ImGui::End();
}

static void Gui_GameOver()
{
    GuiDimBackground();
    GuiBeginCentered("##fim", 16.0f);
    GuiTitle("GAME OVER", GUI_DANGER, 2.6f);
    ImGui::Spacing();
    if (ImGui::BeginTable("placar", 2, ImGuiTableFlags_SizingStretchSame))
    {
        char value[32];
        const char* labels[3] = {"Pontos", "Moedas", "Distância"};
        for (int i = 0; i < 3; ++i)
        {
            if (i == 0)
                snprintf(value, sizeof(value), "%d", (int)g_Game.score);
            else if (i == 1)
                snprintf(value, sizeof(value), "%d", g_Game.coins_collected);
            else
                snprintf(value, sizeof(value), "%.0f m", g_Game.player.s);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(GUI_TEXT_DIM, "%s", labels[i]);
            ImGui::TableSetColumnIndex(1);
            // Valor alinhado à direita da coluna.
            const float w = ImGui::CalcTextSize(value).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - w);
            ImGui::TextColored(i == 1 ? GUI_GOLD : ImVec4(1, 1, 1, 1), "%s", value);
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if (GuiButton("Jogar de novo"))
        RestartRun();
    if (GuiButton("Menu"))
        GoToMenu();
    ImGui::Spacing();
    GuiTextCentered("Enter ou Espaço: jogar de novo", GUI_TEXT_DIM);
    ImGui::End();
}

// HUD da corrida, no canto superior esquerdo: pontos, moedas, distância e os
// power-ups ativos com uma barra do tempo restante. Não recebe mouse/teclado.
static void Gui_Hud()
{
    const float pad = 10.0f * g_GuiScale;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + pad, vp->Pos.y + pad), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.45f);
    ImGui::Begin("##hud", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    // Pontos em fonte grande, com o rótulo pequeno alinhado pela base.
    const float y0 = ImGui::GetCursorPosY();
    ImGui::PushFont(NULL, ImGui::GetStyle().FontSizeBase * 1.7f);
    ImGui::Text("%d", (int)g_Game.score);
    const float big_h = ImGui::GetTextLineHeight();
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::SetCursorPosY(y0 + 0.92f * (big_h - ImGui::GetTextLineHeight()));
    ImGui::TextColored(GUI_TEXT_DIM, "pontos");
    ImGui::TextColored(GUI_GOLD, "Moedas: %d", g_Game.coins_collected);
    ImGui::Text("Distância: %.0f m", g_Game.player.s);

    // Power-ups ativos (Tarefa 5.2): nome, barra do tempo restante e segundos.
    static const char* const NAMES[3] = {"Ímã", "Tênis", "Pontos x2"};
    static const ImVec4 COLORS[3] = {ImVec4(1.00f, 0.45f, 0.40f, 1.0f),
                                     ImVec4(0.45f, 0.80f, 1.00f, 1.0f),
                                     ImVec4(1.00f, 0.85f, 0.30f, 1.0f)};
    const float bar_x = ImGui::GetCursorPosX() + ImGui::CalcTextSize("Pontos x2").x +
                        ImGui::GetStyle().ItemSpacing.x;
    for (int t = 0; t < 3; ++t)
    {
        const float left = g_Game.powerup_timer[t];
        if (left <= 0.0f)
            continue;
        // FramePadding.y = 0 na linha: a barra (da altura do texto) e os
        // textos ficam alinhados pela mesma base.
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
        ImGui::TextColored(COLORS[t], "%s", NAMES[t]);
        ImGui::SameLine(bar_x);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, COLORS[t]);
        ImGui::ProgressBar(std::min(1.0f, left / POWERUP_DURATION),
                           ImVec2(6.0f * ImGui::GetFontSize(), ImGui::GetTextLineHeight()), "");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%2d s", (int)std::ceil(left));
        ImGui::PopStyleVar();
    }
    ImGui::End();
}

// Câmera livre: aviso no topo da tela, centralizado.
static void Gui_FreeCameraHint()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 0.5f * vp->Size.x, vp->Pos.y + 10.0f * g_GuiScale),
                            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##camera_livre", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    GuiTitle("CÂMERA LIVRE", GUI_ACCENT, 1.4f);
    GuiTextCentered("WASD, Espaço e Shift movem  |  mouse olha", ImVec4(1, 1, 1, 1));
    GuiTextCentered("C volta ao jogo  |  P ou Esc pausa", GUI_TEXT_DIM);
    ImGui::End();
}

void Gui_Frame(GLFWwindow* window)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    Gui_InjectPendingClick();

    // Escala da GUI pela altura da janela (referência: 600 pixels), entre
    // GUI_MIN_SCALE e 2x: numa janela mais baixa que 600 px a GUI encolhe junto
    // e a janela "Controles" continua cabendo (Tarefa 7.1b); abaixo de
    // 600 * GUI_MIN_SCALE px as janelas ganham rolagem (GuiBeginCentered).
    const float GUI_MIN_SCALE = 0.5f;
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    const float scale = std::max(GUI_MIN_SCALE, std::min(2.0f, (float)height / 600.0f));
    if (scale != g_GuiScale)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style = g_GuiBaseStyle;
        style.ScaleAllSizes(scale);
        style.FontScaleMain = scale;
        g_GuiScale = scale;
    }

    ImGui::NewFrame();
    g_GuiButtons.clear();

    switch (g_Game.mode)
    {
    case GameMode::MENU:        Gui_Menu(window);       break;
    case GameMode::PLAYING:     Gui_Hud();              break;
    case GameMode::PAUSED:      Gui_Hud(); Gui_Pause(); break;
    case GameMode::FREE_CAMERA: Gui_FreeCameraHint();   break;
    case GameMode::GAME_OVER:   Gui_GameOver();         break;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
void DrawDynamicStripMesh(const DynamicStripMesh& mesh, int object_id, GLuint texture, int uv_mode,
                          float uv_scale, const glm::vec3& tint, int first_strip, int strip_count)
{
    const int total = (int)mesh.first.size();
    if (strip_count < 0 || first_strip + strip_count > total)
        strip_count = total - first_strip;
    if (first_strip < 0 || strip_count <= 0)
        return;

    glm::mat4 model = Matrix_Identity();
    glUniformMatrix4fv(g_model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
    glUniform1i(g_object_id_uniform, object_id);
    glUniform1i(g_uv_mode_uniform, uv_mode);
    glUniform1f(g_uv_scale_uniform, uv_scale);
    glUniform4f(g_bbox_min_uniform, 0.0f, 0.0f, 0.0f, 1.0f);
    glUniform4f(g_bbox_max_uniform, 0.0f, 0.0f, 0.0f, 1.0f);
    ApplyMaterial(object_id, tint);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Todas as faixas pedidas numa única chamada (os dormentes são milhares de
    // faixas de 4 vértices).
    glBindVertexArray(mesh.vertex_array_object_id);
    glMultiDrawArrays(GL_TRIANGLE_STRIP, &mesh.first[first_strip], &mesh.count[first_strip], strip_count);
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
    g_particle_alpha_uniform       = glGetUniformLocation(g_GpuProgramID, "particle_alpha");
    g_num_tunnels_uniform          = glGetUniformLocation(g_GpuProgramID, "num_tunnels");
    g_tunnel_start_uniform         = glGetUniformLocation(g_GpuProgramID, "tunnel_start");
    g_tunnel_forward_uniform       = glGetUniformLocation(g_GpuProgramID, "tunnel_forward");
    g_tunnel_shape_uniform         = glGetUniformLocation(g_GpuProgramID, "tunnel_shape");
    g_fog_color_uniform            = glGetUniformLocation(g_GpuProgramID, "fog_color");
    g_fog_params_uniform           = glGetUniformLocation(g_GpuProgramID, "fog_params");

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

// Carrega um OBJ para g_VirtualScene: leitura (tinyobjloader), normais por
// vértice se o arquivo não as tiver, e VAO/VBOs. Veja
// BuildTrianglesAndAddToVirtualScene() sobre merge_as.
void LoadObjToScene(const char* filename, const char* merge_as, float max_y)
{
    ObjModel model(filename);
    for (size_t i = 1; i < model.attrib.vertices.size(); i += 3)
        model.attrib.vertices[i] = std::min(model.attrib.vertices[i], max_y);
    ComputeNormals(&model);
    BuildTrianglesAndAddToVirtualScene(&model, merge_as);
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel* model, const char* merge_as)
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

        if (merge_as != NULL)
        {
            // Todos os shapes do arquivo num só objeto: os índices de cada
            // shape são acrescentados em sequência, então o objeto é o
            // intervalo [primeiro índice do 1º shape, último índice] e a AABB
            // é a união das AABBs.
            theobject.name = merge_as;
            if (shape > 0)
            {
                const SceneObject& prev = g_VirtualScene[merge_as];
                theobject.first_index = prev.first_index;
                theobject.num_indices = last_index - prev.first_index + 1;
                theobject.bbox_min = glm::min(prev.bbox_min, bbox_min);
                theobject.bbox_max = glm::max(prev.bbox_max, bbox_max);
            }
            g_VirtualScene[merge_as] = theobject;
        }
        else
        {
            g_VirtualScene[model->shapes[shape].name] = theobject;
        }
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
    // O GL_ELEMENT_ARRAY_BUFFER NÃO é desligado aqui: o vínculo dele faz parte
    // do estado do VAO (desligá-lo com o VAO ligado o tiraria do VAO).

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

    // Constantes compartilhadas com o C++ (uma fonte só): os #define vão logo
    // depois da 1ª linha (#version), e "#line 2" mantém os números de linha
    // das mensagens de erro iguais aos do arquivo.
    char defines[160];
    snprintf(defines, sizeof(defines), "#define MAX_POINT_LIGHTS %d\n#define MAX_TUNNELS %d\n#line 2\n",
             MAX_POINT_LIGHTS, MAX_SHADER_TUNNELS);
    const size_t first_line_end = str.find('\n');
    str.insert(first_line_end == std::string::npos ? str.size() : first_line_end + 1, defines);
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
    // Janela minimizada (0 x 0): nada a ajustar (evita razão de aspecto 0/0).
    if (width <= 0 || height <= 0)
        return;

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
    // Teclas seguradas (a repetição automática não muda o estado).
    if (key >= 0 && key <= GLFW_KEY_LAST)
    {
        if (action == GLFW_PRESS)
            g_KeyHeld[key] = true;
        else if (action == GLFW_RELEASE)
            g_KeyHeld[key] = false;
    }

    // Telas da GUI (menu, pausa, game over): se o ImGui estiver usando o
    // teclado (campo de texto em foco ou navegação por teclado, NavEnableKeyboard),
    // a tecla é só dele. A GUI não tem campo de texto e a navegação por teclado
    // está desligada, então WantCaptureKeyboard hoje nunca é verdadeiro aqui:
    // a guarda é só uma rede de segurança para uma GUI futura.
    if (g_Game.mode != GameMode::PLAYING && g_Game.mode != GameMode::FREE_CAMERA &&
        ImGui::GetCurrentContext() != NULL && ImGui::GetIO().WantCaptureKeyboard)
        return;

    const bool press = (action == GLFW_PRESS);
    const bool confirm = (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE);

    // Tecla C: alterna entre a corrida (3ª pessoa) e a câmera livre, com o
    // jogo congelado. Só vale em PLAYING / FREE_CAMERA.
    if (key == GLFW_KEY_C && press)
    {
        if (g_Game.mode == GameMode::PLAYING)
            EnterFreeCamera(window);
        else if (g_Game.mode == GameMode::FREE_CAMERA)
            LeaveFreeCamera(window);
        return;
    }

    // P ou ESC: pausa a corrida / continua. ESC não fecha mais o programa
    // (sair é pelo botão "Sair" do menu). Na câmera livre, P/ESC sai dela
    // (devolve o cursor) e já entra na pausa.
    if ((key == GLFW_KEY_P || key == GLFW_KEY_ESCAPE) && press)
    {
        if (g_Game.mode == GameMode::PLAYING || g_Game.mode == GameMode::FREE_CAMERA)
            PauseRun(window);
        else if (g_Game.mode == GameMode::PAUSED)
            ResumeRun();
        return;
    }

    // Menu: Enter (ou Espaço) equivale ao botão "Jogar". O return evita que
    // o mesmo Espaço vire um pulo na corrida nova.
    if (press && confirm && g_Game.mode == GameMode::MENU)
    {
        StartRunFromMenu();
        return;
    }

    // Fim de jogo: Enter (ou Espaço) equivale ao botão "Jogar de novo".
    if (press && confirm && g_Game.mode == GameMode::GAME_OVER)
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

    // Tecla R: recarregamos os shaders dos arquivos "shader_fragment.glsl" e "shader_vertex.glsl".
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        LoadShadersFromFiles();
        fprintf(stdout,"Shaders recarregados!\n");
        fflush(stdout);
    }
}

// Deslocamento do mouse em pixels -> gira a câmera livre (só no modo câmera
// livre; no resto do jogo o mouse não faz nada).
void FreeCameraMouseLook(float dx_pixels, float dy_pixels)
{
    if (g_Game.mode == GameMode::FREE_CAMERA)
        g_FreeCamera.Look(dx_pixels, dy_pixels);
}

// Chamada pela GLFW a cada movimento do cursor.
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    // Fora da câmera livre o movimento do cursor é só da GUI. Na câmera livre
    // ele sempre gira o olhar: o cursor está capturado (invisível) e as únicas
    // janelas na tela (HUD e dica) são NoInputs, então o ImGui nunca fica com
    // o mouse (WantCaptureMouse não é consultado aqui, de propósito).
    if (g_Game.mode != GameMode::FREE_CAMERA)
    {
        g_CursorPointValid = false; // recalibra na próxima captura
        return;
    }
    if (g_CursorPointValid)
        FreeCameraMouseLook((float)(xpos - g_LastCursorX), (float)(ypos - g_LastCursorY));
    g_LastCursorX = xpos;
    g_LastCursorY = ypos;
    g_CursorPointValid = true;
}

// (direita, cima, frente) pedido pelas teclas seguradas: D/A, Espaço/Shift, W/S.
glm::vec3 FreeCameraInputDirection()
{
    glm::vec3 dir(0.0f, 0.0f, 0.0f);
    if (g_KeyHeld[GLFW_KEY_D]) dir.x += 1.0f;
    if (g_KeyHeld[GLFW_KEY_A]) dir.x -= 1.0f;
    if (g_KeyHeld[GLFW_KEY_SPACE]) dir.y += 1.0f;
    if (g_KeyHeld[GLFW_KEY_LEFT_SHIFT] || g_KeyHeld[GLFW_KEY_RIGHT_SHIFT]) dir.y -= 1.0f;
    if (g_KeyHeld[GLFW_KEY_W]) dir.z += 1.0f;
    if (g_KeyHeld[GLFW_KEY_S]) dir.z -= 1.0f;
    return dir;
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char* description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

// Texto de depuração (tecla H), no canto inferior esquerdo: velocidade e
// faixa do jogador. O placar (pontos, moedas, distância, power-ups) é o HUD
// do ImGui (Gui_Hud()).
void TextRendering_ShowRunInfo(GLFWwindow* window)
{
    if ( !g_ShowInfoText || g_Game.mode == GameMode::MENU )
        return;

    float lineheight = TextRendering_LineHeight(window);
    char buffer[80];
    snprintf(buffer, sizeof(buffer), "Velocidade: %4.1f m/s   Faixa: %+d",
             g_Game.player.speed, g_Game.player.lane);
    TextRendering_PrintString(window, buffer, -1.0f+lineheight/10, -1.0f+2*lineheight/10, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow* window)
{
    // Só com o texto de depuração ligado (H) e nunca no menu (Tarefa 7.1b).
    if ( !g_ShowInfoText || g_Game.mode == GameMode::MENU )
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

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :

