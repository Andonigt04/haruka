#pragma once
#include "core/world_system.h"
#include "core/math_types.h"
#include "character.h"
#include "core/scene.h"
#include "game/planet_generator.h"
#include "renderer/terrain.h"
#include "renderer/shader.h"
#include <memory>
#include <unordered_map>

namespace Haruka {

/**
 * @brief Coordinates planetary bodies, orbital simulation, terrain, and player flight state.
 */
class PlanetarySystem {
public:
    /** @brief Constructs an uninitialized planetary system. */
    PlanetarySystem();
    /** @brief Releases owned runtime resources. */
    ~PlanetarySystem();
    
    /** @brief Binds the scene and world systems used by the simulation. */
    void init(Scene* scene, WorldSystem* worldSystem);
    /** @brief Advances orbital/player simulation by one timestep. */
    void update(double dt);
    /** @brief Renders planetary bodies and terrain layers. */
    void render();

    /** @name Terrain setup */
    ///@{
    void initTerrain(int size = 1024, float heightScale = 200.0f, int seed = 42);
    void renderTerrain(Shader& shader, const glm::vec3& cameraPos);
    void setDetailedSurfaceData(const std::string& bodyName, const PlanetGenerator::PlanetData& data);
    ///@}
    
    /** @name Celestial body creation */
    ///@{
    CelestialBody* addStar(const std::string& name, double mass = 1.989e30, double radius = Units::STAR_RADIUS_MEDIUM);
    CelestialBody* addPlanet(const std::string& name, double orbitalDistance, double mass, double radius, glm::vec3 color = glm::vec3(1.0f));
    CelestialBody* addBody(const CelestialBody& body);
    ///@}
    
    /** @name Accessors */
    ///@{
    Scene* getScene() { return scene; }
    WorldSystem* getWorldSystem() { return worldSystem; }
    CelestialBody* findBody(const std::string& name);
    CelestialBody* getStar() { return star; }
    CelestialBody* getClosestPlanet();
    
    Character* getPlayer() { return player.get(); }
    void setPlayer(std::unique_ptr<Character> p) { player = std::move(p); }
    Terrain* getTerrain() { return terrain.get(); }
    ///@}
    
    /** @name Simulation configuration */
    ///@{
    void setTimeScale(double scale) { timeScale = scale; }
    double getTimeScale() const { return timeScale; }
    ///@}
    
    /** @name Planetary physics */
    ///@{
    double calculateGravityAtPosition(const glm::dvec3& worldPos, glm::dvec3& gravityDirection);
    void applyPlanetaryPhysics(double dt);
    void setPlayerFlightMode(bool enabled);
    ///@}
    
private:
    Scene* scene = nullptr;
    WorldSystem* worldSystem = nullptr;
    std::unique_ptr<Character> player;
    std::unique_ptr<Terrain> terrain;
    std::unordered_map<std::string, PlanetGenerator::PlanetData> detailedSurfaceData;
    
    CelestialBody* star = nullptr;
    std::vector<std::string> bodyNames;
    
    double simulationTime = 0.0;
    double timeScale = 1000.0;
    
    const double G = 6.67430e-11;
    
    /** @brief Synchronizes scene object transforms with orbital simulation state. */
    void syncSceneWithOrbits();
    /** @brief Updates player attachment/orientation on a planet surface. */
    void updatePlayerOnPlanet();
    /** @brief Updates world origin based on current simulation focus. */
    void updateWorldOrigin();
    /** @brief Integrates orbital positions for all managed bodies. */
    void integrateOrbits(double dt);
    /** @brief Samples a generated body surface radius in a given direction. */
    double getSurfaceRadiusAtDirection(const CelestialBody* body, const glm::dvec3& planetToPoint) const;
};

}