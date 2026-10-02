// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RIVAL_NAMES_H
#define OPENHOVER_RIVAL_NAMES_H

#include <string>
#include <vector>

// Number of distinct rival names. A race never has more rivals than this.
int RivalNamePoolSize();

// Display name for a pool index in [0, RivalNamePoolSize()). Out-of-range indices wrap and gain a
// number suffix, so the result is always printable.
std::string RivalName(int pPoolIndex);

// Chooses pCount different pool indices at random for one race, so no two rivals share a name.
// The same seed always gives the same choice. pCount is limited to the pool size.
std::vector<int> PickRivalNames(int pCount, unsigned int pSeed);

#endif
