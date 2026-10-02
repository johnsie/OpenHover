// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Hovercraft.h"

#include <cmath>
#include <iostream>

namespace
{
bool NearlyEqual(double pActual, double pExpected, double pTolerance)
{
    return std::fabs(pActual - pExpected) <= pTolerance;
}
}

int main()
{
    Hovercraft hovercraft;
    HovercraftInput forward;
    forward.mThrottle = 1.0;
    for (int step = 0; step < 60; ++step)
        hovercraft.Step(forward, 1.0 / 60.0);

    const HovercraftState afterForward = hovercraft.State();
    if (afterForward.mSpeed < 18.0 || afterForward.mX <= 0.0 || afterForward.mFuel >= 1.0)
    {
        std::cerr << "forward input did not build sufficient racing speed\n";
        return 1;
    }
    for (int step = 0; step < 180; ++step)
        hovercraft.Step(forward, 1.0 / 60.0);
    if (!NearlyEqual(hovercraft.State().mSpeed, 36.0, 0.001))
    {
        std::cerr << "forward input did not reach the tuned maximum speed\n";
        return 1;
    }
    for (int step = 0; step < 120; ++step)
        hovercraft.Step(HovercraftInput(), 1.0 / 60.0);
    if (hovercraft.State().mFuel <= afterForward.mFuel)
    {
        std::cerr << "fuel did not recover while coasting\n";
        return 1;
    }

    Hovercraft boostedHovercraft;
    HovercraftInput boost;
    boost.mThrottle = 1.0;
    boost.mBoost = true;
    for (int step = 0; step < 60; ++step)
        boostedHovercraft.Step(boost, 1.0 / 60.0);
    if (!NearlyEqual(boostedHovercraft.State().mSpeed, afterForward.mSpeed, 0.001)
        || boostedHovercraft.State().mBoosting)
    {
        std::cerr << "disabled boost changed hovercraft speed\n";
        return 1;
    }

    HovercraftInput turn;
    turn.mSteering = 1.0;
    hovercraft.Step(turn, 0.5);
    if (hovercraft.State().mHeading <= afterForward.mHeading)
    {
        std::cerr << "steering input did not change heading\n";
        return 1;
    }

    HovercraftInput reverseFacing;
    reverseFacing.mReverseFacing = true;
    hovercraft.Step(reverseFacing, 1.0 / 60.0);
    if (!hovercraft.State().mReverseFacing)
    {
        std::cerr << "reverse-facing input did not update craft state\n";
        return 1;
    }

    Hovercraft driftingHovercraft;
    HovercraftInput drift;
    drift.mThrottle = 1.0;
    drift.mSteering = 1.0;
    driftingHovercraft.Step(drift, 0.1);
    if (driftingHovercraft.State().mHeading <= driftingHovercraft.State().mTravelHeading
        || driftingHovercraft.State().mTravelHeading <= 0.0)
    {
        std::cerr << "hovercraft did not carry travel direction through a turn\n";
        return 1;
    }

    HovercraftState driftingState;
    driftingState.mHeading = 0.6;
    driftingState.mTravelHeading = 0.3;
    driftingHovercraft.Reset(driftingState);
    if (!NearlyEqual(driftingHovercraft.State().mTravelHeading, 0.3, 0.0001))
    {
        std::cerr << "hovercraft reset did not preserve travel direction\n";
        return 1;
    }

    HovercraftState belowHoverHeight;
    belowHoverHeight.mHeight = 0.2;
    hovercraft.Reset(belowHoverHeight);
    for (int step = 0; step < 600; ++step)
        hovercraft.Step(HovercraftInput(), 1.0 / 120.0);

    if (!NearlyEqual(hovercraft.State().mHeight, 1.2, 0.01))
    {
        std::cerr << "hover height did not settle\n";
        return 1;
    }

    Hovercraft jumpingHovercraft;
    HovercraftInput jump;
    jump.mJump = true;
    double peakHeight = jumpingHovercraft.State().mHeight;
    int airborneSteps = 0;
    bool descendedAfterPeak = false;
    bool landedWithRebound = false;
    for (int step = 0; step < 360; ++step)
    {
        jumpingHovercraft.Step(jump, 1.0 / 120.0);
        peakHeight = std::fmax(peakHeight, jumpingHovercraft.State().mHeight);
        airborneSteps += jumpingHovercraft.State().mHeight > 1.22 ? 1 : 0;
        descendedAfterPeak = descendedAfterPeak
            || jumpingHovercraft.State().mVerticalSpeed < 0.0;
        landedWithRebound = landedWithRebound
            || (NearlyEqual(jumpingHovercraft.State().mHeight, 1.2, 0.0001)
                && jumpingHovercraft.State().mVerticalSpeed > 0.0);
    }
    if (peakHeight < 1.45 || peakHeight > 2.5 || airborneSteps < 60 || airborneSteps > 150
        || !descendedAfterPeak || !landedWithRebound
        || !NearlyEqual(jumpingHovercraft.State().mHeight, 1.2, 0.01))
    {
        std::cerr << "jump did not launch once and return to hover height\n";
        return 1;
    }

    return 0;
}