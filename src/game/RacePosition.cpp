// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RacePosition.h"

#include <algorithm>
#include <numeric>

int CalculateRacePosition(const std::vector<RaceProgress>& pProgresses, int pRacerIndex)
{
    if (pRacerIndex < 0 || pRacerIndex >= static_cast<int>(pProgresses.size()))
        return 0;

    std::vector<int> racerIndices(pProgresses.size());
    std::iota(racerIndices.begin(), racerIndices.end(), 0);
    std::sort(racerIndices.begin(), racerIndices.end(), [&pProgresses](int pLeft, int pRight)
    {
        const RaceProgress& left = pProgresses[pLeft];
        const RaceProgress& right = pProgresses[pRight];
        if (left.mFinished != right.mFinished)
            return left.mFinished;
        if (left.mCompletedLaps != right.mCompletedLaps)
            return left.mCompletedLaps > right.mCompletedLaps;
        if (left.mNextCheckpoint != right.mNextCheckpoint)
            return left.mNextCheckpoint > right.mNextCheckpoint;
        if (left.mSegmentProgress != right.mSegmentProgress)
            return left.mSegmentProgress > right.mSegmentProgress;
        if (left.mElapsedSeconds != right.mElapsedSeconds)
            return left.mElapsedSeconds < right.mElapsedSeconds;
        return pLeft < pRight;
    });

    for (int position = 0; position < static_cast<int>(racerIndices.size()); ++position)
    {
        if (racerIndices[position] == pRacerIndex)
            return position + 1;
    }
    return 0;
}