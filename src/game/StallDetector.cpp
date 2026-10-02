// SPDX-License-Identifier: MIT OR Apache-2.0
#include "StallDetector.h"

#include <cmath>

void StallDetector::Reset()
{
    mStarted = false;
    mStuckSeconds = 0.0;
}

bool StallDetector::Update(double pX, double pY, double pSeconds)
{
    if (!mStarted || std::hypot(pX - mAnchorX, pY - mAnchorY) > kProgressMetres)
    {
        mStarted = true;
        mAnchorX = pX;
        mAnchorY = pY;
        mStuckSeconds = 0.0;
        return false;
    }
    mStuckSeconds += pSeconds;
    if (mStuckSeconds < kStallSeconds)
        return false;
    mStarted = false;
    mStuckSeconds = 0.0;
    return true;
}
