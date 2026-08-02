#pragma once

#include "tools/math_types.h"

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

}
