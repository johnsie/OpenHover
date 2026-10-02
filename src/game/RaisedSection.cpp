// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RaisedSection.h"

#include <cmath>

bool ResolveRaisedSectionCollision(HovercraftState& pState, const RaisedSection& pSection,
                                   double pCraftRadius, double pRestitution)
{
    if (pSection.mHalfLength <= 0.0 || pSection.mHalfWidth <= 0.0
        || pState.mHeight >= pSection.mClearHeight)
    {
        return false;
    }

    const double forwardX = std::cos(pSection.mHeading);
    const double forwardY = std::sin(pSection.mHeading);
    const double sideX = -forwardY;
    const double sideY = forwardX;
    const double deltaX = pState.mX - pSection.mX;
    const double deltaY = pState.mY - pSection.mY;
    const double localX = deltaX * forwardX + deltaY * forwardY;
    const double localY = deltaX * sideX + deltaY * sideY;
    const double radius = pCraftRadius > 0.0 ? pCraftRadius : 0.0;
    const double limitX = pSection.mHalfLength + radius;
    const double limitY = pSection.mHalfWidth + radius;
    if (std::fabs(localX) > limitX || std::fabs(localY) > limitY)
        return false;

    double impactLocalX = localX;
    double impactLocalY = localY;
    bool resolvedAtContact = false;
    if (pState.mHasPreviousPosition)
    {
        const double previousDeltaX = pState.mPreviousX - pSection.mX;
        const double previousDeltaY = pState.mPreviousY - pSection.mY;
        const double previousLocalX = previousDeltaX * forwardX + previousDeltaY * forwardY;
        const double previousLocalY = previousDeltaX * sideX + previousDeltaY * sideY;
        if (std::fabs(previousLocalX) > limitX || std::fabs(previousLocalY) > limitY)
        {
            double lower = 0.0;
            double upper = 1.0;
            for (int iteration = 0; iteration < 12; ++iteration)
            {
                const double middle = (lower + upper) * 0.5;
                const double sampleX = previousLocalX + (localX - previousLocalX) * middle;
                const double sampleY = previousLocalY + (localY - previousLocalY) * middle;
                if (std::fabs(sampleX) > limitX || std::fabs(sampleY) > limitY)
                    lower = middle;
                else
                    upper = middle;
            }
            impactLocalX = previousLocalX + (localX - previousLocalX) * upper;
            impactLocalY = previousLocalY + (localY - previousLocalY) * upper;
            resolvedAtContact = true;
        }
    }

    const bool exitAlongLength = limitX - std::fabs(impactLocalX) < limitY - std::fabs(impactLocalY);
    const double normalX = exitAlongLength ? (impactLocalX < 0.0 ? -forwardX : forwardX)
                                           : (impactLocalY < 0.0 ? -sideX : sideX);
    const double normalY = exitAlongLength ? (impactLocalX < 0.0 ? -forwardY : forwardY)
                                           : (impactLocalY < 0.0 ? -sideY : sideY);
    pState.mX = pSection.mX + forwardX * impactLocalX + sideX * impactLocalY;
    pState.mY = pSection.mY + forwardY * impactLocalX + sideY * impactLocalY;
    if (!resolvedAtContact)
    {
        const double penetration = exitAlongLength ? limitX - std::fabs(impactLocalX)
                                                    : limitY - std::fabs(impactLocalY);
        pState.mX += normalX * penetration;
        pState.mY += normalY * penetration;
    }

    const double velocityX = std::cos(pState.mTravelHeading) * pState.mSpeed;
    const double velocityY = std::sin(pState.mTravelHeading) * pState.mSpeed;
    const double faceSpeed = velocityX * normalX + velocityY * normalY;
    if (faceSpeed >= 0.0)
        return true;

    const double restitution = pRestitution < 0.0 ? 0.0 : (pRestitution > 1.0 ? 1.0 : pRestitution);
    const double bouncedX = velocityX - (1.0 + restitution) * faceSpeed * normalX;
    const double bouncedY = velocityY - (1.0 + restitution) * faceSpeed * normalY;
    pState.mSpeed = std::sqrt(bouncedX * bouncedX + bouncedY * bouncedY);
    pState.mTravelHeading = std::atan2(bouncedY, bouncedX);
    return true;
}