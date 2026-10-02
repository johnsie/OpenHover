// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RivalController.h"
#include "RouteGuidance.h"

#include <cmath>

namespace
{
const double kAvoidanceHorizon = 16.0;
const double kAvoidanceMinimumAhead = 1.5;
const double kAvoidanceLane = 4.2;
const double kAvoidanceAlongsideBehind = 1.0;
const double kAvoidanceAlongsideLane = 3.0;
const double kAvoidanceBrakeDistance = 6.0;

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
    return InputFor(pState, std::vector<HovercraftState>());
}

HovercraftInput RivalController::InputFor(const HovercraftState& pState,
                                          const std::vector<HovercraftState>& pOthers) const
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
    double steering = headingError * mTuning.mSteeringGain;

    // Avoidance: find the closest craft that is ahead and in our lane, and push the steering to
    // the side it is not on. Craft level with us (the start grid) are ignored.
    const double forwardX = std::cos(pState.mHeading);
    const double forwardY = std::sin(pState.mHeading);
    // Look further ahead the faster we go, so there is room to steer around a craft.
    const double horizon = kAvoidanceHorizon + std::fabs(pState.mSpeed) * 0.8;
    double nearestAhead = horizon;
    double blockerSide = 0.0;
    for (const HovercraftState& other : pOthers)
    {
        const double relativeX = other.mX - pState.mX;
        const double relativeY = other.mY - pState.mY;
        const double ahead = relativeX * forwardX + relativeY * forwardY;
        const double side = forwardX * relativeY - forwardY * relativeX; // left is positive
        // A craft just alongside counts as touching distance, so the rival does not cut back
        // across it the moment it has steered clear of the nose.
        const bool alongside = ahead > -kAvoidanceAlongsideBehind && ahead <= kAvoidanceMinimumAhead
            && std::fabs(side) < kAvoidanceAlongsideLane;
        const bool inLane = ahead > kAvoidanceMinimumAhead && ahead < horizon
            && std::fabs(side) < kAvoidanceLane;
        const double effectiveAhead = alongside ? 0.0 : ahead;
        if ((alongside || inLane) && effectiveAhead < nearestAhead)
        {
            nearestAhead = effectiveAhead;
            blockerSide = side;
        }
    }
    if (nearestAhead < horizon)
    {
        const double urgency = 1.0 - nearestAhead / horizon;
        const double direction = blockerSide > 0.0 ? -1.0 : 1.0;
        steering += direction * (0.35 + 0.65 * urgency);
        if (nearestAhead < kAvoidanceBrakeDistance)
            input.mThrottle *= 0.85;
    }
    input.mSteering = Clamp(steering, -1.0, 1.0);
    return input;
}