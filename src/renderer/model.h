#ifndef MODEL_H
#define MODEL_H

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include <string>

#include "mesh.h"
#include "shader.h"

unsigned int TextureFromFile(const char *path, const std::string &directory, const aiScene *scene);

class Model
{
public:
    Model(const std::string &path) { loadModel(path); }
    
    void Draw(Shader &shader);

    int getVertexCount() const {
        int total = 0;
        for (const auto& mesh : meshes) total += mesh.getVertexCount();
        return total;
    }
    int getTriangleCount() const {
        int total = 0;
        for (const auto& mesh : meshes) total += mesh.getTriangleCount();
        return total;
    }
private:
    std::vector<Mesh> meshes;
    std::string directory;
    std::vector<MeshTexture> textures_loaded;

    Assimp::Importer importer;
    const aiScene* scene = nullptr;

    void loadModel(std::string const &path);
    void processNode(aiNode *node, const aiScene *scene);
    Mesh processMesh(aiMesh *mesh, const aiScene *scene);
    std::vector<MeshTexture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName);
};
#endif