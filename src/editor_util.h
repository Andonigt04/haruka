#pragma once

#include "tools/math_types.h"
#include "core/scene/scene_manager.h"   // SceneObject: las capas son un dato del objeto

#include <string>
#include <vector>

/**
 * @brief Editor-side helpers for the engine rotation convention.
 *
 * `Haruka::Rotation` is `glm::dquat`, but the engine's transform builder
 * (`getTransformMatrix` in application_assets.cpp) reads `rotation.x/y/z` as
 * Euler angles in DEGREES (the w component is ignored unless `useOrientation`
 * is set). These helpers let the editor keep working with Euler degrees while
 * staying compatible with that convention.
 */
namespace EditorUtil {

inline Haruka::Rotation eulerToRotation(const glm::dvec3& eulerDeg) {
    return Haruka::Rotation(1.0, eulerDeg.x, eulerDeg.y, eulerDeg.z);
}

inline glm::dvec3 rotationToEuler(const Haruka::Rotation& r) {
    return glm::dvec3(r.x, r.y, r.z);
}

inline glm::mat4 composeLocalTransform(const glm::dvec3& position, const glm::dvec3& rotation, const glm::dvec3& scale) {
    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, glm::vec3(position));
    transform = glm::rotate(transform, glm::radians((float)rotation.x), glm::vec3(1, 0, 0));
    transform = glm::rotate(transform, glm::radians((float)rotation.y), glm::vec3(0, 1, 0));
    transform = glm::rotate(transform, glm::radians((float)rotation.z), glm::vec3(0, 0, 1));
    transform = glm::scale(transform, glm::vec3(scale));
    return transform;
}

/**
 * @brief CAPAS del editor: una etiqueta persistente por objeto (`properties["layer"]`).
 *
 * ⚠️ Vivían como estáticos dentro de `ObjectsPanel`, y al quitar ese panel se habrían llevado por
 * delante el selector de capa del INSPECTOR, que es su otro usuario. Una capa no es cosa de un
 * panel: es un dato del objeto que se serializa con la escena, así que vive aquí y sobrevive a
 * cualquier panel que la enseñe.
 */
inline const std::vector<std::string>& defaultLayers() {
    static const std::vector<std::string> s = {
        "Default", "Props", "Trees", "Monsters", "Spawns", "Characters", "Planets", "Lights"
    };
    return s;
}

inline std::string objectLayer(const Haruka::SceneObject& obj) {
    if (obj.properties.is_object() && obj.properties.contains("layer") &&
        obj.properties["layer"].is_string()) {
        return obj.properties["layer"].get<std::string>();
    }
    return "Default";
}

inline void setObjectLayer(Haruka::SceneObject& obj, const std::string& layer) {
    if (!obj.properties.is_object()) obj.properties = nlohmann::json::object();
    obj.properties["layer"] = layer;
}

}
