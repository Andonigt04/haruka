#ifndef WORLD_SYSTEM_H
#define WORLD_SYSTEM_H

#include "math_types.h"
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

struct CelestialBody
{
    /** @brief High-precision world-space position. */
    Haruka::WorldPos worldPos;
    /** @brief Camera-relative/local position cache. */
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

namespace Haruka {

/**
 * @brief Manages celestial body storage, origin shifting, and culling metadata.
 */
class WorldSystem {
public:
    WorldSystem();
    ~WorldSystem();

    /** @brief Converts world-space position into local-space relative to reference. */
    static Haruka::LocalPos toLocal(Haruka::WorldPos objectPos, Haruka::WorldPos referencePos);

    /** @brief Updates world origin anchor. */
    void updateOrigin(Haruka::WorldPos newOrigin);
    /** @brief Recomputes local positions for all registered bodies. */
    void updateLocalPositions(Haruka::WorldPos cameraWorldPos);

    /** @brief Adds a celestial body copy to internal storage. */
    void addBody(const CelestialBody& body);
    /** @brief Removes celestial body by name if present. */
    void removeBody(const std::string& name);
    /** @brief Finds body by name and returns mutable non-owning pointer. */
    CelestialBody* findBody(const std::string& name);

    /** @name Accessors */
    ///@{
    const std::vector<CelestialBody>& getBodies() const;
    size_t getBodyCount() const;
    Haruka::WorldPos getOrigin() const;
    ///@}

    /** @brief Initializes compute resources for culling path. */
    void initComputeShaders();
    /** @brief Sets LOD transition distances. */
    void setLODDistances(float lod0, float lod1, float lod2, float lod3);
    /** @brief Executes frustum culling update for registered bodies. */
    void frustumCull(Haruka::WorldPos cameraPos, const glm::mat4& viewProj, float frustumDistance);

private:
    Haruka::WorldPos worldOrigin;
    std::vector<CelestialBody> celestialBodies;
    float lodDistances[4];
};
}

#endif