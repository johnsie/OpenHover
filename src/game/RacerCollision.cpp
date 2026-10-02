// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RacerCollision.h"

#include <cmath>

namespace
{
const double kRacerClearanceHeight = 0.35;
const double kRacerRestitution = 0.35;

void ApplyVelocity(HovercraftState& pState, double pVelocityX, double pVelocityY)
{
    pState.mSpeed = std::sqrt(pVelocityX * pVelocityX + pVelocityY * pVelocityY);
    if (pState.mSpeed > 0.0)
        pState.mTravelHeading = std::atan2(pVelocityY, pVelocityX);
}
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

    const double firstVelocityX = std::cos(pFirst.mTravelHeading) * pFirst.mSpeed;
    const double firstVelocityY = std::sin(pFirst.mTravelHeading) * pFirst.mSpeed;
    const double secondVelocityX = std::cos(pSecond.mTravelHeading) * pSecond.mSpeed;
    const double secondVelocityY = std::sin(pSecond.mTravelHeading) * pSecond.mSpeed;
    const double relativeNormalSpeed = (secondVelocityX - firstVelocityX) * deltaX
        + (secondVelocityY - firstVelocityY) * deltaY;
    if (relativeNormalSpeed < 0.0)
    {
        const double impulse = -(1.0 + kRacerRestitution) * relativeNormalSpeed * 0.5;
        ApplyVelocity(pFirst, firstVelocityX - impulse * deltaX, firstVelocityY - impulse * deltaY);
        ApplyVelocity(pSecond, secondVelocityX + impulse * deltaX, secondVelocityY + impulse * deltaY);
    }
    return true;
}