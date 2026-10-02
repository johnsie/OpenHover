// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RouteGuidance.h"

#include <cmath>

bool IsHeadingAwayFromGate(const HovercraftState& pState, const RaceGate& pGate,
                           double pDirectionThreshold)
{
    const double deltaX = pGate.mX - pState.mX;
    const double deltaY = pGate.mY - pState.mY;
    const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
    if (distanceSquared <= pGate.mRadius * pGate.mRadius)
        return false;

    const double distance = std::sqrt(distanceSquared);
    const double directionX = std::cos(pState.mHeading);
    const double directionY = std::sin(pState.mHeading);
    const double gateDirectionX = deltaX / distance;
    const double gateDirectionY = deltaY / distance;
    return directionX * gateDirectionX + directionY * gateDirectionY < pDirectionThreshold;
}