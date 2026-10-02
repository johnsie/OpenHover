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
    // Whether the rival fires missiles at craft lined up ahead of it.
    bool mFiresMissiles = true;
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
    // Aim at the next waypoint ahead of where the craft now is. Call after the craft has been moved
    // (for example recovered to the road), so the old target cannot be behind it or through a wall.
    void Retarget(const HovercraftState& pState);
    HovercraftInput InputFor(const HovercraftState& pState) const;
    // Same, but steers around any craft in pOthers that is close ahead, so rivals pass each other
    // and the player instead of driving through them.
    HovercraftInput InputFor(const HovercraftState& pState,
                             const std::vector<HovercraftState>& pOthers) const;
    int TargetIndex() const { return mTargetIndex; }

private:
    std::vector<RaceGate> mRoute;
    RivalTuning mTuning;
    int mTargetIndex = 0;
};

#endif