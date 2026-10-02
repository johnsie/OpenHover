// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_LAP_TIMING_H
#define OPENHOVER_LAP_TIMING_H

#include "Race.h"

#include <vector>

struct LapTiming
{
    double mCurrentSeconds = 0.0;
    double mLastSeconds = 0.0;
    double mBestSeconds = 0.0;
    double mLastImprovementSeconds = 0.0;
    double mCurrentSplitSeconds = 0.0;
    double mLastSplitSeconds = 0.0;
    double mBestSplitSeconds = 0.0;
};

class LapTimer
{
public:
    void Reset();
    void Update(const RaceProgress& pProgress);

    const LapTiming& Timing() const { return mTiming; }
    double BestSplitSeconds(int pSplitIndex) const;

private:
    void RecordSplit(int pSplitIndex, double pElapsedSeconds);

    LapTiming mTiming;
    int mObservedLaps = 0;
    int mObservedNextCheckpoint = 0;
    double mLapStartSeconds = 0.0;
    double mSplitStartSeconds = 0.0;
    std::vector<double> mBestSplitSeconds;
};

#endif
