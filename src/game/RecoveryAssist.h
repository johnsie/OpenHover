// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RECOVERY_ASSIST_H
#define OPENHOVER_RECOVERY_ASSIST_H

#include "Course.h"
#include "Hovercraft.h"
#include "Race.h"

// Puts a craft that has left the road back on it, facing along the route. Returns false (and
// changes nothing) when the craft is already on the road, unless pForce is set: that is for an AI
// rival stuck against a wall, which is on the road but going nowhere.
bool RecoverHovercraftToRoute(HovercraftState& pState, const Course& pCourse,
                              const RaceGate& pTarget, bool pForce = false);

#endif