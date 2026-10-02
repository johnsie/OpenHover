// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Race.h"

#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    std::vector<RaceGate> checkpoints;
    checkpoints.push_back({10.0, 0.0, 1.0});
    checkpoints.push_back({20.0, 0.0, 1.0});
    Race race(checkpoints, {30.0, 0.0, 1.0}, 1);

    race.Update(30.0, 0.0, 1.0);
    if (race.Progress().mCompletedLaps != 0)
    {
        std::cerr << "finish gate completed a lap before checkpoints\n";
        return 1;
    }

    race.Update(0.0, 0.0, 1.0);
    race.Update(10.0, 0.0, 1.0);
    race.Update(0.0, 0.0, 1.0);
    race.Update(20.0, 0.0, 1.0);
    race.Update(0.0, 0.0, 1.0);
    race.Update(30.0, 0.0, 1.0);

    if (race.Progress().mCompletedLaps != 1 || !race.Progress().mFinished)
    {
        std::cerr << "ordered gates did not complete the race\n";
        return 1;
    }
    if (std::fabs(race.Progress().mElapsedSeconds - 7.0) > 0.0001)
    {
        std::cerr << "race clock did not track elapsed time\n";
        return 1;
    }

    race.Update(30.0, 0.0, 0.0);
    race.Update(0.0, 0.0, 1.0);
    if (race.Progress().mCompletedLaps != 1 || std::fabs(race.Progress().mElapsedSeconds - 7.0) > 0.0001)
    {
        std::cerr << "completed race state changed\n";
        return 1;
    }

    Race multiLapRace(checkpoints, {30.0, 0.0, 1.0}, 2);
    if (multiLapRace.TargetLaps() != 2)
    {
        std::cerr << "race did not retain its configured lap count\n";
        return 1;
    }
    multiLapRace.Update(10.0, 0.0, 1.0);
    multiLapRace.Update(0.0, 0.0, 1.0);
    multiLapRace.Update(20.0, 0.0, 1.0);
    multiLapRace.Update(0.0, 0.0, 1.0);
    multiLapRace.Update(30.0, 0.0, 1.0);
    if (multiLapRace.Progress().mFinished || multiLapRace.Progress().mCompletedLaps != 1)
    {
        std::cerr << "race finished before its configured lap count\n";
        return 1;
    }

    return 0;
}