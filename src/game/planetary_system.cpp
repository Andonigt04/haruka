#include "planetary_system.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace Haruka {

PlanetarySystem::PlanetarySystem() {}

PlanetarySystem::~PlanetarySystem() {}

void PlanetarySystem::init(Scene* scene, WorldSystem* worldSystem) {
    this->scene = scene;
    this->worldSystem = worldSystem;
    
    std::cout << "✓ Planetary System initialized (empty)" << std::endl;
    std::cout << "  Use addStar() and addPlanet() to populate the system" << std::endl;
}

CelestialBody* PlanetarySystem::addStar(const std::string& name, double mass, double radius) {
    CelestialBody sun;
    sun.name = name;
    sun.worldPos = {0.0, 0.0, 0.0};
    sun.localPos = {0.0f, 0.0f, 0.0f};
    sun.mass = mass;
    sun.radius = static_cast<float>(radius / Units::KM);
    sun.color = glm::vec3(1.0f, 0.9f, 0.0f);
    sun.emissionStrength = 1.0f;
    sun.visible = 1;
    sun.lodLevel = 0;
    sun.velocity = glm::vec3(0.0f);
    
    worldSystem->addBody(sun);
    star = worldSystem->findBody(name);
    bodyNames.push_back(name);
    
    std::cout << "⭐ Star added: " << name << " (radius: " << radius / Units::MEGAMETER << " Mm)" << std::endl;
    return star;
}

CelestialBody* PlanetarySystem::addPlanet(const std::string& name, double orbitalDistance, double mass, double radius, glm::vec3 color) {
    if (!star) {
        std::cerr << "❌ Cannot add planet without a star" << std::endl;
        return nullptr;
    }
    
    CelestialBody planet;
    planet.name = name;
    planet.worldPos = {orbitalDistance, 0.0, 0.0};
    planet.localPos = {static_cast<float>(orbitalDistance / Units::MEGAMETER), 0.0f, 0.0f};
    planet.mass = mass;
    planet.radius = static_cast<float>(radius / Units::KM);
    planet.color = color;
    planet.emissionStrength = 0.0f;
    planet.visible = 1;
    planet.lodLevel = 0;
    
    // Velocidad orbital: v = sqrt(G * M / r)
    double orbitalVelocity = std::sqrt(G * star->mass / orbitalDistance);
    planet.velocity = glm::vec3(0.0f, static_cast<float>(orbitalVelocity), 0.0f);
    
    worldSystem->addBody(planet);
    bodyNames.push_back(name);
    
    std::cout << "🪨 Planet added: " << name 
              << " (distance: " << orbitalDistance / Units::MEGAMETER << " Mm"
              << ", radius: " << radius / Units::KM << " km)" << std::endl;
    return worldSystem->findBody(name);
}

CelestialBody* PlanetarySystem::addBody(const CelestialBody& body) {
    worldSystem->addBody(body);
    bodyNames.push_back(body.name);
    
    std::cout << "✓ Body added: " << body.name << std::endl;
    return worldSystem->findBody(body.name);
}

void PlanetarySystem::update(double dt) {
    if (!worldSystem || !scene) return;
    
    simulationTime += dt * timeScale;
    
    integrateOrbits(dt);
    updateWorldOrigin();
    syncSceneWithOrbits();
    updatePlayerOnPlanet();
    
    if (player) {
        player->update(static_cast<float>(dt));
    }
}

void PlanetarySystem::integrateOrbits(double dt) {
    auto& bodies = const_cast<std::vector<CelestialBody>&>(worldSystem->getBodies());
    
    if (!star) return;
    
    for (auto& body : bodies) {
        if (body.name == star->name) continue;
        
        glm::dvec3 direction = glm::dvec3(star->worldPos.x, star->worldPos.y, star->worldPos.z) - 
                               glm::dvec3(body.worldPos.x, body.worldPos.y, body.worldPos.z);
        double distance = glm::length(direction);
        
        if (distance > body.radius * Units::KM) {
            double forceMagnitude = (G * star->mass * body.mass) / (distance * distance);
            glm::dvec3 acc = glm::normalize(direction) * forceMagnitude;
            
            body.velocity += glm::vec3(acc * dt * timeScale);
            body.worldPos.x += body.velocity.x * dt * timeScale;
            body.worldPos.y += body.velocity.y * dt * timeScale;
            body.worldPos.z += body.velocity.z * dt * timeScale;
        }
    }
}

void PlanetarySystem::updateWorldOrigin() {
    if (!player) return;
    
    auto closestPlanet = getClosestPlanet();
    if (closestPlanet) {
        worldSystem->updateOrigin(closestPlanet->worldPos);
    }
}

void PlanetarySystem::syncSceneWithOrbits() {
    if (!scene || !worldSystem) return;
    
    for (const auto& body : worldSystem->getBodies()) {
        auto sceneObj = scene->getObject(body.name);
        if (!sceneObj) {
            SceneObject obj;
            obj.name = body.name;
            obj.type = "CelestialBody";
            scene->addObject(obj);
            sceneObj = scene->getObject(obj.name);
        }
        
        if (sceneObj) {
            sceneObj->position = glm::dvec3(body.localPos.x, body.localPos.y, body.localPos.z);
            sceneObj->scale = glm::dvec3(body.radius / 100000.0);
            sceneObj->color = body.color;
        }
    }
}

CelestialBody* PlanetarySystem::findBody(const std::string& name) {
    return worldSystem->findBody(name);
}

CelestialBody* PlanetarySystem::getClosestPlanet() {
    if (!player) return nullptr;
    
    glm::vec3 playerPos = player->getPosition();
    CelestialBody* closest = nullptr;
    float minDist = FLT_MAX;
    
    for (const auto& bodyName : bodyNames) {
        auto body = findBody(bodyName);
        if (!body || body->name == (star ? star->name : "")) continue;
        
        glm::vec3 bodyPos = glm::vec3(body->localPos.x, body->localPos.y, body->localPos.z);
        float dist = glm::length(playerPos - bodyPos);
        
        if (dist < minDist) {
            minDist = dist;
            closest = body;
        }
    }
    
    return closest;
}

void PlanetarySystem::updatePlayerOnPlanet() {
    if (!player) return;
    
    auto closestPlanet = getClosestPlanet();
    if (!closestPlanet) return;
    
    glm::dvec3 playerPos = player->getPosition();
    glm::dvec3 planetPos = glm::dvec3(closestPlanet->localPos.x, closestPlanet->localPos.y, closestPlanet->localPos.z);
    glm::dvec3 planetToPlayer = playerPos - planetPos;
    double distToPlanet = glm::length(planetToPlayer);
    
    if (distToPlanet < closestPlanet->radius + 100.0) {
        glm::dvec3 surfacePos = planetPos + glm::normalize(planetToPlayer) * (closestPlanet->radius + 2.0);
        player->setPosition(surfacePos);
    }
}

void PlanetarySystem::render() {
    // Renderizado a través de sincronización con escena
}

}