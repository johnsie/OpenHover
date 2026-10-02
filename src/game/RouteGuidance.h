// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_ROUTE_GUIDANCE_H
#define OPENHOVER_ROUTE_GUIDANCE_H

#include "Hovercraft.h"
#include "Race.h"

double GetGateDirection(const HovercraftState& pState, const RaceGate& pGate);

bool IsHeadingAwayFromGate(const HovercraftState& pState, const RaceGate& pGate,
                           double pDirectionThreshold = -0.25);

#endif