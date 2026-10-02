// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RIVAL_CONTROLLER_H
#define OPENHOVER_RIVAL_CONTROLLER_H

#include "Hovercraft.h"
#include "Race.h"

#include <vector>

enum class RivalDifficulty
{
    Relaxed,
    Standard,
    Expert
};

struct RivalTuning
{
    double mPace = 1.0;
    double mSteeringGain = 1.5;
    double mCornerLookAheadDistance = 12.0;
};

RivalDifficulty NextRivalDifficulty(RivalDifficulty pDifficulty);
const char* RivalDifficultyName(RivalDifficulty pDifficulty);
RivalTuning TuneRivalForDifficulty(const RivalTuning& pBaseTuning, RivalDifficulty pDifficulty);

class RivalController
{
public:
    explicit RivalController(const std::vector<RaceGate>& pRoute,
                             const RivalTuning& pTuning = RivalTuning());

    void Reset();
    void Update(const HovercraftState& pState);
    HovercraftInput InputFor(const HovercraftState& pState) const;
    int TargetIndex() const { return mTargetIndex; }

private:
    std::vector<RaceGate> mRoute;
    RivalTuning mTuning;
    int mTargetIndex = 0;
};

#endif