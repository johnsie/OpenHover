// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RacerCollision.h"

#include <cmath>

namespace
{
const double kRacerClearanceHeight = 0.35;
}

bool ResolveRacerCollision(HovercraftState& pFirst, HovercraftState& pSecond, double pRadius)
{
    if (pRadius <= 0.0)
        return false;

    if (std::fabs(pFirst.mHeight - pSecond.mHeight) >= kRacerClearanceHeight)
        return false;

    double deltaX = pSecond.mX - pFirst.mX;
    double deltaY = pSecond.mY - pFirst.mY;
    const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
    const double minimumDistance = pRadius * 2.0;
    if (distanceSquared >= minimumDistance * minimumDistance)
        return false;

    const double distance = std::sqrt(distanceSquared);
    if (distance == 0.0)
    {
        deltaX = 1.0;
        deltaY = 0.0;
    }
    else
    {
        deltaX /= distance;
        deltaY /= distance;
    }

    const double separation = (minimumDistance - distance) * 0.5;
    pFirst.mX -= deltaX * separation;
    pFirst.mY -= deltaY * separation;
    pSecond.mX += deltaX * separation;
    pSecond.mY += deltaY * separation;
    pFirst.mSpeed *= 0.7;
    pSecond.mSpeed *= 0.7;
    return true;
}