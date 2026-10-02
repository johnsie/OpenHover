// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_CHAMPIONSHIP_H
#define OPENHOVER_CHAMPIONSHIP_H

#include <vector>

class Championship
{
public:
    explicit Championship(int pEventCount);

    void Reset();
    bool RecordResult(int pPlayerPosition);
    bool RecordResults(const std::vector<int>& pPositions);
    bool AdvanceEvent();

    int CurrentEvent() const { return mCurrentEvent; }
    int EventCount() const { return mEventCount; }
    int PlayerPoints() const { return mPlayerPoints; }
    int CompetitorCount() const { return mCompetitorCount; }
    int CompetitorPoints(int pCompetitorIndex) const;
    int StandingForCompetitor(int pCompetitorIndex) const;
    bool EventRecorded() const { return mEventRecorded; }
    bool Complete() const { return mComplete; }

private:
    int mEventCount;
    int mCurrentEvent = 0;
    int mPlayerPoints = 0;
    int mCompetitorCount = 0;
    std::vector<int> mCompetitorPoints;
    bool mEventRecorded = false;
    bool mComplete = false;
};

#endif