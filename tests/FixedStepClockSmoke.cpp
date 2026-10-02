// SPDX-License-Identifier: MIT OR Apache-2.0
#include "FixedStepClock.h"

#include <iostream>

int main()
{
    FixedStepClock clock(0.01, 4);
    if (clock.Consume(0.025) != 2 || clock.Consume(0.005) != 1)
    {
        std::cerr << "fixed clock did not preserve partial frame time\n";
        return 1;
    }

    if (clock.Consume(0.2) != 4)
    {
        std::cerr << "fixed clock did not cap catch-up work\n";
        return 1;
    }

    clock.Reset();
    if (clock.Consume(0.0) != 0 || clock.Consume(-1.0) != 0)
    {
        std::cerr << "fixed clock advanced for invalid frame time\n";
        return 1;
    }

    return 0;
}