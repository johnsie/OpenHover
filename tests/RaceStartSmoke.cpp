// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RaceStart.h"

#include <iostream>

int main()
{
    RaceStart start;
    if (!start.Ready() || start.CountdownActive() || start.Started() || start.LightsLit() != 0)
    {
        std::cerr << "race start was not ready for an explicit start\n";
        return 1;
    }

    start.Update(2.0);
    if (!start.Ready())
    {
        std::cerr << "race started before an explicit start\n";
        return 1;
    }

    start.Begin();
    if (start.Ready() || !start.CountdownActive() || start.LightsLit() != 1
        || start.SecondsRemaining() != 6)
    {
        std::cerr << "countdown did not begin at its first light\n";
        return 1;
    }

    start.Update(2.0);
    if (start.Started() || start.LightsLit() != 2 || start.SecondsRemaining() != 4)
    {
        std::cerr << "countdown did not advance to its second light\n";
        return 1;
    }

    start.Update(2.0);
    if (start.Started() || start.LightsLit() != 3 || start.SecondsRemaining() != 2)
    {
        std::cerr << "countdown did not advance to its third light\n";
        return 1;
    }

    start.Update(2.0);
    if (!start.Started() || start.LightsLit() != 0 || start.SecondsRemaining() != 0)
    {
        std::cerr << "countdown did not start the race\n";
        return 1;
    }

    return 0;
}