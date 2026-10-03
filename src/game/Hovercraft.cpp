// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Hovercraft.h"

#include "Ground.h"

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
        mState.mHeight = mState.mGroundHeight + mTuning.mHoverHeight;
}

void Hovercraft::SetTuning(const HovercraftTuning& pTuning)
{
    mTuning = pTuning;
}

bool Hovercraft::ApplyGround(const GroundProfile& pGround)
{
    const bool blocked = ResolveGroundStep(mState, pGround);
    mState.mSurfaceHeight = mState.mGroundHeight;
    return blocked;
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

    if (mState.mHeight < mState.mGroundHeight)
    {
        // Placed under the ground (a spawn, a recovery): sit on it.
        mState.mHeight = mState.mGroundHeight + mTuning.mHoverHeight;
        mState.mVerticalSpeed = 0.0;
    }
    // A raised section the craft is over sets a surface above the ground; a surface level with the
    // ground (the default, zero on a flat track) means there is none.
    const double groundHover = mTuning.mHoverHeight + mState.mGroundHeight;
    const double hoverHeight = mState.mSurfaceHeight > mState.mGroundHeight
        ? std::max(groundHover, mState.mSurfaceHeight) : groundHover;
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
    const bool padBoosting = !spinningOut && mState.mPadBoostSeconds > 0.0;
    mState.mBoosting = padBoosting;

    mState.mSpeed += acceleration * pSeconds;
    if (padBoosting)
    {
        mState.mSpeed += mTuning.mBoostAcceleration * pSeconds;
        mState.mPadBoostSeconds = std::max(0.0, mState.mPadBoostSeconds - pSeconds);
    }
    mState.mBoostEnergy = 1.0;
    mState.mSpeed /= 1.0 + mTuning.mLinearDrag * pSeconds;
    const double maximumSpeed = padBoosting ? mTuning.mBoostMaximumSpeed : mTuning.mMaximumSpeed;
    mState.mSpeed = Clamp(mState.mSpeed, -mTuning.mMaximumSpeed * 0.45, maximumSpeed);
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

    if (jumpPressed && !mState.mFalling && std::fabs(mState.mHeight - hoverHeight) <= mTuning.mJumpHeightThreshold)
    {
        mState.mVerticalSpeed = mTuning.mJumpImpulse;
        mState.mFalling = true;
        mJumping = true;
    }
    if (mState.mFalling)
    {
        mState.mVerticalSpeed -= mTuning.mFallGravity * pSeconds;
        mState.mHeight += mState.mVerticalSpeed * pSeconds;
        if (mState.mHeight <= hoverHeight && mState.mVerticalSpeed <= 0.0)
        {
            mState.mHeight = hoverHeight;
            mState.mVerticalSpeed = -mState.mVerticalSpeed * mTuning.mLandingRestitution;
            mState.mFalling = false;
            mJumping = false;
        }
        return;
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