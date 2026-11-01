#ifndef _COLLISIONS_H
#define _COLLISIONS_H

#include <glm/vec3.hpp>

// Testes de intersecção do jogo (ver src/collisions.cpp). Sem OpenGL.
// Tudo em coordenadas de pista: x = lateral, y = altura, z = s (distância
// ao longo da pista). Todos os intervalos são fechados: encostar conta.

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

// Quanto o topo da caixa passou de um teto na altura 'ceiling' (0 se não
// passou; encostar não conta). Usado para o teto do túnel.
float CeilingPenetration(const AABB& box, float ceiling);

#endif
