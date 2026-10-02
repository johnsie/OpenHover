// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RaisedSection.h"

#include <cmath>
#include <iostream>

int main()
{
    RaisedSection section = {0.0, 0.0, 1.0, 4.0, 0.0, 1.6};
    HovercraftState state;
    state.mX = -1.5;
    state.mHeading = 0.0;
    state.mTravelHeading = 0.0;
    state.mSpeed = 10.0;
    state.mHeight = 1.2;
    if (!ResolveRaisedSectionCollision(state, section) || std::fabs(state.mX + 1.9) > 0.0001
        || std::fabs(state.mSpeed - 8.0) > 0.0001 || std::fabs(state.mHeading) > 0.0001
        || state.mTravelHeading <= 3.0)
    {
        std::cerr << "grounded craft did not bounce from raised section\n";
        return 1;
    }

    state = HovercraftState();
    state.mPreviousX = -4.0;
    state.mPreviousY = 0.0;
    state.mHasPreviousPosition = true;
    state.mX = 0.8;
    state.mHeading = 0.0;
    state.mTravelHeading = 0.0;
    state.mSpeed = 10.0;
    state.mHeight = 1.2;
    if (!ResolveRaisedSectionCollision(state, section) || std::fabs(state.mX + 1.9) > 0.01
        || std::fabs(state.mHeading) > 0.0001 || state.mTravelHeading <= 3.0)
    {
        std::cerr << "raised section did not resolve at first contact\n";
        return 1;
    }

    state = HovercraftState();
    state.mX = 0.0;
    state.mHeight = 1.7;
    if (ResolveRaisedSectionCollision(state, section))
    {
        std::cerr << "airborne craft could not clear raised section\n";
        return 1;
    }

    RaisedSection platform = {0.0, 0.0, 8.0, 4.0, 0.0, 1.45, true};
    state = HovercraftState();
    state.mHeight = 1.8;
    state.mVerticalSpeed = -2.0;
    if (!LandOnRaisedSection(state, platform) || std::fabs(state.mHeight - 2.0) > 0.0001
        || state.mVerticalSpeed != 0.0 || std::fabs(state.mSurfaceHeight - 2.0) > 0.0001)
    {
        std::cerr << "descending craft did not land on raised platform\n";
        return 1;
    }
    state.mVerticalSpeed = 2.0;
    if (LandOnRaisedSection(state, platform))
    {
        std::cerr << "rising craft landed on raised platform\n";
        return 1;
    }
    return 0;
}