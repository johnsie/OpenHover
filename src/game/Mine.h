// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_MINE_H
#define OPENHOVER_MINE_H

#include "Hovercraft.h"

struct Mine
{
    double mX = 0.0;
    double mY = 0.0;
    double mRadius = 1.0;
    bool mTriggered = false;
};

bool ApplyMine(HovercraftState& pState, Mine& pMine, double pSpinOutSeconds = 2.5);

#endif