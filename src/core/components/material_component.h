#pragma once
#include <string>
#include <map>
#include <glm/glm.hpp>
#include <memory>
#include "../component.h"

namespace Haruka {

class MaterialComponent : public Component {
public:
    MaterialComponent();
    ~MaterialComponent() override = default;
    
    std::string getType() const override { return "MaterialComponent"; }
    
    // Identificador
    std::string name = "DefaultMaterial";
    std::string shaderPath = "shaders/pbr.frag";
    
    // Texturas
    std::map<std::string, std::string> textures;
    
    // Propiedades PBR
    glm::vec3 albedo = glm::vec3(0.8f);
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    
    glm::vec3 emission = glm::vec3(0.0f);
    
    // Serialización
    nlohmann::json toJSON() const;
    void fromJSON(const nlohmann::json& j);
};

}