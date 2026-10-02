// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RaceMode.h"

RaceMode NextRaceMode(RaceMode pMode)
{
    switch (pMode)
    {
    case RaceMode::SingleRace:
        return RaceMode::TimeTrial;
    case RaceMode::TimeTrial:
        return RaceMode::Practice;
    case RaceMode::Practice:
        return RaceMode::Championship;
    case RaceMode::Championship:
        return RaceMode::SingleRace;
    }
    return RaceMode::SingleRace;
}

const char* RaceModeName(RaceMode pMode)
{
    switch (pMode)
    {
    case RaceMode::SingleRace:
        return "Single Race";
    case RaceMode::TimeTrial:
        return "Time Trial";
    case RaceMode::Practice:
        return "Practice";
    case RaceMode::Championship:
        return "Championship";
    }
    return "Single Race";
}

bool RaceModeUsesRivals(RaceMode pMode)
{
    return pMode == RaceMode::SingleRace || pMode == RaceMode::Championship;
}

bool RaceModeHasFinish(RaceMode pMode)
{
    return pMode != RaceMode::Practice;
}