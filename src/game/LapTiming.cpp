// SPDX-License-Identifier: MIT OR Apache-2.0
#include "LapTiming.h"

void LapTimer::Reset()
{
    mTiming = LapTiming();
    mObservedLaps = 0;
    mObservedNextCheckpoint = 0;
    mLapStartSeconds = 0.0;
    mSplitStartSeconds = 0.0;
    mBestSplitSeconds.clear();
}

void LapTimer::Update(const RaceProgress& pProgress)
{
    if (pProgress.mCompletedLaps > mObservedLaps)
    {
        RecordSplit(mObservedNextCheckpoint, pProgress.mElapsedSeconds);
        mTiming.mLastSeconds = pProgress.mElapsedSeconds - mLapStartSeconds;
        const double previousBestSeconds = mTiming.mBestSeconds;
        if (mTiming.mBestSeconds == 0.0 || mTiming.mLastSeconds < mTiming.mBestSeconds)
            mTiming.mBestSeconds = mTiming.mLastSeconds;
        mTiming.mLastImprovementSeconds = previousBestSeconds > 0.0
            && mTiming.mLastSeconds < previousBestSeconds
            ? previousBestSeconds - mTiming.mLastSeconds : 0.0;
        mLapStartSeconds = pProgress.mElapsedSeconds;
        mObservedLaps = pProgress.mCompletedLaps;
        mObservedNextCheckpoint = 0;
        mSplitStartSeconds = pProgress.mElapsedSeconds;
    }
    else if (!pProgress.mFinished && pProgress.mNextCheckpoint > mObservedNextCheckpoint)
    {
        RecordSplit(pProgress.mNextCheckpoint - 1, pProgress.mElapsedSeconds);
        mObservedNextCheckpoint = pProgress.mNextCheckpoint;
    }
    mTiming.mCurrentSeconds = pProgress.mElapsedSeconds - mLapStartSeconds;
    mTiming.mCurrentSplitSeconds = pProgress.mElapsedSeconds - mSplitStartSeconds;
}

double LapTimer::BestSplitSeconds(int pSplitIndex) const
{
    return pSplitIndex >= 0 && pSplitIndex < static_cast<int>(mBestSplitSeconds.size())
        ? mBestSplitSeconds[pSplitIndex] : 0.0;
}

void LapTimer::RecordSplit(int pSplitIndex, double pElapsedSeconds)
{
    mTiming.mLastSplitSeconds = pElapsedSeconds - mSplitStartSeconds;
    if (pSplitIndex >= static_cast<int>(mBestSplitSeconds.size()))
        mBestSplitSeconds.resize(pSplitIndex + 1, 0.0);
    double& bestSplit = mBestSplitSeconds[pSplitIndex];
    if (bestSplit == 0.0 || mTiming.mLastSplitSeconds < bestSplit)
        bestSplit = mTiming.mLastSplitSeconds;
    mTiming.mBestSplitSeconds = bestSplit;
    mSplitStartSeconds = pElapsedSeconds;
}
