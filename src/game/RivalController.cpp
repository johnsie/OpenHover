// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RivalController.h"
#include "RouteGuidance.h"

#include <cmath>

namespace
{
const double kPi = 3.14159265358979323846;

double Clamp(double pValue, double pMinimum, double pMaximum)
{
    return pValue < pMinimum ? pMinimum : (pValue > pMaximum ? pMaximum : pValue);
}

double WrapAngle(double pAngle)
{
    while (pAngle > kPi)
        pAngle -= 2.0 * kPi;
    while (pAngle < -kPi)
        pAngle += 2.0 * kPi;
    return pAngle;
}
}

RivalDifficulty NextRivalDifficulty(RivalDifficulty pDifficulty)
{
    switch (pDifficulty)
    {
    case RivalDifficulty::Relaxed:
        return RivalDifficulty::Standard;
    case RivalDifficulty::Standard:
        return RivalDifficulty::Expert;
    case RivalDifficulty::Expert:
        return RivalDifficulty::Relaxed;
    }
    return RivalDifficulty::Standard;
}

const char* RivalDifficultyName(RivalDifficulty pDifficulty)
{
    switch (pDifficulty)
    {
    case RivalDifficulty::Relaxed:
        return "Relaxed";
    case RivalDifficulty::Standard:
        return "Standard";
    case RivalDifficulty::Expert:
        return "Expert";
    }
    return "Standard";
}

RivalTuning TuneRivalForDifficulty(const RivalTuning& pBaseTuning, RivalDifficulty pDifficulty)
{
    RivalTuning tuning = pBaseTuning;
    switch (pDifficulty)
    {
    case RivalDifficulty::Relaxed:
        tuning.mPace *= 0.82;
        tuning.mSteeringGain *= 0.82;
        tuning.mCornerLookAheadDistance *= 0.6;
        break;
    case RivalDifficulty::Expert:
        tuning.mPace = Clamp(tuning.mPace + 0.08, 0.0, 1.0);
        tuning.mSteeringGain *= 1.12;
        tuning.mCornerLookAheadDistance *= 1.8;
        break;
    case RivalDifficulty::Standard:
        break;
    }
    return tuning;
}

RivalController::RivalController(const std::vector<RaceGate>& pRoute, const RivalTuning& pTuning)
        : mRoute(pRoute),
            mTuning(pTuning)
{
}

void RivalController::Reset()
{
    mTargetIndex = 0;
}

void RivalController::Update(const HovercraftState& pState)
{
    if (mRoute.empty())
        return;

    const RaceGate& target = mRoute[mTargetIndex];
    const double deltaX = pState.mX - target.mX;
    const double deltaY = pState.mY - target.mY;
    if (deltaX * deltaX + deltaY * deltaY <= target.mRadius * target.mRadius)
        mTargetIndex = (mTargetIndex + 1) % static_cast<int>(mRoute.size());
}

HovercraftInput RivalController::InputFor(const HovercraftState& pState) const
{
    HovercraftInput input;
    if (mRoute.empty())
        return input;

    const RaceGate& target = mRoute[mTargetIndex];
    const double deltaX = target.mX - pState.mX;
    const double deltaY = target.mY - pState.mY;
    const double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    double aimX = target.mX;
    double aimY = target.mY;
    if (mRoute.size() > 1 && distance < mTuning.mCornerLookAheadDistance)
    {
        const RaceGate& nextTarget = mRoute[(mTargetIndex + 1) % mRoute.size()];
        const double nextDeltaX = nextTarget.mX - target.mX;
        const double nextDeltaY = nextTarget.mY - target.mY;
        const double nextDistance = std::sqrt(nextDeltaX * nextDeltaX + nextDeltaY * nextDeltaY);
        if (nextDistance > 0.0)
        {
            const double blend = Clamp(1.0 - distance / mTuning.mCornerLookAheadDistance, 0.0, 1.0);
            const double lookAhead = std::fmin(mTuning.mCornerLookAheadDistance, nextDistance * 0.45);
            aimX += nextDeltaX / nextDistance * lookAhead * blend;
            aimY += nextDeltaY / nextDistance * lookAhead * blend;
        }
    }
    const double desiredHeading = std::atan2(aimY - pState.mY, aimX - pState.mX);
    const double headingError = WrapAngle(desiredHeading - pState.mHeading);
    input.mThrottle = Clamp(mTuning.mPace, 0.0, 1.0);
    input.mSteering = Clamp(headingError * mTuning.mSteeringGain, -1.0, 1.0);
    return input;
}