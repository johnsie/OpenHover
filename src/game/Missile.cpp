// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Missile.h"

#include "WallCollision.h"

#include <algorithm>
#include <cmath>

namespace
{
const double kMissileAcceleration = 30.0;
const double kMissileMaximumSpeed = 54.0;
const double kMissileLifetime = 5.0;
const double kMissileSinkSeconds = 1.2;
const double kMissileSinkSpeed = 1.35;
const double kMissileRechargeSeconds = 10.0;
const double kMissileSpawnDistance = 1.65;
const double kMissileRadius = 0.22;
const double kMissileHitRadius = 1.0;
const double kHeightClearance = 0.35;
const double kPi = 3.14159265358979323846;
}

void Missile::Reset()
{
    mState = HovercraftState();
    mRemainingSeconds = 0.0;
    mSinkSeconds = 0.0;
    mRechargeSeconds = 0.0;
    mActive = false;
    mSinking = false;
}

double Missile::RechargeFraction() const
{
    return 1.0 - std::min(1.0, mRechargeSeconds / kMissileRechargeSeconds);
}

bool Missile::Fire(const HovercraftState& pSource)
{
    if (!Ready())
        return false;
    mState = pSource;
    const double launchHeading = mState.mHeading + (mState.mReverseFacing ? kPi : 0.0);
    mState.mX += std::cos(launchHeading) * kMissileSpawnDistance;
    mState.mY += std::sin(launchHeading) * kMissileSpawnDistance;
    mState.mTravelHeading = launchHeading;
    mState.mSpeed = std::max(20.0, std::fabs(pSource.mSpeed) + 12.0);
    mState.mVerticalSpeed = 0.0;
    mState.mSpinOutSeconds = 0.0;
    mState.mHasPreviousPosition = false;
    mRemainingSeconds = kMissileLifetime;
    mSinkSeconds = 0.0;
    mRechargeSeconds = kMissileRechargeSeconds;
    mActive = true;
    mSinking = false;
    return true;
}

void Missile::Step(double pSeconds, const Course& pCourse)
{
    if (pSeconds <= 0.0)
        return;
    mRechargeSeconds = std::max(0.0, mRechargeSeconds - pSeconds);
    if (!mActive)
        return;
    if (mSinking)
    {
        mState.mHeight -= kMissileSinkSpeed * pSeconds;
        mSinkSeconds -= pSeconds;
        if (mSinkSeconds <= 0.0 || mState.mHeight <= -0.2)
        {
            mState = HovercraftState();
            mActive = false;
            mSinking = false;
        }
        return;
    }
    mState.mPreviousX = mState.mX;
    mState.mPreviousY = mState.mY;
    mState.mHasPreviousPosition = true;
    mState.mSpeed = std::min(kMissileMaximumSpeed, mState.mSpeed + kMissileAcceleration * pSeconds);
    mState.mSpeed /= 1.0 + 0.12 * pSeconds;
    mState.mX += std::cos(mState.mTravelHeading) * mState.mSpeed * pSeconds;
    mState.mY += std::sin(mState.mTravelHeading) * mState.mSpeed * pSeconds;
    ResolveCourseWallCollision(mState, pCourse, kMissileRadius, 1.12);
    mRemainingSeconds -= pSeconds;
    if (mRemainingSeconds <= 0.0)
    {
        mSinking = true;
        mSinkSeconds = kMissileSinkSeconds;
        mState.mSpeed = 0.0;
        mState.mVerticalSpeed = -kMissileSinkSpeed;
    }
}

bool Missile::ApplyHit(HovercraftState& pTarget)
{
    if (!mActive || mSinking || std::fabs(pTarget.mHeight - mState.mHeight) >= kHeightClearance)
        return false;
    const double deltaX = pTarget.mX - mState.mX;
    const double deltaY = pTarget.mY - mState.mY;
    if (deltaX * deltaX + deltaY * deltaY > kMissileHitRadius * kMissileHitRadius)
        return false;
    ApplySpinOut(pTarget);
    mState = HovercraftState();
    mRemainingSeconds = 0.0;
    mSinkSeconds = 0.0;
    mActive = false;
    mSinking = false;
    return true;
}