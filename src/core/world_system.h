#ifndef WORLD_SYSTEM_H
#define WORLD_SYSTEM_H

#include "math_types.h"

class WorldSystem {
public:
    static Haruka::LocalPos to_local(Haruka::WorldPos object_pos, Haruka::WorldPos camera_pos) {
        return Haruka::LocalPos(object_pos - camera_pos);
    }
};

#endif