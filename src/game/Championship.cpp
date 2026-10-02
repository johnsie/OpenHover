// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Championship.h"

#include <algorithm>

namespace
{
int PointsForPosition(int pPosition)
{
    switch (pPosition)
    {
    case 1:
        return 3;
    case 2:
        return 2;
    case 3:
        return 1;
    default:
        return 0;
    }
}
}

Championship::Championship(int pEventCount)
    : mEventCount(pEventCount > 0 ? pEventCount : 1)
{
    Reset();
}

void Championship::Reset()
{
    mCurrentEvent = 0;
    mPlayerPoints = 0;
    mCompetitorCount = 0;
    mCompetitorPoints.clear();
    mEventRecorded = false;
    mComplete = false;
}

bool Championship::RecordResult(int pPlayerPosition)
{
    return RecordResults(std::vector<int>(1, pPlayerPosition));
}

bool Championship::RecordResults(const std::vector<int>& pPositions)
{
    if (mEventRecorded || mComplete || pPositions.empty())
        return false;

    if (mCompetitorCount != 0 && mCompetitorCount != static_cast<int>(pPositions.size()))
        return false;

    std::vector<int> sortedPositions = pPositions;
    std::sort(sortedPositions.begin(), sortedPositions.end());
    for (int competitorIndex = 0; competitorIndex < static_cast<int>(sortedPositions.size()); ++competitorIndex)
    {
        if (sortedPositions[competitorIndex] != competitorIndex + 1)
            return false;
    }

    mCompetitorCount = static_cast<int>(pPositions.size());
    if (mCompetitorPoints.empty())
        mCompetitorPoints.assign(mCompetitorCount, 0);
    for (int competitorIndex = 0; competitorIndex < mCompetitorCount; ++competitorIndex)
        mCompetitorPoints[competitorIndex] += PointsForPosition(pPositions[competitorIndex]);
    mPlayerPoints = mCompetitorPoints[0];
    mEventRecorded = true;
    return true;
}

int Championship::CompetitorPoints(int pCompetitorIndex) const
{
    if (pCompetitorIndex < 0 || pCompetitorIndex >= static_cast<int>(mCompetitorPoints.size()))
        return 0;
    return mCompetitorPoints[pCompetitorIndex];
}

int Championship::StandingForCompetitor(int pCompetitorIndex) const
{
    if (pCompetitorIndex < 0 || pCompetitorIndex >= static_cast<int>(mCompetitorPoints.size()))
        return 0;

    int standing = 1;
    for (int competitorIndex = 0; competitorIndex < static_cast<int>(mCompetitorPoints.size()); ++competitorIndex)
    {
        if (mCompetitorPoints[competitorIndex] > mCompetitorPoints[pCompetitorIndex]
            || (mCompetitorPoints[competitorIndex] == mCompetitorPoints[pCompetitorIndex]
                && competitorIndex < pCompetitorIndex))
            ++standing;
    }
    return standing;
}

bool Championship::AdvanceEvent()
{
    if (!mEventRecorded || mComplete)
        return false;
    if (mCurrentEvent + 1 >= mEventCount)
    {
        mComplete = true;
        return false;
    }
    ++mCurrentEvent;
    mEventRecorded = false;
    return true;
}