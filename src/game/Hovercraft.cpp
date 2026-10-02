// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Hovercraft.h"

#include <algorithm>
#include <cmath>

namespace
{
double Clamp(double pValue, double pMinimum, double pMaximum)
{
    return std::max(pMinimum, std::min(pValue, pMaximum));
}

double WrapAngle(double pAngle)
{
    while (pAngle > 3.14159265358979323846)
        pAngle -= 6.28318530717958647692;
    while (pAngle < -3.14159265358979323846)
        pAngle += 6.28318530717958647692;
    return pAngle;
}
}

Hovercraft::Hovercraft(const HovercraftTuning& pTuning)
    : mTuning(pTuning)
{
    Reset();
}

void Hovercraft::Reset(const HovercraftState& pState)
{
    mState = pState;
    mJumpHeld = false;
    if (mState.mHeight == 0.0)
        mState.mHeight = mTuning.mHoverHeight;
}

void Hovercraft::Step(const HovercraftInput& pInput, double pSeconds)
{
    if (pSeconds <= 0.0)
        return;

    const double throttle = Clamp(pInput.mThrottle, -1.0, 1.0);
    const double steering = Clamp(pInput.mSteering, -1.0, 1.0);
    const bool jumpPressed = pInput.mJump && !mJumpHeld;
    mJumpHeld = pInput.mJump;
    const double acceleration = throttle >= 0.0
        ? throttle * mTuning.mAcceleration
        : throttle * mTuning.mReverseAcceleration;
    mState.mBoosting = pInput.mBoost && throttle > 0.0 && mState.mBoostEnergy > 0.0;

    mState.mSpeed += acceleration * pSeconds;
    if (mState.mBoosting)
    {
        mState.mSpeed += mTuning.mBoostAcceleration * pSeconds;
        mState.mBoostEnergy = Clamp(mState.mBoostEnergy - mTuning.mBoostDrainPerSecond * pSeconds,
                                    0.0, 1.0);
    }
    else
    {
        mState.mBoostEnergy = Clamp(mState.mBoostEnergy + mTuning.mBoostRechargePerSecond * pSeconds,
                                    0.0, 1.0);
    }
    mState.mSpeed /= 1.0 + mTuning.mLinearDrag * pSeconds;
    mState.mHeading += steering * mTuning.mTurnRate * pSeconds;
    const double travelAdjustment = Clamp(mTuning.mTravelDirectionResponse * pSeconds, 0.0, 1.0);
    mState.mTravelHeading += WrapAngle(mState.mHeading - mState.mTravelHeading) * travelAdjustment;
    mState.mPreviousX = mState.mX;
    mState.mPreviousY = mState.mY;
    mState.mHasPreviousPosition = true;
    mState.mX += std::cos(mState.mTravelHeading) * mState.mSpeed * pSeconds;
    mState.mY += std::sin(mState.mTravelHeading) * mState.mSpeed * pSeconds;

    if (jumpPressed && std::fabs(mState.mHeight - mTuning.mHoverHeight) <= mTuning.mJumpHeightThreshold)
        mState.mVerticalSpeed = mTuning.mJumpImpulse;
    const double heightError = mTuning.mHoverHeight - mState.mHeight;
    mState.mVerticalSpeed += (heightError * mTuning.mHoverSpring
                              - mState.mVerticalSpeed * mTuning.mHoverDamping) * pSeconds;
    mState.mHeight += mState.mVerticalSpeed * pSeconds;
    if (mState.mHeight < mTuning.mHoverHeight && mState.mVerticalSpeed < 0.0)
    {
        mState.mHeight = mTuning.mHoverHeight;
        mState.mVerticalSpeed = -mState.mVerticalSpeed * mTuning.mLandingRestitution;
    }
}