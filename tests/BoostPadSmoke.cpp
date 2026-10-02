// SPDX-License-Identifier: MIT OR Apache-2.0
#include "BoostPad.h"

#include <iostream>

int main()
{
    BoostPad pad;
    pad.mX = 3.0;
    pad.mY = 4.0;
    pad.mRadius = 2.0;
    HovercraftState hovercraft;
    hovercraft.mX = 4.0;
    hovercraft.mY = 4.0;
    hovercraft.mBoostEnergy = 0.25;
    if (!ApplyBoostPad(hovercraft, pad) || hovercraft.mPadBoostSeconds <= 0.0)
    {
        std::cerr << "boost pad did not start a timed boost\n";
        return 1;
    }

    if (ApplyBoostPad(hovercraft, pad))
    {
        std::cerr << "active timed boost triggered a pad again\n";
        return 1;
    }

    Hovercraft boostedHovercraft;
    boostedHovercraft.Reset(hovercraft);
    boostedHovercraft.Step(HovercraftInput(), 0.1);
    if (!boostedHovercraft.State().mBoosting || boostedHovercraft.State().mSpeed <= 0.0)
    {
        std::cerr << "timed boost did not accelerate the hovercraft\n";
        return 1;
    }
    for (int step = 0; step < 30; ++step)
        boostedHovercraft.Step(HovercraftInput(), 0.1);
    if (boostedHovercraft.State().mPadBoostSeconds != 0.0 || boostedHovercraft.State().mBoosting)
    {
        std::cerr << "timed boost did not expire\n";
        return 1;
    }

    hovercraft.mX = 8.0;
    hovercraft.mBoostEnergy = 0.5;
    hovercraft.mPadBoostSeconds = 0.0;
    if (ApplyBoostPad(hovercraft, pad) || hovercraft.mPadBoostSeconds != 0.0)
    {
        std::cerr << "distant hovercraft triggered a timed boost\n";
        return 1;
    }

    return 0;
}