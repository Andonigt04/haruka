#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Haruka {

struct AABB {
    glm::dvec3 min;
    glm::dvec3 max;
    
    AABB() : min(0), max(0) {}
    AABB(glm::dvec3 m, glm::dvec3 M) : min(m), max(M) {}
    
    bool contains(glm::dvec3 point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }
    
    bool intersects(const AABB& other) const {
        return !(max.x < other.min.x || min.x > other.max.x ||
                 max.y < other.min.y || min.y > other.max.y ||
                 max.z < other.min.z || min.z > other.max.z);
    }
};

struct RigidBody;

class OctreeNode {
public:
    OctreeNode(AABB bounds, int depth = 0);
    ~OctreeNode();
    
    void insert(std::shared_ptr<RigidBody> body);
    void remove(std::shared_ptr<RigidBody> body);
    void getCollidingBodies(const AABB& region, std::vector<std::shared_ptr<RigidBody>>& result);
    
    bool isLeaf() const { return children[0] == nullptr; }
    int getDepth() const { return depth; }
    const AABB& getBounds() const { return bounds; }

private:
    AABB bounds;
    int depth;
    std::vector<std::shared_ptr<RigidBody>> bodies;
    std::unique_ptr<OctreeNode> children[8];
    
    static const int MAX_BODIES = 4;
    static const int MAX_DEPTH = 10;
    
    void subdivide();
    int getOctant(glm::dvec3 pos) const;
};

class Octree {
public:
    Octree(glm::dvec3 center, double size);
    ~Octree();
    
    void insert(std::shared_ptr<RigidBody> body);
    void remove(std::shared_ptr<RigidBody> body);
    void getNearbodies(std::shared_ptr<RigidBody> body, std::vector<std::shared_ptr<RigidBody>>& result);
    void rebuild();
    
    const OctreeNode* getRoot() const { return root.get(); }

private:
    std::unique_ptr<OctreeNode> root;
};

}