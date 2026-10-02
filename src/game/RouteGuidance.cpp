// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RouteGuidance.h"

#include <cmath>

double GetGateDirection(const HovercraftState& pState, const RaceGate& pGate)
{
    return std::atan2(pGate.mY - pState.mY, pGate.mX - pState.mX);
}

bool IsHeadingAwayFromGate(const HovercraftState& pState, const RaceGate& pGate,
                           double pDirectionThreshold)
{
    const double deltaX = pGate.mX - pState.mX;
    const double deltaY = pGate.mY - pState.mY;
    const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
    if (distanceSquared <= pGate.mRadius * pGate.mRadius)
        return false;

    if (std::fabs(pState.mSpeed) < 1.0)
        return false;

    const double distance = std::sqrt(distanceSquared);
    const double travelSign = pState.mSpeed < 0.0 ? -1.0 : 1.0;
    const double directionX = std::cos(pState.mTravelHeading) * travelSign;
    const double directionY = std::sin(pState.mTravelHeading) * travelSign;
    const double gateDirectionX = deltaX / distance;
    const double gateDirectionY = deltaY / distance;
    return directionX * gateDirectionX + directionY * gateDirectionY < pDirectionThreshold;
}