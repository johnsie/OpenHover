// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_BOOST_PAD_H
#define OPENHOVER_BOOST_PAD_H

#include "Hovercraft.h"

struct BoostPad
{
    double mX = 0.0;
    double mY = 0.0;
    double mRadius = 1.0;
};

bool ApplyBoostPad(HovercraftState& pState, const BoostPad& pPad);

#endif