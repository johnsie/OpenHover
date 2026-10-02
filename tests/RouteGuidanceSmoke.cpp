// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RouteGuidance.h"

#include <iostream>

int main()
{
    HovercraftState state;
    RaceGate gate = {10.0, 0.0, 1.0};
    if (IsHeadingAwayFromGate(state, gate))
    {
        std::cerr << "forward-facing hovercraft was marked wrong way\n";
        return 1;
    }

    state.mHeading = 3.14159265358979323846;
    if (!IsHeadingAwayFromGate(state, gate))
    {
        std::cerr << "reverse-facing hovercraft was not marked wrong way\n";
        return 1;
    }

    state.mX = 10.0;
    if (IsHeadingAwayFromGate(state, gate))
    {
        std::cerr << "hovercraft inside a gate was marked wrong way\n";
        return 1;
    }
    return 0;
}