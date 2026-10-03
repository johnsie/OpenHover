// SPDX-License-Identifier: MIT OR Apache-2.0
#include "HazardZone.h"

#include <cmath>

namespace
{
const double kHazardClearHeight = 1.45;
}

bool ApplyHazardZone(HovercraftState& pState, const HazardZone& pZone, double pSeconds)
{
    if (pSeconds <= 0.0 || pZone.mRadius <= 0.0 || pZone.mSpeedLossPerSecond <= 0.0)
        return false;
    const double deltaX = pState.mX - pZone.mX;
    const double deltaY = pState.mY - pZone.mY;
    if (pState.mHeight - pState.mGroundHeight >= kHazardClearHeight
        || deltaX * deltaX + deltaY * deltaY > pZone.mRadius * pZone.mRadius)
        return false;
    pState.mSpeed *= std::exp(-pZone.mSpeedLossPerSecond * pSeconds);
    return true;
}