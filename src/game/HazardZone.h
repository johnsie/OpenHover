// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_HAZARD_ZONE_H
#define OPENHOVER_HAZARD_ZONE_H

#include "Hovercraft.h"

struct HazardZone
{
    double mX = 0.0;
    double mY = 0.0;
    double mRadius = 1.0;
    double mSpeedLossPerSecond = 0.9;
};

bool ApplyHazardZone(HovercraftState& pState, const HazardZone& pZone, double pSeconds);

#endif