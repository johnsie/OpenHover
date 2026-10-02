// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RacePosition.h"

#include <iostream>
#include <vector>

int main()
{
    std::vector<RaceProgress> progresses(4);
    progresses[0].mCompletedLaps = 1;
    progresses[0].mElapsedSeconds = 10.0;
    progresses[1].mNextCheckpoint = 2;
    progresses[1].mElapsedSeconds = 50.0;
    progresses[2].mCompletedLaps = 1;
    progresses[2].mElapsedSeconds = 9.0;
    progresses[3].mCompletedLaps = 1;
    progresses[3].mElapsedSeconds = 9.0;

    if (CalculateRacePosition(progresses, 2) != 1
        || CalculateRacePosition(progresses, 3) != 2
        || CalculateRacePosition(progresses, 0) != 3
        || CalculateRacePosition(progresses, 1) != 4)
    {
        std::cerr << "race positions did not respect progress and stable ties\n";
        return 1;
    }

    progresses[1].mFinished = true;
    if (CalculateRacePosition(progresses, 1) != 1)
    {
        std::cerr << "finished racer did not lead the standings\n";
        return 1;
    }

    if (CalculateRacePosition(progresses, -1) != 0
        || CalculateRacePosition(progresses, 4) != 0)
    {
        std::cerr << "invalid racer index produced a position\n";
        return 1;
    }
    return 0;
}