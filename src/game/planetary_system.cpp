#include "planetary_system.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>

namespace Haruka {

namespace {
constexpr double kPlayerCollisionRadiusKm = 0.00095; // ~0.95 m
constexpr double kGroundEpsilonKm = 0.00150;         // ~1.5 m de separación visual/colisión
constexpr double kTerminalFallSpeedKmS = 0.05;   // 50 m/s
}

namespace {
bool rayTriangleIntersect(
    const glm::dvec3& origin,
    const glm::dvec3& dir,
    const glm::dvec3& v0,
    const glm::dvec3& v1,
    const glm::dvec3& v2,
    double& t
) {
    const double EPS = 1e-9;
    glm::dvec3 edge1 = v1 - v0;
    glm::dvec3 edge2 = v2 - v0;
    glm::dvec3 h = glm::cross(dir, edge2);
    double a = glm::dot(edge1, h);
    if (a > -EPS && a < EPS) return false;

    double f = 1.0 / a;
    glm::dvec3 s = origin - v0;
    double u = f * glm::dot(s, h);
    if (u < 0.0 || u > 1.0) return false;

    glm::dvec3 q = glm::cross(s, edge1);
    double v = f * glm::dot(dir, q);
    if (v < 0.0 || u + v > 1.0) return false;

    double tmpT = f * glm::dot(edge2, q);
    if (tmpT > EPS) {
        t = tmpT;
        return true;
    }
    return false;
}
}

PlanetarySystem::PlanetarySystem() {}

PlanetarySystem::~PlanetarySystem() {}

void PlanetarySystem::init(Scene* scene, WorldSystem* worldSystem) {
    this->scene = scene;
    this->worldSystem = worldSystem;
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

void PlanetarySystem::setDetailedSurfaceData(const std::string& bodyName, const PlanetGenerator::PlanetData& data) {
    detailedSurfaceData[bodyName] = data;
}

double PlanetarySystem::getSurfaceRadiusAtDirection(const CelestialBody* body, const glm::dvec3& planetToPoint) const {
    if (!body) return 0.0;

    if (glm::length(planetToPoint) < 1e-9) {
        return static_cast<double>(body->radius);
    }

    auto it = detailedSurfaceData.find(body->name);
    if (it == detailedSurfaceData.end() || it->second.vertices.empty()) {
        return static_cast<double>(body->radius);
    }

    glm::dvec3 dir = glm::normalize(planetToPoint);
    const auto& data = it->second;

    // Precisión alta: intersección rayo (centro->dir) contra triángulos de superficie
    if (!data.indices.empty() && data.indices.size() % 3 == 0) {
        const glm::dvec3 origin(0.0);
        double closestT = std::numeric_limits<double>::infinity();

        for (size_t i = 0; i + 2 < data.indices.size(); i += 3) {
            const glm::dvec3 v0 = glm::dvec3(data.vertices[data.indices[i]]);
            const glm::dvec3 v1 = glm::dvec3(data.vertices[data.indices[i + 1]]);
            const glm::dvec3 v2 = glm::dvec3(data.vertices[data.indices[i + 2]]);

            double t = 0.0;
            if (rayTriangleIntersect(origin, dir, v0, v1, v2, t)) {
                if (t < closestT) {
                    closestT = t;
                }
            }
        }

        if (std::isfinite(closestT)) {
            return static_cast<double>(body->radius) * closestT;
        }
    }

    // Fallback: aproximación por vértice más alineado
    double bestDot = -std::numeric_limits<double>::infinity();
    double localRadius = 1.0;
    for (const auto& v : data.vertices) {
        glm::dvec3 vn = glm::normalize(glm::dvec3(v));
        double d = glm::dot(vn, dir);
        if (d > bestDot) {
            bestDot = d;
            localRadius = glm::length(glm::dvec3(v));
        }
    }

    return static_cast<double>(body->radius) * localRadius;
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
    
    // Modo seguro: no mutar la Scene del editor por frame desde gameplay.
    // Esto evita invalidar punteros internos de panels/inspector y corrupción de heap.
    // integrateOrbits(dt);
    // updateWorldOrigin();
    // syncSceneWithOrbits();
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
    
    glm::dvec3 playerPos = player->getPosition();
    CelestialBody* closest = nullptr;
    double minDist = std::numeric_limits<double>::max();
    
    for (const auto& bodyName : bodyNames) {
        auto body = findBody(bodyName);
        if (!body || body->name == (star ? star->name : "")) continue;
        
        glm::dvec3 bodyPos = body->worldPos;
        double dist = glm::length(playerPos - bodyPos);
        
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
    glm::dvec3 planetPos = closestPlanet->worldPos;
    glm::dvec3 planetToPlayer = playerPos - planetPos;
    double distToPlanet = glm::length(planetToPlayer);
    
    double localSurfaceRadius = getSurfaceRadiusAtDirection(closestPlanet, planetToPlayer);
    if (distToPlanet < localSurfaceRadius + 100.0) {
        glm::dvec3 n = (distToPlanet > 1e-9)
            ? (planetToPlayer / distToPlanet)
            : glm::dvec3(0.0, 1.0, 0.0);
        glm::dvec3 surfacePos = planetPos + n * (localSurfaceRadius + kPlayerCollisionRadiusKm + kGroundEpsilonKm);
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
        
        // F = G * M / r² (distance está en km -> convertir a metros)
        double distanceMeters = distance * 1000.0;
        double accelerationMps2 = (G * body->mass) / (distanceMeters * distanceMeters);
        double acceleration = accelerationMps2 / 1000.0; // km/s²
        
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
    glm::dvec3 currentVelocity = player->getVelocity();

    // Saneado defensivo contra NaN/Inf
    auto finite3 = [](const glm::dvec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    if (!finite3(playerPos)) {
        playerPos = glm::dvec3(0.0, 6373.0, 0.0);
        player->setPosition(playerPos);
    }
    if (!finite3(currentVelocity)) {
        currentVelocity = glm::dvec3(0.0);
        player->setVelocity(currentVelocity);
    }

    glm::dvec3 gravityDir;
    double gravityMagnitude = calculateGravityAtPosition(playerPos, gravityDir);
    
    // Aplicar gravedad al jugador
    
    // Velocidad terminal realista (50 m/s = 0.05 km/s)
    double currentSpeed = glm::length(currentVelocity);
    
    // La gravedad del jugador debe usar tiempo real; timeScale queda para órbitas/simulación general.
    glm::dvec3 gravityAcceleration = gravityDir * gravityMagnitude * dt;
    
    currentVelocity += gravityAcceleration;
    currentSpeed = glm::length(currentVelocity);
    if (currentSpeed > kTerminalFallSpeedKmS && currentSpeed > 1e-9) {
        currentVelocity = glm::normalize(currentVelocity) * kTerminalFallSpeedKmS;
    }
    
    player->setVelocity(currentVelocity);

    // Integración con subpasos para evitar tunneling
    int substeps = std::clamp((int)std::ceil((glm::length(currentVelocity) * dt) / 0.01), 1, 8);
    double subDt = dt / static_cast<double>(substeps);

    for (int i = 0; i < substeps; ++i) {
        playerPos += currentVelocity * subDt;

        auto closestPlanet = getClosestPlanet();
        if (closestPlanet) {
            glm::dvec3 planetPos = closestPlanet->worldPos;
            glm::dvec3 planetToPlayer = playerPos - planetPos;
            double distToPlanet = glm::length(planetToPlayer);
            double surfaceDistance = getSurfaceRadiusAtDirection(closestPlanet, planetToPlayer)
                                   + kPlayerCollisionRadiusKm
                                   + kGroundEpsilonKm;

            if (distToPlanet < surfaceDistance) {
                glm::dvec3 n = (distToPlanet > 1e-9)
                    ? (planetToPlayer / distToPlanet)
                    : glm::dvec3(0.0, 1.0, 0.0);

                playerPos = planetPos + n * surfaceDistance;

                // Quitar componente hacia adentro
                double inwardSpeed = glm::dot(currentVelocity, -n);
                if (inwardSpeed > 0.0) {
                    currentVelocity += n * inwardSpeed;
                }

                // Fricción tangencial
                glm::dvec3 tangentialVelocity = currentVelocity - n * glm::dot(currentVelocity, n);
                currentVelocity = tangentialVelocity * 0.98 + n * glm::max(0.0, glm::dot(currentVelocity, n));
            }
        }
    }

    player->setPosition(playerPos);
    player->setVelocity(currentVelocity);
}

void PlanetarySystem::setPlayerFlightMode(bool enabled) {
    if (player) {
        player->setFlightMode(enabled);
    }
}

}