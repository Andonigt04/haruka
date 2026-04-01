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

    // Inicializar terreno por defecto desde la capa planetaria (gameplay)
    initTerrain(512, 200.0f, 42);
    
    std::cout << "✓ Planetary System initialized (empty)" << std::endl;
    std::cout << "  Use addStar() and addPlanet() to populate the system" << std::endl;
}

void PlanetarySystem::initTerrain(int size, float heightScale, int seed) {
    terrain = std::make_unique<Terrain>(size, heightScale);
    terrain->setPosition(glm::vec3(-size * 0.5f, -10.0f, -size * 0.5f));
    terrain->setScale(glm::vec3(10.0f, 1.0f, 10.0f));
    terrain->generatePerlin(seed);
    std::cout << "[PlanetarySystem] Terrain initialized: size=" << size
              << " heightScale=" << heightScale << " seed=" << seed << std::endl;
}

void PlanetarySystem::renderTerrain(Shader& shader, const glm::vec3& cameraPos) {
    if (!terrain) return;

    // Asegurar texturas válidas para shaders deferred que esperan samplers
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);

    shader.setInt("texture_diffuse1", 0);
    shader.setInt("texture_specular1", 1);
    shader.setInt("texture_emissive1", 2);

    terrain->render(shader, cameraPos);
}

void PlanetarySystem::render() {
    // Render explícito de terreno desde gameplay cuando el pipeline lo solicite.
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
    applyPlanetaryPhysics(dt);
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
            glm::dvec3 localKm(body.localPos.x, body.localPos.y, body.localPos.z);
            double renderRadius = Units::kmToRender(static_cast<double>(body.radius));

            sceneObj->position = Units::kmToRender(localKm);
            sceneObj->scale = glm::dvec3(renderRadius);
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

// ========== PHYSICS SYSTEM ==========

double PlanetarySystem::calculateGravityAtPosition(const glm::dvec3& worldPos, glm::dvec3& gravityDirection) {
    double totalAcceleration = 0.0;
    gravityDirection = glm::dvec3(0.0);
    
    // Calcular gravedad de cada cuerpo celeste
    for (const auto& bodyName : bodyNames) {
        auto body = findBody(bodyName);
        if (!body) continue;
        
        glm::dvec3 bodyWorldPos = body->worldPos;
        glm::dvec3 toBody = bodyWorldPos - worldPos;
        double distance = glm::length(toBody);
        
        // No aplicar gravedad si estamos dentro del planeta
        if (distance < body->radius * Units::KM) {
            distance = body->radius * Units::KM;
        }
        
        // F = G * M / r²
        double acceleration = (G * body->mass) / (distance * distance);
        
        // Acumular dirección ponderada
        if (distance > 0) {
            gravityDirection += glm::normalize(toBody) * acceleration;
            totalAcceleration += acceleration;
        }
    }
    
    return glm::length(gravityDirection);
}

void PlanetarySystem::applyPlanetaryPhysics(double dt) {
    if (!player || !worldSystem) return;
    
    // Si el jugador está en modo vuelo/nave, desactivar gravedad
    if (player->isInFlightMode()) {
        return;
    }
    
    glm::dvec3 playerPos = player->getPosition();
    glm::dvec3 gravityDir;
    double gravityMagnitude = calculateGravityAtPosition(playerPos, gravityDir);
    
    // Aplicar gravedad al jugador
    glm::dvec3 currentVelocity = player->getVelocity();
    
    // Velocidad terminal (~50 m/s = 50 unidades km/s)
    double terminalVelocity = 50.0 / Units::KM;
    double currentSpeed = glm::length(currentVelocity);
    
    glm::dvec3 gravityAcceleration = gravityDir * gravityMagnitude * dt * timeScale;
    
    // Limitar a velocidad terminal
    if (currentSpeed > terminalVelocity) {
        gravityAcceleration = glm::normalize(currentVelocity) * terminalVelocity;
    } else {
        currentVelocity += gravityAcceleration;
    }
    
    player->setVelocity(currentVelocity);
    
    // Raycast para detectar colisión con terreno
    auto closestPlanet = getClosestPlanet();
    if (closestPlanet) {
        glm::dvec3 planetPos = glm::dvec3(closestPlanet->localPos.x, closestPlanet->localPos.y, closestPlanet->localPos.z);
        glm::dvec3 planetToPlayer = playerPos - planetPos;
        double distToPlanet = glm::length(planetToPlayer);
        double surfaceDistance = closestPlanet->radius + 0.1; // 100 metros sobre la superficie
        
        // Si está por debajo de la superficie, "aterrar"
        if (distToPlanet < surfaceDistance) {
            glm::dvec3 surfacePos = planetPos + glm::normalize(planetToPlayer) * surfaceDistance;
            player->setPosition(surfacePos);
            
            // Detener velocidad que entra en el planeta
            glm::dvec3 velocityNormal = glm::normalize(currentVelocity);
            glm::dvec3 surfaceNormal = glm::normalize(planetToPlayer);
            double inwardVelocity = glm::dot(velocityNormal, -surfaceNormal);
            
            if (inwardVelocity > 0) {
                currentVelocity -= velocityNormal * inwardVelocity * 0.8; // Friction
                player->setVelocity(currentVelocity);
            }
        }
    }
}

void PlanetarySystem::setPlayerFlightMode(bool enabled) {
    if (player) {
        player->setFlightMode(enabled);
    }
}

}