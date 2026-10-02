// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Race.h"

#include <cmath>

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
    mHasPreviousPosition = false;
}

void Race::Update(double pX, double pY, double pSeconds)
{
    if (mProgress.mFinished)
        return;

    if (pSeconds > 0.0)
        mProgress.mElapsedSeconds += pSeconds;

    const auto completeLap = [&]()
    {
        ++mProgress.mCompletedLaps;
        if (mProgress.mCompletedLaps >= mTargetLaps)
            mProgress.mFinished = true;
        else
            mProgress.mNextCheckpoint = 0;
        mProgress.mSegmentProgress = 0.0;
        mInsideActiveGate = false;
        mPreviousX = pX;
        mPreviousY = pY;
        mHasPreviousPosition = true;
    };

    const RaceGate& activeGate = mProgress.mNextCheckpoint < static_cast<int>(mCheckpoints.size())
        ? mCheckpoints[mProgress.mNextCheckpoint]
        : mFinish;
    const RaceGate& previousGate = mProgress.mNextCheckpoint == 0
        ? mFinish : mCheckpoints[mProgress.mNextCheckpoint - 1];
    const double segmentX = activeGate.mX - previousGate.mX;
    const double segmentY = activeGate.mY - previousGate.mY;
    const double segmentLengthSquared = segmentX * segmentX + segmentY * segmentY;
    if (segmentLengthSquared > 0.0)
    {
        const double progress = ((pX - previousGate.mX) * segmentX
            + (pY - previousGate.mY) * segmentY) / segmentLengthSquared;
        mProgress.mSegmentProgress = progress < 0.0 ? 0.0 : (progress > 1.0 ? 1.0 : progress);
    }
    const bool inside = IsInside(activeGate, pX, pY);
    bool crossedGate = false;
    if (mHasPreviousPosition && segmentLengthSquared > 0.0)
    {
        const double segmentLength = std::sqrt(segmentLengthSquared);
        const double directionX = segmentX / segmentLength;
        const double directionY = segmentY / segmentLength;
        const double previousForward = (mPreviousX - activeGate.mX) * directionX
            + (mPreviousY - activeGate.mY) * directionY;
        const double currentForward = (pX - activeGate.mX) * directionX
            + (pY - activeGate.mY) * directionY;
        if (previousForward < 0.0 && currentForward >= 0.0)
        {
            const double crossingFraction = -previousForward / (currentForward - previousForward);
            const double crossingX = mPreviousX + (pX - mPreviousX) * crossingFraction;
            const double crossingY = mPreviousY + (pY - mPreviousY) * crossingFraction;
            const double lateral = (crossingX - activeGate.mX) * -directionY
                + (crossingY - activeGate.mY) * directionX;
            crossedGate = std::fabs(lateral) <= activeGate.mRadius;
        }
    }
    if ((inside && !mInsideActiveGate) || crossedGate)
    {
        if (mProgress.mNextCheckpoint < static_cast<int>(mCheckpoints.size()))
            ++mProgress.mNextCheckpoint;
        else
            completeLap();
        mProgress.mSegmentProgress = 0.0;
        mInsideActiveGate = false;
        mPreviousX = pX;
        mPreviousY = pY;
        mHasPreviousPosition = true;
        return;
    }
    mInsideActiveGate = inside;
    mPreviousX = pX;
    mPreviousY = pY;
    mHasPreviousPosition = true;
}

bool Race::IsInside(const RaceGate& pGate, double pX, double pY) const
{
    const double deltaX = pX - pGate.mX;
    const double deltaY = pY - pGate.mY;
    return deltaX * deltaX + deltaY * deltaY <= pGate.mRadius * pGate.mRadius;
}