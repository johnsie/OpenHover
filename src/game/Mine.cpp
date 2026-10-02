// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Mine.h"

#include <cmath>

bool ApplyMine(HovercraftState& pState, Mine& pMine, double pSpinOutSeconds)
{
    if (pMine.mTriggered || pMine.mRadius <= 0.0 || pSpinOutSeconds <= 0.0)
        return false;
    const double deltaX = pState.mX - pMine.mX;
    const double deltaY = pState.mY - pMine.mY;
    if (deltaX * deltaX + deltaY * deltaY > pMine.mRadius * pMine.mRadius)
        return false;
    pMine.mTriggered = true;
    ApplySpinOut(pState, pSpinOutSeconds);
    return true;
}