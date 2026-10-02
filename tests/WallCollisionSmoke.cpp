// SPDX-License-Identifier: MIT OR Apache-2.0
#include "WallCollision.h"

#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    std::vector<RaceGate> route = {{0.0, 0.0, 1.0}, {10.0, 0.0, 1.0},
                                   {10.0, 10.0, 1.0}, {0.0, 10.0, 1.0}};
    Course course(route, 2.0);
    HovercraftState state;
    state.mX = 5.0;
    state.mY = -1.8;
    state.mHeading = -1.5707963267948966;
    state.mTravelHeading = state.mHeading;
    state.mSpeed = 10.0;
    if (!ResolveCourseWallCollision(state, course) || std::fabs(state.mY + 1.1) > 0.0001
        || std::fabs(state.mSpeed - 10.5) > 0.0001 || state.mHeading >= 0.0
        || state.mTravelHeading <= 0.0)
    {
        std::cerr << "wall collision did not rebound the hovercraft\n";
        return 1;
    }

    if (ResolveCourseWallCollision(state, course))
    {
        std::cerr << "legal hovercraft collided with a wall\n";
        return 1;
    }

    state = HovercraftState();
    state.mPreviousX = 5.0;
    state.mPreviousY = -0.8;
    state.mHasPreviousPosition = true;
    state.mX = 5.0;
    state.mY = -8.0;
    state.mHeading = -1.5707963267948966;
    state.mTravelHeading = state.mHeading;
    state.mSpeed = 10.0;
    if (!ResolveCourseWallCollision(state, course) || std::fabs(state.mY + 1.1) > 0.01)
    {
        std::cerr << "wall collision did not resolve at first contact\n";
        return 1;
    }

    state = HovercraftState();
    state.mX = 5.0;
    state.mY = 1.8;
    state.mHeading = 1.5707963267948966;
    state.mTravelHeading = state.mHeading;
    state.mSpeed = 10.0;
    if (!ResolveCourseWallCollision(state, course) || std::fabs(state.mY - 1.1) > 0.0001
        || std::fabs(state.mSpeed - 10.5) > 0.0001 || state.mHeading <= 0.0
        || state.mTravelHeading >= 0.0)
    {
        std::cerr << "inner wall did not rebound the hovercraft\n";
        return 1;
    }
    return 0;
}