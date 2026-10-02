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
    int LastPointsAwarded(int pCompetitorIndex) const;
    // Place in the series: most points first; competitors level on points are ordered by where
    // they finished the latest event, and then by index.
    int StandingForCompetitor(int pCompetitorIndex) const;
    bool EventRecorded() const { return mEventRecorded; }
    bool Complete() const { return mComplete; }

private:
    int mEventCount;
    int mCurrentEvent = 0;
    int mPlayerPoints = 0;
    int mCompetitorCount = 0;
    std::vector<int> mCompetitorPoints;
    std::vector<int> mLastPointsAwarded;
    std::vector<int> mLastPositions; // finishing place of each competitor in the latest event
    bool mEventRecorded = false;
    bool mComplete = false;
};

#endif
