#ifndef PRIMITIVE_SHAPES_H
#define PRIMITIVE_SHAPES_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "mesh.h"

class PrimitiveShapes
{
public:
    static void createSphere(float radius, int sectors, int stacks, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices);
    static void createSphereLOD(float radius, int sectors, int stacks, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices);
    static void createCubeSphere(float radius, int subdivisions, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices);
    static void createCube(float size, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices);
    static void createCapsule(float radius, float height, int sectors, int stacks, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices);
    static void createPlane(float width, float height, int subdivisions, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices);
    
    // Versiones que devuelven Vertex directamente para Mesh
    static void createSphereVertex(float radius, int sectors, int stacks, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);
    static void createCubeVertex(float size, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);

private:
    PrimitiveShapes() = default;
};
#endif