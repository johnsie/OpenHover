// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_MISSILE_H
#define OPENHOVER_MISSILE_H

#include "Course.h"
#include "Hovercraft.h"

class Missile
{
public:
    void Reset();
    bool Fire(const HovercraftState& pSource);
    void Step(double pSeconds, const Course& pCourse);
    bool ApplyHit(HovercraftState& pTarget);

    const HovercraftState& State() const { return mState; }
    bool Active() const { return mActive; }
    bool Sinking() const { return mSinking; }
    bool Ready() const { return !mActive && mRechargeSeconds <= 0.0; }
    double RechargeFraction() const;

private:
    HovercraftState mState;
    double mRemainingSeconds = 0.0;
    double mSinkSeconds = 0.0;
    double mRechargeSeconds = 0.0;
    bool mActive = false;
    bool mSinking = false;
};

#endif