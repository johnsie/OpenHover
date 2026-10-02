// SPDX-License-Identifier: MIT OR Apache-2.0
#include "LapTiming.h"

#include <cmath>
#include <iostream>

int main()
{
    LapTimer timer;
    RaceProgress progress;
    progress.mElapsedSeconds = 12.5;
    timer.Update(progress);
    if (std::fabs(timer.Timing().mCurrentSeconds - 12.5) > 0.0001)
    {
        std::cerr << "current lap time was not recorded\n";
        return 1;
    }

    progress.mElapsedSeconds = 15.0;
    progress.mNextCheckpoint = 1;
    timer.Update(progress);
    if (std::fabs(timer.Timing().mLastSplitSeconds - 15.0) > 0.0001
        || std::fabs(timer.Timing().mBestSplitSeconds - 15.0) > 0.0001
        || std::fabs(timer.BestSplitSeconds(0) - 15.0) > 0.0001)
    {
        std::cerr << "checkpoint split was not recorded\n";
        return 1;
    }

    progress.mElapsedSeconds = 18.0;
    progress.mNextCheckpoint = 2;
    timer.Update(progress);
    if (std::fabs(timer.Timing().mLastSplitSeconds - 3.0) > 0.0001
        || std::fabs(timer.BestSplitSeconds(1) - 3.0) > 0.0001)
    {
        std::cerr << "subsequent checkpoint split was not recorded\n";
        return 1;
    }

    progress.mElapsedSeconds = 19.0;
    progress.mCompletedLaps = 1;
    progress.mNextCheckpoint = 0;
    timer.Update(progress);
    if (std::fabs(timer.Timing().mLastSeconds - 19.0) > 0.0001
        || std::fabs(timer.Timing().mBestSeconds - 19.0) > 0.0001)
    {
        std::cerr << "first completed lap was not recorded\n";
        return 1;
    }
    if (std::fabs(timer.Timing().mLastSplitSeconds - 1.0) > 0.0001
        || std::fabs(timer.BestSplitSeconds(2) - 1.0) > 0.0001)
    {
        std::cerr << "finish split was not recorded\n";
        return 1;
    }

    progress.mElapsedSeconds = 36.0;
    progress.mCompletedLaps = 2;
    timer.Update(progress);
    if (std::fabs(timer.Timing().mLastSeconds - 17.0) > 0.0001
        || std::fabs(timer.Timing().mBestSeconds - 17.0) > 0.0001)
    {
        std::cerr << "best lap was not updated\n";
        return 1;
    }
    return 0;
}