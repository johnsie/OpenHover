// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RaceMode.h"

#include <iostream>

int main()
{
    RaceMode mode = RaceMode::SingleRace;
    for (int index = 0; index < 4; ++index)
        mode = NextRaceMode(mode);
    if (mode != RaceMode::SingleRace || !RaceModeUsesRivals(RaceMode::Championship)
        || RaceModeUsesRivals(RaceMode::TimeTrial) || RaceModeHasFinish(RaceMode::Practice)
        || RaceModeName(RaceMode::SingleRace)[0] == '\0')
    {
        std::cerr << "race modes did not report expected behavior\n";
        return 1;
    }
    return 0;
}