// SPDX-License-Identifier: MIT OR Apache-2.0
#include "FixedStepClock.h"

#include <algorithm>

FixedStepClock::FixedStepClock(double pStepSeconds, int pMaximumSteps)
    : mStepSeconds(pStepSeconds > 0.0 ? pStepSeconds : 1.0 / 120.0),
      mMaximumSteps(pMaximumSteps > 0 ? pMaximumSteps : 1)
{
}

int FixedStepClock::Consume(double pFrameSeconds)
{
    if (pFrameSeconds <= 0.0)
        return 0;

    mAccumulator += std::min(pFrameSeconds, mStepSeconds * mMaximumSteps);
    int steps = 0;
    while (mAccumulator + mStepSeconds * 1.0e-9 >= mStepSeconds && steps < mMaximumSteps)
    {
        mAccumulator = std::max(0.0, mAccumulator - mStepSeconds);
        ++steps;
    }
    return steps;
}

void FixedStepClock::Reset()
{
    mAccumulator = 0.0;
}