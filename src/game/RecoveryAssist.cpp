// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RecoveryAssist.h"

#include <cmath>

bool RecoverHovercraftToRoute(HovercraftState& pState, const Course& pCourse,
                              const RaceGate& pTarget)
{
    if (pCourse.IsOnRoad(pState.mX, pState.mY))
        return false;

    double routeX = pState.mX;
    double routeY = pState.mY;
    pCourse.ProjectToRoad(pState.mX, pState.mY, routeX, routeY);
    pState.mX = routeX;
    pState.mY = routeY;
    pState.mSpeed = 0.0;
    pState.mVerticalSpeed = 0.0;
    pState.mBoosting = false;
    if (pTarget.mX != routeX || pTarget.mY != routeY)
        pState.mHeading = std::atan2(pTarget.mY - routeY, pTarget.mX - routeX);
    pState.mTravelHeading = pState.mHeading;
    return true;
}