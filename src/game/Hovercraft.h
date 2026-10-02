// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_HOVERCRAFT_H
#define OPENHOVER_HOVERCRAFT_H

struct HovercraftInput
{
    double mThrottle = 0.0;
    double mSteering = 0.0;
    bool mBoost = false;
    bool mJump = false;
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
    double mBoostEnergy = 1.0;
    bool mBoosting = false;
    bool mHasPreviousPosition = false;
};

struct HovercraftTuning
{
    double mAcceleration = 28.0;
    double mReverseAcceleration = 14.0;
    double mLinearDrag = 0.58;
    double mTurnRate = 2.4;
    double mTravelDirectionResponse = 4.5;
    double mHoverHeight = 1.2;
    double mHoverSpring = 28.0;
    double mHoverDamping = 9.0;
    double mBoostAcceleration = 40.0;
    double mBoostDrainPerSecond = 0.42;
    double mBoostRechargePerSecond = 0.16;
    double mJumpImpulse = 8.0;
    double mJumpHeightThreshold = 0.12;
    double mLandingRestitution = 0.24;
};

class Hovercraft
{
public:
    explicit Hovercraft(const HovercraftTuning& pTuning = HovercraftTuning());

    void Reset(const HovercraftState& pState = HovercraftState());
    void Step(const HovercraftInput& pInput, double pSeconds);

    const HovercraftState& State() const { return mState; }

private:
    HovercraftTuning mTuning;
    HovercraftState mState;
    bool mJumpHeld = false;
};

#endif