#ifndef MATH_TYPES_H
#define MATH_TYPES_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Haruka {
    // Para posiciones en el universo (64 bits)
    using WorldPos = glm::dvec3; 
    
    // Para datos que van a la GPU o distancias relativas (32 bits)
    using LocalPos = glm::vec3;
    
    // Rotaciones (siempre usar cuaterniones para evitar el Gimbal Lock en el espacio)
    using Rotation = glm::dquat;

};
#endif