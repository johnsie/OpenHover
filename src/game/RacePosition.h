// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RACE_POSITION_H
#define OPENHOVER_RACE_POSITION_H

#include "Race.h"

#include <vector>

int CalculateRacePosition(const std::vector<RaceProgress>& pProgresses, int pRacerIndex);

#endif