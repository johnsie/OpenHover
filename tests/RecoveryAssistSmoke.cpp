// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RecoveryAssist.h"

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
    state.mY = -4.0;
    state.mSpeed = 12.0;
    state.mVerticalSpeed = 3.0;
    state.mTravelHeading = 1.0;
    state.mBoosting = true;
    if (!RecoverHovercraftToRoute(state, course, {10.0, 0.0, 1.0})
        || std::fabs(state.mX - 5.0) > 0.0001 || std::fabs(state.mY) > 0.0001
        || state.mSpeed != 0.0 || state.mVerticalSpeed != 0.0 || state.mBoosting
        || std::fabs(state.mHeading) > 0.0001 || std::fabs(state.mTravelHeading) > 0.0001)
    {
        std::cerr << "route recovery did not restore a safe state\n";
        return 1;
    }

    if (RecoverHovercraftToRoute(state, course, {10.0, 0.0, 1.0}))
    {
        std::cerr << "route recovery changed a legal state\n";
        return 1;
    }
    return 0;
}