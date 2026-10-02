// SPDX-License-Identifier: MIT OR Apache-2.0
#include "HazardZone.h"

#include <iostream>

int main()
{
    HazardZone zone = {0.0, 0.0, 2.0, 1.0};
    HovercraftState state;
    state.mSpeed = 10.0;
    if (!ApplyHazardZone(state, zone, 1.0) || state.mSpeed >= 10.0 || state.mSpeed <= 0.0)
    {
        std::cerr << "hazard zone did not apply a time-scaled speed penalty\n";
        return 1;
    }
    state.mX = 3.0;
    const double speed = state.mSpeed;
    if (ApplyHazardZone(state, zone, 1.0) || state.mSpeed != speed)
    {
        std::cerr << "hazard zone affected a craft outside its radius\n";
        return 1;
    }
    state.mX = 0.0;
    state.mHeight = 1.5;
    if (ApplyHazardZone(state, zone, 1.0) || state.mSpeed != speed)
    {
        std::cerr << "hazard zone affected an airborne craft\n";
        return 1;
    }
    return 0;
}