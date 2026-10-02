// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RACER_COLLISION_H
#define OPENHOVER_RACER_COLLISION_H

#include "Hovercraft.h"

bool ResolveRacerCollision(HovercraftState& pFirst, HovercraftState& pSecond,
                           double pRadius = 1.25);

#endif