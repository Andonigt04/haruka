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

class PlanetarySystem {
public:
    PlanetarySystem();
    ~PlanetarySystem();
    
    void init(Scene* scene, WorldSystem* worldSystem);
    void update(double dt);
    void render();

    // Terreno planetario (gestionado por gameplay, no por Application)
    void initTerrain(int size = 1024, float heightScale = 200.0f, int seed = 42);
    void renderTerrain(Shader& shader, const glm::vec3& cameraPos);
    void setDetailedSurfaceData(const std::string& bodyName, std::shared_ptr<PlanetGenerator::PlanetData> data);
    
    // API para agregar cuerpos celestes
    CelestialBody* addStar(const std::string& name, double mass = 1.989e30, double radius = Units::STAR_RADIUS_MEDIUM);
    CelestialBody* addPlanet(const std::string& name, double orbitalDistance, double mass, double radius, glm::vec3 color = glm::vec3(1.0f));
    CelestialBody* addBody(const CelestialBody& body);
    
    // Getters
    Scene* getScene() { return scene; }
    WorldSystem* getWorldSystem() { return worldSystem; }
    CelestialBody* findBody(const std::string& name);
    CelestialBody* getStar() { return star; }
    CelestialBody* getClosestPlanet();
    
    Character* getPlayer() { return player.get(); }
    void setPlayer(std::unique_ptr<Character> p) { player = std::move(p); }
    Terrain* getTerrain() { return terrain.get(); }
    
    // Configuración
    void setTimeScale(double scale) { timeScale = scale; }
    double getTimeScale() const { return timeScale; }
    
    // Física Planetaria
    double calculateGravityAtPosition(const glm::dvec3& worldPos, glm::dvec3& gravityDirection);
    void applyPlanetaryPhysics(double dt);
    void setPlayerFlightMode(bool enabled);
    
private:
    Scene* scene = nullptr;
    WorldSystem* worldSystem = nullptr;
    std::unique_ptr<Character> player;
    std::unique_ptr<Terrain> terrain;
    std::unordered_map<std::string, std::shared_ptr<PlanetGenerator::PlanetData>> detailedSurfaceData;
    
    CelestialBody* star = nullptr;
    std::vector<std::string> bodyNames;
    
    double simulationTime = 0.0;
    double timeScale = 1000.0;
    
    const double G = 6.67430e-11;
    
    void syncSceneWithOrbits();
    void updatePlayerOnPlanet();
    void updateWorldOrigin();
    void integrateOrbits(double dt);
    double getSurfaceRadiusAtDirection(const CelestialBody* body, const glm::dvec3& planetToPoint) const;
};

}