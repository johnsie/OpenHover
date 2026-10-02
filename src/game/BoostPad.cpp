// SPDX-License-Identifier: MIT OR Apache-2.0
#include "BoostPad.h"

bool ApplyBoostPad(HovercraftState& pState, const BoostPad& pPad)
{
    if (pPad.mRadius <= 0.0 || pState.mBoostEnergy >= 1.0)
        return false;

    const double deltaX = pState.mX - pPad.mX;
    const double deltaY = pState.mY - pPad.mY;
    if (deltaX * deltaX + deltaY * deltaY > pPad.mRadius * pPad.mRadius)
        return false;

    pState.mBoostEnergy = 1.0;
    return true;
}