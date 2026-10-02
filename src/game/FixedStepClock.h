// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_FIXED_STEP_CLOCK_H
#define OPENHOVER_FIXED_STEP_CLOCK_H

class FixedStepClock
{
public:
    explicit FixedStepClock(double pStepSeconds = 1.0 / 120.0, int pMaximumSteps = 8);

    int Consume(double pFrameSeconds);
    double StepSeconds() const { return mStepSeconds; }
    void Reset();

private:
    double mStepSeconds;
    double mAccumulator = 0.0;
    int mMaximumSteps;
};

#endif