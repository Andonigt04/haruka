#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>
#include "octree.h"

namespace Haruka {

struct RigidBody {
    glm::dvec3 position;
    glm::dvec3 velocity;
    glm::dvec3 acceleration;
    double mass;
    double radius;
    bool isKinematic = false;
    std::string name;
};

struct CollisionInfo {
    RigidBody* bodyA;
    RigidBody* bodyB;
    double penetration;
    glm::dvec3 normal;
};

class PhysicsEngine {
public:
    PhysicsEngine();
    ~PhysicsEngine();
    
    void addBody(std::shared_ptr<RigidBody> body);
    void removeBody(const std::string& name);
    std::shared_ptr<RigidBody> getBody(const std::string& name);
    
    void update(double deltaTime);
    void setGravity(glm::dvec3 g) { gravity = g; }
    glm::dvec3 getGravity() const { return gravity; }
    
    const std::vector<CollisionInfo>& getCollisions() const { return collisions; }
    void initOctree(glm::dvec3 center, double size) {
        octree = std::make_unique<Octree>(center, size);
    }

private:
    std::vector<std::shared_ptr<RigidBody>> bodies;
    std::vector<CollisionInfo> collisions;
    glm::dvec3 gravity{0.0, -9.81, 0.0};
    std::unique_ptr<Octree> octree;
    
    void integrateForces(double dt);
    void detectCollisions();
    void resolveCollisions();
    void broadPhaseAABB();
};

}