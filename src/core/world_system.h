#ifndef WORLD_SYSTEM_H
#define WORLD_SYSTEM_H

#include "math_types.h"
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

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
    uint32_t visible;
    uint32_t lodLevel;
};

class WorldSystem {
public:
    WorldSystem();
    ~WorldSystem();

    // Coordinate conversion
    static Haruka::LocalPos toLocal(Haruka::WorldPos objectPos, Haruka::WorldPos referencePos);

    // Origin management
    void updateOrigin(Haruka::WorldPos newOrigin);
    void updateLocalPositions(Haruka::WorldPos cameraWorldPos);

    // Object management
    void addBody(const CelestialBody& body);
    void removeBody(const std::string& name);
    CelestialBody* findBody(const std::string& name);

    // Getters
    const std::vector<CelestialBody>& getBodies() const;
    size_t getBodyCount() const;
    Haruka::WorldPos getOrigin() const;

    // Culling & LOD
    void initComputeShaders();
    void setLODDistances(float lod0, float lod1, float lod2, float lod3);
    void frustumCull(Haruka::WorldPos cameraPos, const glm::mat4& viewProj, float frustumDistance);

private:
    Haruka::WorldPos worldOrigin;
    std::vector<CelestialBody> celestialBodies;
    float lodDistances[4];
};

#endif