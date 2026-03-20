#pragma once

#include "scene.h"
#include <string>

namespace Haruka {

void savePrefab(const SceneObject& obj, const std::string& path);
SceneObject loadPrefab(const std::string& path);

}