// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RecoveryAssist.h"

#include <cmath>

bool RecoverHovercraftToRoute(HovercraftState& pState, const Course& pCourse,
                              const RaceGate& pTarget, bool pForce)
{
    if (!pForce && pCourse.IsOnRoad(pState.mX, pState.mY))
        return false;

    double routeX = pState.mX;
    double routeY = pState.mY;
    pCourse.ProjectToRoad(pState.mX, pState.mY, routeX, routeY);
    pState.mX = routeX;
    pState.mY = routeY;
    pState.mSpeed = 0.0;
    pState.mVerticalSpeed = 0.0;
    pState.mBoosting = false;
    // Face along the road, not straight at the next gate: on a winding course the straight line
    // to a distant gate can point into a wall or back the way the player came.
    if (pCourse.HalfWidth() > 0.0)
        pState.mHeading = pCourse.RouteHeadingNear(routeX, routeY);
    else if (pTarget.mX != routeX || pTarget.mY != routeY)
        pState.mHeading = std::atan2(pTarget.mY - routeY, pTarget.mX - routeX);
    pState.mTravelHeading = pState.mHeading;
    return true;
}