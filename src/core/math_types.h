#ifndef MATH_TYPES_H
#define MATH_TYPES_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Haruka {
    // ===== POSITION TYPES =====
    using WorldPos = glm::dvec3; 
    using LocalPos = glm::vec3;
    
    // ===== ROTATION =====
    using Rotation = glm::dquat;

    namespace Units
    {
        // Unit distance base
        constexpr double KM = 1.0;

        // Large Scales
        constexpr double MEGAMETER = 1000.0 * KM; // 10³ km
        constexpr double GIGAMETER = 1000000.0 * KM; // 10⁶ km
        constexpr double TERAMETER = 1000000000.0 * KM; // 10⁹ km
        constexpr double PETAMETER = 1000000000000.0 * KM; // 10¹² km

        // Celestial Scales
        constexpr double PLANETARY_RADIUS_SMALL = 1000.0 * KM;
        constexpr double PLANETARY_RADIUS_MEDIUM = 6000.0 * KM;
        constexpr double PLANETARY_RADIUS_LARGE = 70000.0 * KM;

        constexpr double ORBITAL_DISTANCE_CLOSE = 100.0 * MEGAMETER;
        constexpr double ORBITAL_DISTANCE_MEDIUM = 150.0 * GIGAMETER;
        constexpr double ORBITAL_DISTANCE_LARGE = 5.0 * TERAMETER;

        constexpr double STAR_RADIUS_SMALL = 500000.0 * KM;
        constexpr double STAR_RADIUS_MEDIUM = 700000.0 * KM;
        constexpr double STAR_RADIUS_LARGE = 1500000.0 * KM;

        // Galactic scales
        constexpr double LIGHT_SPEED = 299792.458 * KM;
        constexpr double LIGHT_YEAR = 9.460730e12 * KM;
        constexpr double PARSEC = 3.0857e13 * KM;

        constexpr double GALACTIC_RADIUS = 50000.0 * LIGHT_YEAR; 
        constexpr double INTERGALACTIC_DISTANCE = 2.5e6 * LIGHT_YEAR;

        // Visualización astronómica real: 1 unidad de render = 1 km
        // (sin compresión global)
        constexpr double RENDER_KM_PER_UNIT = 1.0;

        inline double kmToRender(double km) {
            return km / RENDER_KM_PER_UNIT;
        }

        inline glm::dvec3 kmToRender(const glm::dvec3& kmVec) {
            return kmVec / RENDER_KM_PER_UNIT;
        }

        inline glm::vec3 kmToRender(const glm::vec3& kmVec) {
            return kmVec / static_cast<float>(RENDER_KM_PER_UNIT);
        }
    }
};
#endif