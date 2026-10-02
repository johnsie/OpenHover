// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RACE_MODE_H
#define OPENHOVER_RACE_MODE_H

enum class RaceMode
{
    SingleRace,
    TimeTrial,
    Practice,
    Championship
};

RaceMode NextRaceMode(RaceMode pMode);
const char* RaceModeName(RaceMode pMode);
bool RaceModeUsesRivals(RaceMode pMode);
bool RaceModeHasFinish(RaceMode pMode);

#endif