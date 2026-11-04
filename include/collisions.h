#ifndef _COLLISIONS_H
#define _COLLISIONS_H

#include <vector>

#include <glm/vec3.hpp>

#include "track.h"

// Testes de intersecção do jogo (ver src/collisions.cpp). Sem OpenGL.
// Tudo em coordenadas de pista: x = lateral, y = altura, z = s (distância
// ao longo da pista) -- exceto os testes de ponto no túnel (PointInsideTunnel*),
// que recebem um ponto em coordenadas de MUNDO. Todos os intervalos são
// fechados: encostar conta.

// Caixa alinhada aos eixos. Invariante: min <= max em cada componente.
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

// Caixa a partir do centro e das semi-extensões. Semi-extensões negativas
// são tomadas em módulo, de modo que sempre vale min <= max.
AABB MakeAABB(glm::vec3 center, glm::vec3 half_extents);

// Interseção caixa-caixa (faces encostadas contam como interseção).
bool AABBIntersectsAABB(const AABB& a, const AABB& b);

// Caixa varrida de um movimento: a menor caixa que contém a caixa antes e a
// caixa depois do passo. Testar só a posição final deixaria um corpo rápido
// "atravessar" o outro entre dois quadros (tunneling).
AABB SweepAABB(const AABB& before, const AABB& after);

// Idem para um obstáculo que andou dz (m) ao longo de s neste passo e está em
// 'now': a caixa antes era 'now' deslocada de -dz em z (dz < 0: o trem vindo
// na direção do jogador estendia-se para trás de max.z; dz > 0, de min.z).
AABB SweepAABBAlongZ(const AABB& now, float dz);

// Esfera-caixa: ponto da caixa mais próximo do centro a distância <= raio.
bool SphereIntersectsAABB(glm::vec3 center, float radius, const AABB& box);

// Esfera-esfera (esferas tangentes contam como interseção).
bool SphereIntersectsSphere(glm::vec3 c1, float r1, glm::vec3 c2, float r2);

// Altura da rampa em (x, s). A rampa é a caixa `ramp`: sobe linearmente de
// ramp.min.y em s = ramp.min.z até ramp.max.y em s = ramp.max.z. Devolve false
// (sem tocar em *out_height) se (x, s) está fora da caixa.
bool RampHeightAt(const AABB& ramp, float x, float s, float* out_height);

// Altura da rampa sob a "pegada" (projeção em x e s) da caixa: se a pegada
// toca a rampa, *out_height = altura do ponto mais alto da rampa sob ela, que
// é o da frente da pegada (box.max.z, limitado ao fim da rampa), já que a
// rampa sobe com s. Assim os pés chegam ao topo da rampa (= teto do trem
// colado nela) no instante em que a frente do jogador chega ao trem. Devolve
// false (sem tocar em *out_height) se a pegada não toca a rampa.
bool RampHeightUnder(const AABB& ramp, const AABB& box, float* out_height);

// A lateral da rampa é parede para quem entra de lado. 'from' e 'to' são a
// caixa do jogador no fim do passo com o x do começo e com o x novo (a troca
// de faixa do passo). Bloqueia (true) se a pegada de 'from' não toca a rampa,
// a de 'to' toca, e a altura da rampa sob 'to' passa de to.min.y +
// step_height (degrau alto demais). Então *out_x = centro x mais próximo de
// 'to', do lado de 'from', com a pegada fora da faixa x da rampa. Entrar pela
// frente (pegada já na faixa x da rampa) não é lateral: devolve false.
bool RampSideBlocks(const AABB& ramp, const AABB& from, const AABB& to, float step_height,
                    float* out_x);

// Sobreposição das pegadas (x e z) de duas caixas, ignorando a altura.
bool FootprintsOverlap(const AABB& a, const AABB& b);

// Jogador pousando sobre o teto de um obstáculo (ex.: trem): pegada atual
// sobre o obstáculo, base atual no topo ou abaixo dele (mais a tolerância) e
// "vindo de cima": a base estava em ou acima do topo (menos a tolerância) no
// quadro anterior OU no instante em que a pegada do jogador começou a tocar a
// do obstáculo (interpolado linearmente entre os dois quadros; se já tocava
// no quadro anterior, é a base anterior). O segundo caso é o de quem sobe
// pela rampa colada à frente do trem, ou troca de faixa de um teto para o
// teto vizinho. Batida frontal/lateral (base abaixo do topo ao entrar) devolve
// false. Sem limite inferior para a base atual: uma queda rápida que atravessa
// o teto num único quadro ainda é pouso.
bool LandsOnTop(const AABB& player_prev, const AABB& player_now,
                const AABB& obstacle, float tolerance);

// Jogador EM PÉ sobre o teto de um obstáculo (trem): pegada sobre ele e base
// no topo ou acima (menos a tolerância). Usado (com o jogador apoiado) para o
// trem não matar quem está em cima dele, qualquer que seja a fase do passo, e
// para a altura do chão sob o jogador (câmera). Revisão final, item 6.
bool StandsOnTop(const AABB& player, const AABB& obstacle, float tolerance);

// Parada de uma caixa que anda para s MENOR (trem móvel vindo na direção do
// jogador): se 'other' está na mesma faixa (as pegadas em x se sobrepõem) e
// inteiro à frente dela no sentido do movimento (other.max.z <= mover.min.z,
// com 1 mm de tolerância), devolve true e *stop_z = other.max.z + gap: a ponta
// dianteira (min.z) do 'mover' não pode passar daí. Revisão final, item 6.
bool StopBehindAlongZ(const AABB& mover, const AABB& other, float gap, float* stop_z);

// Proximidade ao longo de s (z) usada no spawn (Revisão final, item 6):
// o intervalo [s0, s1] encosta na caixa estendida de 'margin' para os dois
// lados em z (fechado) ...
bool SpanTouchesBoxAlongZ(float s0, float s1, const AABB& box, float margin);
// ... e o ponto s está estritamente dentro dela (aberto: na borda, não).
bool PointNearBoxAlongZ(float s, const AABB& box, float margin);

// Quanto o topo da caixa passou de um teto na altura 'ceiling' (0 se não
// passou; encostar não conta). Usado para o teto do túnel.
float CeilingPenetration(const AABB& box, float ceiling);

// Teto efetivo (altura máxima do topo da caixa) a `distance` metros antes da
// boca de um túnel: o teto de verdade (`ceiling`) mais `slope` metros de folga
// por metro de distância (distance <= 0: o próprio teto). Cresce de forma
// contínua com a distância, então quem entra no túnel no ar é achatado aos
// poucos em vez de levar um tranco na boca.
float TunnelApproachCeiling(float ceiling, float distance, float slope);

// Ponto de mundo dentro do volume interior de um túnel (um segmento reto com
// tunnel = true): s dentro de [s_start, s_start + length], deslocamento lateral
// com sinal |lat| <= TUNNEL_INNER_HALF_WIDTH e 0 < altura < TunnelCeilingHeight(lat)
// (a abóbada: ~7 m no eixo, descendo até 2 m junto às paredes). O s e o
// deslocamento lateral saem da projeção exata do ponto no eixo do segmento.
// Acima do teto, ao lado da parede ou antes/depois do túnel: false. Usado
// pela câmera livre para escurecer o céu/sol só quando ela está dentro.
bool PointInsideTunnelSegment(const TrackSegment& seg, glm::vec3 point);

// Idem para qualquer túnel mantido da pista.
bool PointInsideTunnel(const Track& track, glm::vec3 point);

// ---- Câmera livre x cenário ----
//
// A câmera livre é uma esfera de raio 'radius' (veja FREE_CAMERA_RADIUS em
// camera.h). As peças sólidas do túnel (paredes, abóbada e portais) são
// testadas pela forma exata da seção, em coordenadas de pista
// p = (lateral, altura, s): a casca da abóbada é a região entre o intradorso
// (TUNNEL_INNER_HALF_WIDTH) e o extradorso (+ TUNNEL_WALL_THICKNESS), no
// trecho do segmento; cada portal é uma parede de 2 *
// TUNNEL_PORTAL_HALF_WIDTH x TUNNEL_PORTAL_HEIGHT x TUNNEL_PORTAL_DEPTH com a
// abertura do intradorso, logo antes da entrada e logo depois da saída. A
// esfera é aproximada pela região sólida inflada de 'radius' (conservador
// nos cantos). 'seg' que não é túnel: false.
bool SphereHitsTunnel(const TrackSegment& seg, glm::vec3 p, float radius);

// A câmera livre (esfera de centro 'world', em coordenadas de MUNDO) encosta
// em algo sólido? Chão: centro abaixo de 'radius' (na altura da pista, se o
// ponto se projeta nela; senão, y do mundo). Perto da pista
// (Track::WorldToTrack até FREE_CAMERA_TRACK_REACH m): as caixas 'solids' em
// coordenadas de pista (obstáculos, muros, postes, prédios, jogador) com
// SphereIntersectsAABB e as peças dos túneis mantidos (SphereHitsTunnel).
// Longe da pista, só o chão (lá não há cenário gerado).
const float FREE_CAMERA_TRACK_REACH = 30.0f; // m
bool FreeCameraBlocked(const Track& track, const std::vector<AABB>& solids, glm::vec3 world,
                       float radius);

#endif
