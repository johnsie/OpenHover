// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RouteGuidance.h"

#include <cmath>
#include <iostream>

int main()
{
    HovercraftState state;
    RaceGate gate = {10.0, 0.0, 1.0};
    RaceGate northGate = {0.0, 10.0, 1.0};
    RaceGate westGate = {-10.0, 0.0, 1.0};
    const double pi = 3.14159265358979323846;
    if (std::fabs(GetGateDirection(state, gate)) > 0.001
        || std::fabs(GetGateDirection(state, northGate) - pi / 2.0) > 0.001
        || std::fabs(std::fabs(GetGateDirection(state, westGate)) - pi) > 0.001)
    {
        std::cerr << "gate directions did not match cardinal headings\n";
        return 1;
    }

    if (IsHeadingAwayFromGate(state, gate))
    {
        std::cerr << "stationary hovercraft was marked wrong way\n";
        return 1;
    }

    state.mSpeed = 10.0;
    state.mTravelHeading = 0.0;
    state.mHeading = 3.14159265358979323846;
    if (IsHeadingAwayFromGate(state, gate))
    {
        std::cerr << "drifting hovercraft was marked wrong way\n";
        return 1;
    }

    state.mTravelHeading = 3.14159265358979323846;
    if (!IsHeadingAwayFromGate(state, gate))
    {
        std::cerr << "away-traveling hovercraft was not marked wrong way\n";
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