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
    mJumping = false;
    if (mState.mHeight == 0.0)
        mState.mHeight = mTuning.mHoverHeight;
}

void ApplySpinOut(HovercraftState& pState, double pSeconds)
{
    pState.mSpinOutSeconds = std::max(pState.mSpinOutSeconds, pSeconds);
    pState.mBoosting = false;
    pState.mBoostEnergy = 0.0;
}

void Hovercraft::Step(const HovercraftInput& pInput, double pSeconds)
{
    if (pSeconds <= 0.0)
        return;

    const double hoverHeight = std::max(mTuning.mHoverHeight, mState.mSurfaceHeight);
    const bool spinningOut = mState.mSpinOutSeconds > 0.0;
    double throttle = spinningOut ? 0.0 : Clamp(pInput.mThrottle, -1.0, 1.0);
    const double steering = spinningOut ? 0.0 : Clamp(pInput.mSteering, -1.0, 1.0);
    const bool jumpPressed = !spinningOut && pInput.mJump && !mJumpHeld;
    mState.mReverseFacing = !spinningOut && pInput.mReverseFacing;
    mJumpHeld = pInput.mJump;
    if (throttle > 0.0)
    {
        mState.mFuel = Clamp(mState.mFuel - throttle * mTuning.mFuelBurnPerSecond * pSeconds, 0.0, 1.0);
        if (mState.mFuel <= 0.0)
            throttle = 0.0;
    }
    else
        mState.mFuel = Clamp(mState.mFuel + mTuning.mFuelRecoveryPerSecond * pSeconds, 0.0, 1.0);
    const double acceleration = throttle >= 0.0
        ? throttle * mTuning.mAcceleration
        : throttle * mTuning.mReverseAcceleration;
    const double thrustTarget = throttle > 0.0 ? throttle : 0.0;
    const double thrustBlend = Clamp(pSeconds * 8.0, 0.0, 1.0);
    mState.mEngineThrust += (thrustTarget - mState.mEngineThrust) * thrustBlend;
    mState.mBoosting = false;

    mState.mSpeed += acceleration * pSeconds;
    mState.mBoostEnergy = 1.0;
    mState.mSpeed /= 1.0 + mTuning.mLinearDrag * pSeconds;
    mState.mSpeed = Clamp(mState.mSpeed, -mTuning.mMaximumSpeed * 0.45, mTuning.mMaximumSpeed);
    if (spinningOut)
    {
        mState.mSpinOutSeconds = std::max(0.0, mState.mSpinOutSeconds - pSeconds);
        mState.mHeading = WrapAngle(mState.mHeading + 9.0 * pSeconds);
        mState.mTravelHeading = WrapAngle(mState.mTravelHeading + 6.0 * pSeconds);
    }
    else
        mState.mHeading += steering * mTuning.mTurnRate * pSeconds;
    const double travelAdjustment = Clamp(mTuning.mTravelDirectionResponse * pSeconds, 0.0, 1.0);
    mState.mTravelHeading += WrapAngle(mState.mHeading - mState.mTravelHeading) * travelAdjustment;
    mState.mPreviousX = mState.mX;
    mState.mPreviousY = mState.mY;
    mState.mHasPreviousPosition = true;
    mState.mX += std::cos(mState.mTravelHeading) * mState.mSpeed * pSeconds;
    mState.mY += std::sin(mState.mTravelHeading) * mState.mSpeed * pSeconds;

    if (jumpPressed && std::fabs(mState.mHeight - hoverHeight) <= mTuning.mJumpHeightThreshold)
    {
        mState.mVerticalSpeed = mTuning.mJumpImpulse;
        mJumping = true;
    }
    const double heightError = hoverHeight - mState.mHeight;
    const double hoverSpring = mJumping ? mTuning.mJumpHoverSpring : mTuning.mHoverSpring;
    const double hoverDamping = mJumping ? mTuning.mJumpHoverDamping : mTuning.mHoverDamping;
    mState.mVerticalSpeed += (heightError * hoverSpring - mState.mVerticalSpeed * hoverDamping)
        * pSeconds;
    mState.mHeight += mState.mVerticalSpeed * pSeconds;
    if (mState.mHeight < hoverHeight && mState.mVerticalSpeed < 0.0)
    {
        mState.mHeight = hoverHeight;
        mState.mVerticalSpeed = -mState.mVerticalSpeed * mTuning.mLandingRestitution;
        mJumping = false;
    }
}