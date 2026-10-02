// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_WALL_COLLISION_H
#define OPENHOVER_WALL_COLLISION_H

#include "Course.h"
#include "Hovercraft.h"

bool ResolveCourseWallCollision(HovercraftState& pState, const Course& pCourse,
                                double pCraftRadius = 0.9, double pRestitution = 1.05);

#endif