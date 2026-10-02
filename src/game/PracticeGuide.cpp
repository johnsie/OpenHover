// SPDX-License-Identifier: MIT OR Apache-2.0
#include "PracticeGuide.h"

#include <cmath>

void PracticeGuide::Reset(bool pWeaponsAllowed)
{
    mStep = PracticeStep::Accelerate;
    mWeaponsAllowed = pWeaponsAllowed;
}

void PracticeGuide::ObserveInput(double pThrottle, double pSteering, bool pJump)
{
    if (mStep == PracticeStep::Accelerate && pThrottle > 0.5)
    {
        Complete(PracticeStep::Accelerate);
        return;
    }
    if (mStep == PracticeStep::Steer && std::fabs(pSteering) > 0.35)
    {
        Complete(PracticeStep::Steer);
        return;
    }
    if (mStep == PracticeStep::Jump && pJump)
        Complete(PracticeStep::Jump);
}

void PracticeGuide::ObserveCheckpoint()
{
    Complete(PracticeStep::Checkpoint);
}

void PracticeGuide::ObserveBoost()
{
    Complete(PracticeStep::Boost);
}

void PracticeGuide::ObserveRecovery()
{
    Complete(PracticeStep::Recover);
}

void PracticeGuide::ObserveFire()
{
    Complete(PracticeStep::Fire);
}

PracticeStep PracticeGuide::Step() const
{
    return mStep;
}

int PracticeGuide::CompletedStepCount() const
{
    return static_cast<int>(mStep);
}

int PracticeGuide::TotalStepCount() const
{
    return mWeaponsAllowed ? 7 : 6;
}

void PracticeGuide::Complete(PracticeStep pStep)
{
    if (mStep != pStep)
        return;
    mStep = static_cast<PracticeStep>(static_cast<int>(mStep) + 1);
    if (!mWeaponsAllowed && mStep == PracticeStep::Fire)
        mStep = PracticeStep::Complete;
}
