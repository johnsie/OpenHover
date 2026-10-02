// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Championship.h"

#include <iostream>

int main()
{
    Championship championship(3);
    if (!championship.RecordResults({2, 1, 3}) || championship.PlayerPoints() != 2
        || championship.CompetitorPoints(1) != 3 || championship.CompetitorPoints(2) != 1
        || championship.LastPointsAwarded(0) != 2 || championship.LastPointsAwarded(1) != 3
        || championship.StandingForCompetitor(0) != 2 || championship.StandingForCompetitor(1) != 1
        || championship.RecordResult(1) || !championship.AdvanceEvent()
        || championship.CurrentEvent() != 1 || championship.EventRecorded())
    {
        std::cerr << "championship did not record and advance its first event\n";
        return 1;
    }

    if (!championship.RecordResults({1, 3, 2}) || !championship.AdvanceEvent()
        || !championship.RecordResults({3, 2, 1}) || championship.AdvanceEvent()
        || !championship.Complete() || championship.PlayerPoints() != 6
        || championship.CompetitorPoints(1) != 6 || championship.CompetitorPoints(2) != 6
        || championship.StandingForCompetitor(0) != 1 || championship.StandingForCompetitor(2) != 3)
    {
        std::cerr << "championship points or completion state was incorrect\n";
        return 1;
    }

    championship.Reset();
    if (championship.CurrentEvent() != 0 || championship.PlayerPoints() != 0
        || championship.CompetitorCount() != 0 || championship.CompetitorPoints(1) != 0
        || championship.LastPointsAwarded(0) != 0
        || championship.EventRecorded() || championship.Complete())
    {
        std::cerr << "championship reset did not restore its initial state\n";
        return 1;
    }
    return 0;
}
