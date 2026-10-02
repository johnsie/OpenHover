// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Race.h"

Race::Race(const std::vector<RaceGate>& pCheckpoints, const RaceGate& pFinish, int pTargetLaps)
    : mCheckpoints(pCheckpoints),
    mFinish(pFinish),
    mTargetLaps(pTargetLaps > 0 ? pTargetLaps : 1)
{
    Reset();
}

void Race::Reset()
{
    mProgress = RaceProgress();
    mInsideActiveGate = false;
}

void Race::Update(double pX, double pY, double pSeconds)
{
    if (mProgress.mFinished)
        return;

    if (pSeconds > 0.0)
        mProgress.mElapsedSeconds += pSeconds;

    const RaceGate& activeGate = mProgress.mNextCheckpoint < static_cast<int>(mCheckpoints.size())
        ? mCheckpoints[mProgress.mNextCheckpoint]
        : mFinish;
    const bool inside = IsInside(activeGate, pX, pY);
    if (inside && !mInsideActiveGate)
    {
        if (mProgress.mNextCheckpoint < static_cast<int>(mCheckpoints.size()))
            ++mProgress.mNextCheckpoint;
        else
        {
            ++mProgress.mCompletedLaps;
            if (mProgress.mCompletedLaps >= mTargetLaps)
                mProgress.mFinished = true;
            else
                mProgress.mNextCheckpoint = 0;
        }
    }
    mInsideActiveGate = inside;
}

bool Race::IsInside(const RaceGate& pGate, double pX, double pY) const
{
    const double deltaX = pX - pGate.mX;
    const double deltaY = pY - pGate.mY;
    return deltaX * deltaX + deltaY * deltaY <= pGate.mRadius * pGate.mRadius;
}