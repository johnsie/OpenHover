// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RACE_H
#define OPENHOVER_RACE_H

#include <vector>

struct RaceGate
{
    double mX = 0.0;
    double mY = 0.0;
    double mRadius = 1.0;
};

struct RaceProgress
{
    int mNextCheckpoint = 0;
    int mCompletedLaps = 0;
    double mElapsedSeconds = 0.0;
    bool mFinished = false;
};

class Race
{
public:
    Race(const std::vector<RaceGate>& pCheckpoints, const RaceGate& pFinish, int pTargetLaps = 3);

    void Reset();
    void Update(double pX, double pY, double pSeconds);

    const RaceProgress& Progress() const { return mProgress; }
    int TargetLaps() const { return mTargetLaps; }

private:
    bool IsInside(const RaceGate& pGate, double pX, double pY) const;

    std::vector<RaceGate> mCheckpoints;
    RaceGate mFinish;
    int mTargetLaps;
    RaceProgress mProgress;
    bool mInsideActiveGate = false;
};

#endif