// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RecoveryAssist.h"

#include <cmath>

bool RecoverHovercraftToRoute(HovercraftState& pState, const Course& pCourse,
                              const RaceGate& pTarget, bool pForce, const GroundProfile* pGround)
{
    const bool trapped = pState.mInPit && pGround != nullptr;
    if (!pForce && !trapped && pCourse.IsOnRoad(pState.mX, pState.mY))
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
    if (trapped)
    {
        // Back along the road to the higher ground the craft fell from, and a good run-up beyond.
        const double backX = -std::cos(pState.mHeading);
        const double backY = -std::sin(pState.mHeading);
        for (double distance = 2.0; distance <= 120.0; distance += 2.0)
        {
            if (pGround->HeightAt(routeX + backX * distance, routeY + backY * distance) > pState.mGroundHeight + 0.5)
            {
                double x = routeX + backX * (distance + 40.0);
                double y = routeY + backY * (distance + 40.0);
                double roadX = x;
                double roadY = y;
                pCourse.ProjectToRoad(x, y, roadX, roadY);
                pState.mX = roadX;
                pState.mY = roadY;
                pState.mHeading = pCourse.RouteHeadingNear(roadX, roadY);
                pState.mTravelHeading = pState.mHeading;
                break;
            }
        }
        pState.mGroundHeight = pGround->HeightAt(pState.mX, pState.mY);
        pState.mHeight = pState.mGroundHeight + HovercraftTuning().mHoverHeight;
        pState.mInPit = false;
    }
    return true;
}