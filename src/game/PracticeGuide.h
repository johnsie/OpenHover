// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_PRACTICE_GUIDE_H
#define OPENHOVER_PRACTICE_GUIDE_H

enum class PracticeStep
{
    Accelerate,
    Steer,
    Checkpoint,
    Boost,
    Jump,
    Recover,
    Fire,
    Complete
};

class PracticeGuide
{
public:
    void Reset(bool pWeaponsAllowed);
    void ObserveInput(double pThrottle, double pSteering, bool pJump);
    void ObserveCheckpoint();
    void ObserveBoost();
    void ObserveRecovery();
    void ObserveFire();
    PracticeStep Step() const;
    int CompletedStepCount() const;
    int TotalStepCount() const;

private:
    void Complete(PracticeStep pStep);

    PracticeStep mStep = PracticeStep::Accelerate;
    bool mWeaponsAllowed = true;
};

#endif
