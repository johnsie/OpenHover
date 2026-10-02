// SPDX-License-Identifier: MIT OR Apache-2.0
#include "SteeringAssist.h"

#include <iostream>

int main()
{
    HovercraftState state;
    HovercraftInput input;
    RaceGate northGate = {0.0, 10.0, 1.0};
    HovercraftInput assisted = ApplySteeringAssist(input, state, northGate);
    if (assisted.mSteering <= 0.0 || assisted.mThrottle != input.mThrottle)
    {
        std::cerr << "steering assist did not guide the craft toward its target\n";
        return 1;
    }

    input.mSteering = -1.0;
    assisted = ApplySteeringAssist(input, state, northGate);
    if (assisted.mSteering <= -1.0 || assisted.mSteering >= 0.0)
    {
        std::cerr << "steering assist did not preserve player steering authority\n";
        return 1;
    }

    RaceGate currentGate = {0.0, 0.0, 1.0};
    assisted = ApplySteeringAssist(input, state, currentGate);
    if (assisted.mSteering != input.mSteering)
    {
        std::cerr << "steering assist changed input at the target\n";
        return 1;
    }

    state.mSpeed = 12.0;
    input.mThrottle = 1.0;
    assisted = ApplyBrakingAssist(input, state, northGate);
    if (assisted.mThrottle >= input.mThrottle || assisted.mSteering != input.mSteering)
    {
        std::cerr << "braking assist did not reduce throttle for a sharp turn\n";
        return 1;
    }

    state.mHeading = 1.5707963267948966;
    assisted = ApplyBrakingAssist(input, state, northGate);
    if (assisted.mThrottle != input.mThrottle)
    {
        std::cerr << "braking assist reduced throttle on a straight\n";
        return 1;
    }

    state.mHeading = 3.14159265358979323846;
    assisted = ApplyBrakingAssist(input, state, currentGate);
    if (assisted.mThrottle != input.mThrottle)
    {
        std::cerr << "braking assist reduced throttle inside the target gate\n";
        return 1;
    }
    return 0;
}