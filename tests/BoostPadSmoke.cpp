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
    if (!ApplyBoostPad(hovercraft, pad) || hovercraft.mBoostEnergy != 1.0)
    {
        std::cerr << "boost pad did not refill energy\n";
        return 1;
    }

    if (ApplyBoostPad(hovercraft, pad))
    {
        std::cerr << "full boost energy triggered a pad again\n";
        return 1;
    }

    hovercraft.mX = 8.0;
    hovercraft.mBoostEnergy = 0.5;
    if (ApplyBoostPad(hovercraft, pad) || hovercraft.mBoostEnergy != 0.5)
    {
        std::cerr << "distant hovercraft triggered a boost pad\n";
        return 1;
    }

    return 0;
}