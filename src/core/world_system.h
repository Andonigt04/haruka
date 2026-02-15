#ifndef WORLD_SYSTEM_H
#define WORLD_SYSTEM_H

#include "math_types.h"
#include <vector>
#include <string>
struct CelestialBody
{
    Haruka::WorldPos worldPos;
    Haruka::LocalPos localPos;
    glm::vec3 velocity;
    float radius;
    float mass;
    std::string name;

    glm::vec3 color;
    float emissionStrength;
};

class WorldSystem {
public:
    WorldSystem() : worldOrigin(0.0, 0.0, 0.0) {}

    // ===== COORDINATE CONVERSION =====
    static Haruka::LocalPos toLocal(Haruka::WorldPos objectPos, Haruka::WorldPos referencePos) {
        glm::dvec3 diff = objectPos - referencePos;
        return Haruka::LocalPos(diff);
    }

    // ===== FLOATING ORIGIN =====
    void updateOrigin(Haruka::WorldPos newOrigin)
    {
        glm::dvec3 offset = newOrigin - worldOrigin;
        worldOrigin = newOrigin;

        for (auto& body : celestialBodies)
        {
            body.worldPos -= offset;
        }
    }

    void updateLocalPositions(Haruka::WorldPos cameraWorldPos)
    {
        for (auto& body : celestialBodies)
        {
            body.localPos = toLocal(body.worldPos, cameraWorldPos);
        }
    }

    // ===== OBJECT MANAGEMENT =====
    void addBody(const CelestialBody& body)
    {
        celestialBodies.push_back(body);
    }

    void removeBody(const std::string& name) {
        celestialBodies.erase(
            std::remove_if(celestialBodies.begin(), celestialBodies.end(),
                [&name](const CelestialBody& b) { return b.name == name; }),
            celestialBodies.end()
        );
    }

    CelestialBody* findBody(const std::string& name)
    {
        for (auto& body : celestialBodies)
        {
            if (body.name == name) return &body;
        }
    }

    const std::vector<CelestialBody>& getBodies() const
    {
        return celestialBodies;
    }

    size_t getBodyCount() const
    {
        return celestialBodies.size();
    }

    Haruka::WorldPos getOrigin() const
    {
        return worldOrigin;
    }
private:
    Haruka::WorldPos worldOrigin;
    std::vector<CelestialBody> celestialBodies;
};
#endif