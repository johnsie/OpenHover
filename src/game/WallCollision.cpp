// SPDX-License-Identifier: MIT OR Apache-2.0
#include "WallCollision.h"

#include <cmath>

bool ResolveCourseWallCollision(HovercraftState& pState, const Course& pCourse,
                                double pCraftRadius, double pRestitution)
{
    double roadX = pState.mX;
    double roadY = pState.mY;
    pCourse.ProjectToRoad(pState.mX, pState.mY, roadX, roadY);
    double normalX = pState.mX - roadX;
    double normalY = pState.mY - roadY;
    const double distance = std::sqrt(normalX * normalX + normalY * normalY);
    const double craftRadius = pCraftRadius > 0.0 ? pCraftRadius : 0.0;
    const double legalOffset = pCourse.HalfWidth() > craftRadius
        ? pCourse.HalfWidth() - craftRadius : 0.0;
    if (distance <= legalOffset || distance == 0.0)
        return false;

    normalX /= distance;
    normalY /= distance;
    if (pState.mHasPreviousPosition)
    {
        double previousRoadX = pState.mPreviousX;
        double previousRoadY = pState.mPreviousY;
        pCourse.ProjectToRoad(pState.mPreviousX, pState.mPreviousY, previousRoadX, previousRoadY);
        const double previousDistance = std::sqrt((pState.mPreviousX - previousRoadX)
            * (pState.mPreviousX - previousRoadX) + (pState.mPreviousY - previousRoadY)
            * (pState.mPreviousY - previousRoadY));
        if (previousDistance <= legalOffset)
        {
            double lower = 0.0;
            double upper = 1.0;
            for (int iteration = 0; iteration < 12; ++iteration)
            {
                const double middle = (lower + upper) * 0.5;
                const double sampleX = pState.mPreviousX + (pState.mX - pState.mPreviousX) * middle;
                const double sampleY = pState.mPreviousY + (pState.mY - pState.mPreviousY) * middle;
                double sampleRoadX = sampleX;
                double sampleRoadY = sampleY;
                pCourse.ProjectToRoad(sampleX, sampleY, sampleRoadX, sampleRoadY);
                const double sampleDistance = std::sqrt((sampleX - sampleRoadX) * (sampleX - sampleRoadX)
                    + (sampleY - sampleRoadY) * (sampleY - sampleRoadY));
                if (sampleDistance <= legalOffset)
                    lower = middle;
                else
                    upper = middle;
            }
            pState.mX = pState.mPreviousX + (pState.mX - pState.mPreviousX) * lower;
            pState.mY = pState.mPreviousY + (pState.mY - pState.mPreviousY) * lower;
        }
        else
        {
            pState.mX = roadX + normalX * legalOffset;
            pState.mY = roadY + normalY * legalOffset;
        }
    }
    else
    {
        pState.mX = roadX + normalX * legalOffset;
        pState.mY = roadY + normalY * legalOffset;
    }

    const double velocityX = std::cos(pState.mTravelHeading) * pState.mSpeed;
    const double velocityY = std::sin(pState.mTravelHeading) * pState.mSpeed;
    const double outwardSpeed = velocityX * normalX + velocityY * normalY;
    if (outwardSpeed <= 0.0)
        return true;

    const double restitution = pRestitution < 0.0 ? 0.0 : (pRestitution > 1.25 ? 1.25 : pRestitution);
    const double bouncedX = velocityX - (1.0 + restitution) * outwardSpeed * normalX;
    const double bouncedY = velocityY - (1.0 + restitution) * outwardSpeed * normalY;
    pState.mSpeed = std::sqrt(bouncedX * bouncedX + bouncedY * bouncedY);
    if (pState.mSpeed > 0.0)
        pState.mTravelHeading = std::atan2(bouncedY, bouncedX);
    return true;
}