#pragma once

#include "core/camera.h"
#include "game/planet_generator.h"

extern Camera* g_gameCamera;

// Geometría procedural del planeta disponible globalmente para el renderer
extern Haruka::PlanetGenerator::PlanetData g_earthTerrainData;