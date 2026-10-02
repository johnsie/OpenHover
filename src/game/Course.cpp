// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Course.h"

#include <cmath>
#include <limits>

Course::Course(const std::vector<RaceGate>& pWaypoints, double pHalfWidth)
    : mWaypoints(pWaypoints),
      mHalfWidth(pHalfWidth > 0.0 ? pHalfWidth : 1.0)
{
    double travelled = 0.0;
    for (std::size_t index = 0; index < mWaypoints.size(); ++index)
    {
        const RaceGate& next = mWaypoints[(index + 1) % mWaypoints.size()];
        mSegmentStart.push_back(travelled);
        travelled += std::hypot(next.mX - mWaypoints[index].mX, next.mY - mWaypoints[index].mY);
    }
    mRouteLength = travelled;
}

bool Course::IsOnRoad(double pX, double pY) const
{
    double nearestX = pX;
    double nearestY = pY;
    ProjectToRoad(pX, pY, nearestX, nearestY);
    const double deltaX = pX - nearestX;
    const double deltaY = pY - nearestY;
    return deltaX * deltaX + deltaY * deltaY <= mHalfWidth * mHalfWidth;
}

void Course::ProjectToRoad(double pX, double pY, double& pOutX, double& pOutY) const
{
    pOutX = pX;
    pOutY = pY;
    if (mWaypoints.size() < 2)
        return;

    double nearestDistanceSquared = std::numeric_limits<double>::max();
    for (int index = 0; index < static_cast<int>(mWaypoints.size()); ++index)
    {
        const RaceGate& start = mWaypoints[index];
        const RaceGate& end = mWaypoints[(index + 1) % mWaypoints.size()];
        const double directionX = end.mX - start.mX;
        const double directionY = end.mY - start.mY;
        const double lengthSquared = directionX * directionX + directionY * directionY;
        if (lengthSquared == 0.0)
            continue;

        double progress = ((pX - start.mX) * directionX + (pY - start.mY) * directionY)
            / lengthSquared;
        progress = progress < 0.0 ? 0.0 : (progress > 1.0 ? 1.0 : progress);
        const double candidateX = start.mX + directionX * progress;
        const double candidateY = start.mY + directionY * progress;
        const double deltaX = pX - candidateX;
        const double deltaY = pY - candidateY;
        const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
        if (distanceSquared < nearestDistanceSquared)
        {
            nearestDistanceSquared = distanceSquared;
            pOutX = candidateX;
            pOutY = candidateY;
        }
    }
}
double Course::RouteHeadingNear(double pX, double pY) const
{
    double heading = 0.0;
    double nearestDistanceSquared = std::numeric_limits<double>::max();
    for (int index = 0; index < static_cast<int>(mWaypoints.size()); ++index)
    {
        const RaceGate& start = mWaypoints[index];
        const RaceGate& end = mWaypoints[(index + 1) % mWaypoints.size()];
        const double directionX = end.mX - start.mX;
        const double directionY = end.mY - start.mY;
        const double lengthSquared = directionX * directionX + directionY * directionY;
        if (lengthSquared == 0.0)
            continue;
        double progress = ((pX - start.mX) * directionX + (pY - start.mY) * directionY) / lengthSquared;
        progress = progress < 0.0 ? 0.0 : (progress > 1.0 ? 1.0 : progress);
        const double deltaX = pX - (start.mX + directionX * progress);
        const double deltaY = pY - (start.mY + directionY * progress);
        const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
        if (distanceSquared < nearestDistanceSquared)
        {
            nearestDistanceSquared = distanceSquared;
            heading = std::atan2(directionY, directionX);
        }
    }
    return heading;
}

double Course::RouteDistanceNear(double pX, double pY, double pHintDistance, double pWindow) const
{
    if (mWaypoints.size() < 2 || mRouteLength <= 0.0)
        return 0.0;
    double bestDistanceSquared = std::numeric_limits<double>::max();
    double bestRouteDistance = 0.0;
    for (int index = 0; index < static_cast<int>(mWaypoints.size()); ++index)
    {
        const RaceGate& start = mWaypoints[index];
        const RaceGate& end = mWaypoints[(index + 1) % mWaypoints.size()];
        const double directionX = end.mX - start.mX;
        const double directionY = end.mY - start.mY;
        const double lengthSquared = directionX * directionX + directionY * directionY;
        if (lengthSquared == 0.0)
            continue;
        const double segmentLength = std::sqrt(lengthSquared);
        if (pHintDistance >= 0.0)
        {
            // Skip segments that lie wholly outside the window around the hint, measured round the
            // lap in both directions.
            const double segmentMiddle = mSegmentStart[index] + segmentLength * 0.5;
            double gap = std::fabs(segmentMiddle - pHintDistance);
            gap = std::fmin(gap, mRouteLength - gap);
            if (gap > pWindow + segmentLength * 0.5)
                continue;
        }
        double progress = ((pX - start.mX) * directionX + (pY - start.mY) * directionY) / lengthSquared;
        progress = progress < 0.0 ? 0.0 : (progress > 1.0 ? 1.0 : progress);
        const double deltaX = pX - (start.mX + directionX * progress);
        const double deltaY = pY - (start.mY + directionY * progress);
        const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
        if (distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            bestRouteDistance = mSegmentStart[index] + segmentLength * progress;
        }
    }
    return std::fmod(bestRouteDistance, mRouteLength);
}
