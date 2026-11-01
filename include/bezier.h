#ifndef _BEZIER_H
#define _BEZIER_H

#include <glm/vec3.hpp>

// Curva de Bézier cúbica (Tarefa 5.3), sem OpenGL: usada pelo porco voador do
// cofrinho (Game). Implementação própria na forma de Bernstein.

// Ponto da curva em t (normalmente em [0, 1]):
//   B(t) = (1-t)^3 P0 + 3 (1-t)^2 t P1 + 3 (1-t) t^2 P2 + t^3 P3
// B(0) = P0 e B(1) = P3; P1 e P2 só puxam a curva.
glm::vec3 CubicBezier(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t);

// Derivada dB/dt (vetor tangente, não normalizado):
//   B'(t) = 3 (1-t)^2 (P1-P0) + 6 (1-t) t (P2-P1) + 3 t^2 (P3-P2)
glm::vec3 CubicBezierTangent(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t);

#endif // _BEZIER_H
