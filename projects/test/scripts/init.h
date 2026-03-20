#pragma once

#include "core/scene.h"
#include "game/character.h"
#include "game/planetary_system.h"
#include <memory>

namespace GameLogic {
    class GameInitializer {
    public:
        static void initializeGame(Haruka::Scene* scene);
    };
}