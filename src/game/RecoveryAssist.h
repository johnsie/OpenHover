// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RECOVERY_ASSIST_H
#define OPENHOVER_RECOVERY_ASSIST_H

#include "Course.h"
#include "Hovercraft.h"
#include "Race.h"

bool RecoverHovercraftToRoute(HovercraftState& pState, const Course& pCourse,
                              const RaceGate& pTarget);

#endif