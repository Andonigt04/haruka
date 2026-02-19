#include "world_system.h"

WorldSystem::WorldSystem() : worldOrigin(0.0, 0.0, 0.0) {
    // Default LOD distances (in km)
    lodDistances[0] = 1000.0f;        // < 1,000 km = LOD 0
    lodDistances[1] = 10000.0f;       // < 10,000 km = LOD 1
    lodDistances[2] = 100000.0f;      // < 100,000 km = LOD 2
    lodDistances[3] = 1000000.0f;     // < 1 million km = LOD 3
}

WorldSystem::~WorldSystem() {}

Haruka::LocalPos WorldSystem::toLocal(Haruka::WorldPos objectPos, Haruka::WorldPos referencePos) {
    glm::dvec3 diff = objectPos - referencePos;
    return Haruka::LocalPos(diff);
}

void WorldSystem::updateOrigin(Haruka::WorldPos newOrigin) {
    glm::dvec3 offset = newOrigin - worldOrigin;
    worldOrigin = newOrigin;

    for (auto& body : celestialBodies) {
        body.worldPos -= offset;
    }
}

void WorldSystem::updateLocalPositions(Haruka::WorldPos cameraWorldPos) {
    for (auto& body : celestialBodies) {
        body.localPos = toLocal(body.worldPos, cameraWorldPos);
    }
}

void WorldSystem::addBody(const CelestialBody& body) {
    celestialBodies.push_back(body);
}

void WorldSystem::removeBody(const std::string& name) {
    celestialBodies.erase(
        std::remove_if(celestialBodies.begin(), celestialBodies.end(),
            [&name](const CelestialBody& b) { return b.name == name; }),
        celestialBodies.end()
    );
}

CelestialBody* WorldSystem::findBody(const std::string& name) {
    for (auto& body : celestialBodies) {
        if (body.name == name) return &body;
    }
    return nullptr;
}

const std::vector<CelestialBody>& WorldSystem::getBodies() const {
    return celestialBodies;
}

size_t WorldSystem::getBodyCount() const {
    return celestialBodies.size();
}

Haruka::WorldPos WorldSystem::getOrigin() const {
    return worldOrigin;
}

void WorldSystem::initComputeShaders() {
    // Placeholder: CPU culling, no compute shader needed
}

void WorldSystem::setLODDistances(float lod0, float lod1, float lod2, float lod3) {
    lodDistances[0] = lod0;
    lodDistances[1] = lod1;
    lodDistances[2] = lod2;
    lodDistances[3] = lod3;
}

void WorldSystem::frustumCull(Haruka::WorldPos cameraPos, const glm::mat4& viewProj, float frustumDistance) {
    for (auto& body : celestialBodies) {
        float distance = static_cast<float>(glm::length(body.worldPos - cameraPos));
        
        if (distance < frustumDistance + body.radius) {
            body.visible = 1;
            
            if (distance < lodDistances[0]) {
                body.lodLevel = 0;
            } else if (distance < lodDistances[1]) {
                body.lodLevel = 1;
            } else if (distance < lodDistances[2]) {
                body.lodLevel = 2;
            } else if (distance < lodDistances[3]) {
                body.lodLevel = 3;
            } else {
                body.visible = 0;
            }
        } else {
            body.visible = 0;
        }
    }
}