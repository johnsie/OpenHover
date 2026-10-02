// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RivalController.h"

#include <iostream>
#include <vector>

int main()
{
    std::vector<RaceGate> route;
    route.push_back({10.0, 0.0, 1.0});
    route.push_back({10.0, 10.0, 1.0});
    RivalController controller(route);
    HovercraftState state;

    HovercraftInput initialInput = controller.InputFor(state);
    if (initialInput.mThrottle != 1.0 || initialInput.mBoost)
    {
        std::cerr << "rival did not accelerate directly toward its first target\n";
        return 1;
    }

    state.mX = 10.0;
    controller.Update(state);
    if (controller.TargetIndex() != 1)
    {
        std::cerr << "rival did not advance to the next route target\n";
        return 1;
    }

    HovercraftInput turnInput = controller.InputFor(state);
    if (turnInput.mSteering <= 0.0)
    {
        std::cerr << "rival did not steer toward its next target\n";
        return 1;
    }

    RivalTuning cautiousTuning;
    cautiousTuning.mPace = 0.65;
    RivalController cautiousController(route, cautiousTuning);
    HovercraftInput cautiousInput = cautiousController.InputFor(HovercraftState());
    if (cautiousInput.mThrottle != 0.65 || cautiousInput.mBoost)
    {
        std::cerr << "rival tuning did not control pace\n";
        return 1;
    }

    const RivalTuning relaxedTuning = TuneRivalForDifficulty(cautiousTuning, RivalDifficulty::Relaxed);
    const RivalTuning standardTuning = TuneRivalForDifficulty(cautiousTuning, RivalDifficulty::Standard);
    const RivalTuning expertTuning = TuneRivalForDifficulty(cautiousTuning, RivalDifficulty::Expert);
    if (relaxedTuning.mPace >= standardTuning.mPace || relaxedTuning.mSteeringGain >= standardTuning.mSteeringGain
        || expertTuning.mPace <= standardTuning.mPace || expertTuning.mSteeringGain <= standardTuning.mSteeringGain
        || expertTuning.mCornerLookAheadDistance <= standardTuning.mCornerLookAheadDistance
        || NextRivalDifficulty(RivalDifficulty::Expert) != RivalDifficulty::Relaxed)
    {
        std::cerr << "rival difficulty did not adjust tuning predictably\n";
        return 1;
    }

    RivalController standardController(route, standardTuning);
    RivalController expertController(route, expertTuning);
    HovercraftState approachState;
    approachState.mX = 2.0;
    const HovercraftInput standardApproach = standardController.InputFor(approachState);
    const HovercraftInput expertApproach = expertController.InputFor(approachState);
    if (expertApproach.mSteering <= standardApproach.mSteering || expertApproach.mBoost)
    {
        std::cerr << "expert rival did not anticipate the upcoming corner\n";
        return 1;
    }

    return 0;
}