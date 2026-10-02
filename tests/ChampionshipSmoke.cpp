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
        // All three finish level on 6 points; the best finish in the last event (competitor 2 won
        // it, the player was third) decides the order.
        || championship.StandingForCompetitor(2) != 1 || championship.StandingForCompetitor(1) != 2
        || championship.StandingForCompetitor(0) != 3)
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
    // Competitors level on points are ordered by where they finished the latest event, so finishing
    // last never ranks level with finishing fourth.
    Championship eight(3);
    if (!eight.RecordResults({8, 1, 2, 3, 4, 5, 6, 7}) || eight.StandingForCompetitor(1) != 1
        || eight.StandingForCompetitor(2) != 2 || eight.StandingForCompetitor(3) != 3
        || eight.StandingForCompetitor(4) != 4 || eight.StandingForCompetitor(7) != 7
        || eight.StandingForCompetitor(0) != 8)
    {
        std::cerr << "tied competitors were not ordered by their latest finishing place\n";
        return 1;
    }
    // After a second event, level competitors are ordered by that event's finishing places.
    if (!eight.AdvanceEvent() || !eight.RecordResults({1, 8, 7, 6, 5, 4, 3, 2})
        // The player and competitor 1 are level on 3 points; the player won this event, so leads.
        || eight.StandingForCompetitor(0) != 1 || eight.StandingForCompetitor(1) != 2
        // Competitors 2 and 7 are level on 2 points; 7 finished second, 2 finished seventh.
        || eight.StandingForCompetitor(7) != 3 || eight.StandingForCompetitor(2) != 4)
    {
        std::cerr << "series standings after a second event were wrong\n";
        return 1;
    }
    return 0;
}
