// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RaceStart.h"

RaceStart::RaceStart(double pSeconds)
    : mDuration(pSeconds > 0.0 ? pSeconds : 0.1),
      mRemaining(mDuration)
{
}

void RaceStart::Reset()
{
    mRemaining = mDuration;
    mCountingDown = false;
    mStarted = false;
}

void RaceStart::Begin()
{
    if (!mStarted)
        mCountingDown = true;
}

void RaceStart::Update(double pSeconds)
{
    if (!mCountingDown || mStarted || pSeconds <= 0.0)
        return;

    mRemaining -= pSeconds;
    if (mRemaining <= 0.0)
    {
        mRemaining = 0.0;
        mStarted = true;
    }
}

int RaceStart::LightsLit() const
{
    if (!mCountingDown || mStarted)
        return 0;

    const int lights = static_cast<int>((mDuration - mRemaining) * 3.0 / mDuration) + 1;
    return lights > 3 ? 3 : lights;
}