// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RAISED_SECTION_H
#define OPENHOVER_RAISED_SECTION_H

#include "Hovercraft.h"

struct RaisedSection
{
    double mX = 0.0;
    double mY = 0.0;
    double mHalfLength = 2.0;
    double mHalfWidth = 4.0;
    double mHeading = 0.0;
    double mClearHeight = 1.6;
};

bool ResolveRaisedSectionCollision(HovercraftState& pState, const RaisedSection& pSection,
                                   double pCraftRadius = 0.9, double pRestitution = 0.8);

#endif