// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_HOVERCRAFT_H
#define OPENHOVER_HOVERCRAFT_H

struct HovercraftInput
{
    double mThrottle = 0.0;
    double mSteering = 0.0;
    bool mBoost = false;
    bool mJump = false;
    bool mFire = false;
    bool mReverseFacing = false;
};

struct HovercraftState
{
    double mX = 0.0;
    double mY = 0.0;
    double mPreviousX = 0.0;
    double mPreviousY = 0.0;
    double mHeading = 0.0;
    double mTravelHeading = 0.0;
    double mSpeed = 0.0;
    double mHeight = 0.0;
    double mVerticalSpeed = 0.0;
    double mEngineThrust = 0.0;
    double mFuel = 1.0;
    double mBoostEnergy = 1.0;
    double mPadBoostSeconds = 0.0;
    double mSpinOutSeconds = 0.0;
    bool mBoosting = false;
    bool mHasPreviousPosition = false;
    bool mReverseFacing = false;
    double mSurfaceHeight = 0.0;
};

void ApplySpinOut(HovercraftState& pState, double pSeconds = 1.25);

struct HovercraftTuning
{
    double mAcceleration = 28.0;
    double mReverseAcceleration = 12.0;
    double mLinearDrag = 0.45;
    double mMaximumSpeed = 36.0;
    double mBoostMaximumSpeed = 42.0;
    double mTurnRate = 2.4;
    double mTravelDirectionResponse = 4.5;
    double mHoverHeight = 1.2;
    double mHoverSpring = 28.0;
    double mHoverDamping = 9.0;
    double mBoostAcceleration = 28.0;
    double mBoostDrainPerSecond = 0.42;
    double mBoostRechargePerSecond = 0.16;
    double mFuelBurnPerSecond = 0.006;
    double mFuelRecoveryPerSecond = 0.012;
    double mJumpImpulse = 7.4;
    double mJumpHoverSpring = 26.0;
    double mJumpHoverDamping = 7.0;
    double mJumpHeightThreshold = 0.12;
    double mLandingRestitution = 0.08;
};

class Hovercraft
{
public:
    explicit Hovercraft(const HovercraftTuning& pTuning = HovercraftTuning());

    void Reset(const HovercraftState& pState = HovercraftState());
    void SetTuning(const HovercraftTuning& pTuning);
    void Step(const HovercraftInput& pInput, double pSeconds);

    const HovercraftState& State() const { return mState; }

private:
    HovercraftTuning mTuning;
    HovercraftState mState;
    bool mJumpHeld = false;
    bool mJumping = false;
};

#endif