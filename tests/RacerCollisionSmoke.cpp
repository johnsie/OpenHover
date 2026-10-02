// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RacerCollision.h"

#include <cmath>
#include <iostream>

int main()
{
    HovercraftState first;
    first.mSpeed = 10.0;
    HovercraftState second;
    second.mX = 1.0;
    second.mSpeed = 6.0;
    if (!ResolveRacerCollision(first, second, 1.25))
    {
        std::cerr << "overlapping racers were not resolved\n";
        return 1;
    }

    const double deltaX = second.mX - first.mX;
    const double deltaY = second.mY - first.mY;
    if (std::fabs(std::sqrt(deltaX * deltaX + deltaY * deltaY) - 2.5) > 0.0001
        || std::fabs(first.mSpeed + second.mSpeed - 16.0) > 0.0001
        || first.mSpeed >= 10.0 || second.mSpeed <= 6.0)
    {
        std::cerr << "racer collision had the wrong separation or momentum\n";
        return 1;
    }

    if (ResolveRacerCollision(first, second, 1.25))
    {
        std::cerr << "separated racers were resolved again\n";
        return 1;
    }

    first = HovercraftState();
    second = HovercraftState();
    first.mHeight = 1.6;
    second.mHeight = 1.2;
    second.mX = 1.0;
    if (ResolveRacerCollision(first, second, 1.25))
    {
        std::cerr << "airborne racer did not clear another craft\n";
        return 1;
    }

    return 0;
}