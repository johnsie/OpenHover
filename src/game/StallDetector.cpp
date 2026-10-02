// SPDX-License-Identifier: MIT OR Apache-2.0
#include "StallDetector.h"

#include <cmath>

void StallDetector::Reset()
{
    mStarted = false;
    mStuckSeconds = 0.0;
    mProgressStarted = false;
    mProgressSeconds = 0.0;
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

bool StallDetector::UpdateProgress(double pRouteDistance, double pSeconds)
{
    if (!mProgressStarted || pRouteDistance >= mBestDistance + kProgressDistance)
    {
        mProgressStarted = true;
        mBestDistance = pRouteDistance;
        mProgressSeconds = 0.0;
        return false;
    }
    mProgressSeconds += pSeconds;
    if (mProgressSeconds < kProgressWindowSeconds)
        return false;
    mProgressStarted = false;
    mProgressSeconds = 0.0;
    return true;
}
