// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RivalController.h"

#include <cmath>
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

    // Avoidance: a craft close ahead pushes the steering to the other side; others are ignored.
    std::vector<RaceGate> straight = {{200.0, 0.0, 3.0}, {400.0, 0.0, 3.0}};
    RivalController racer(straight);
    HovercraftState self;
    self.mHeading = 0.0;
    const double freeSteering = racer.InputFor(self).mSteering;
    HovercraftState blocker;
    blocker.mX = 8.0;
    blocker.mY = 1.0; // slightly to the left (positive y is left of heading 0)
    const HovercraftInput leftBlocked = racer.InputFor(self, {blocker});
    blocker.mY = -1.0;
    const HovercraftInput rightBlocked = racer.InputFor(self, {blocker});
    HovercraftState farAway = blocker;
    farAway.mX = 60.0;
    HovercraftState behind = blocker;
    behind.mX = -8.0;
    HovercraftState level = blocker;
    level.mX = 0.5;
    level.mY = 3.2; // a start-grid neighbour beside us
    HovercraftState otherLane = blocker;
    otherLane.mY = 9.0;
    if (leftBlocked.mSteering >= freeSteering - 0.3 || rightBlocked.mSteering <= freeSteering + 0.3
        || racer.InputFor(self, {farAway}).mSteering != freeSteering
        || racer.InputFor(self, {behind}).mSteering != freeSteering
        || racer.InputFor(self, {level}).mSteering != freeSteering
        || racer.InputFor(self, {otherLane}).mSteering != freeSteering)
    {
        std::cerr << "rival avoidance did not steer away from a close craft ahead only\n";
        return 1;
    }

    // Firing: a craft lined up ahead within range draws a missile; nothing else does.
    HovercraftState target;
    target.mX = 25.0;
    target.mY = 0.5;
    HovercraftState offLane = target;
    offLane.mY = 6.0;
    HovercraftState tooFar = target;
    tooFar.mX = 200.0;
    HovercraftState tooClose = target;
    tooClose.mX = 3.0;
    HovercraftState behindUs = target;
    behindUs.mX = -25.0;
    if (!racer.InputFor(self, {target}).mFire || racer.InputFor(self, {offLane}).mFire
        || racer.InputFor(self, {tooFar}).mFire || racer.InputFor(self, {tooClose}).mFire
        || racer.InputFor(self, {behindUs}).mFire || racer.InputFor(self).mFire)
    {
        std::cerr << "rival did not fire only at a craft lined up ahead\n";
        return 1;
    }
    RivalController relaxed(straight, TuneRivalForDifficulty(RivalTuning(), RivalDifficulty::Relaxed));
    RivalController expert(straight, TuneRivalForDifficulty(RivalTuning(), RivalDifficulty::Expert));
    if (relaxed.InputFor(self, {target}).mFire || !expert.InputFor(self, {target}).mFire)
    {
        std::cerr << "relaxed rivals must not fire, and expert rivals must\n";
        return 1;
    }

    // After a rival is moved (recovered), it aims at the waypoint ahead of its new position, not at
    // whatever it was chasing before.
    std::vector<RaceGate> loop = {{0.0, 0.0, 3.0}, {100.0, 0.0, 3.0}, {100.0, 100.0, 3.0}, {0.0, 100.0, 3.0}};
    RivalController retargeting(loop);
    HovercraftState onFirstSide;
    onFirstSide.mX = 50.0;
    onFirstSide.mY = 0.0;
    retargeting.Retarget(onFirstSide);
    const int afterFirst = retargeting.TargetIndex();
    HovercraftState onThirdSide;
    onThirdSide.mX = 50.0;
    onThirdSide.mY = 100.0;
    retargeting.Retarget(onThirdSide);
    const int afterThird = retargeting.TargetIndex();
    HovercraftState onClosingSide;
    onClosingSide.mX = 0.0;
    onClosingSide.mY = 50.0;
    retargeting.Retarget(onClosingSide);
    if (afterFirst != 1 || afterThird != 3 || retargeting.TargetIndex() != 0)
    {
        std::cerr << "retargeting did not aim at the next waypoint ahead: " << afterFirst << ", "
                  << afterThird << ", " << retargeting.TargetIndex() << "\n";
        return 1;
    }

    // Driving past a parked craft in the middle of a wide road without touching it.
    std::vector<RaceGate> longRoad = {{300.0, 0.0, 3.0}, {600.0, 0.0, 3.0}};
    RivalController passer(longRoad);
    Hovercraft craft;
    HovercraftState start;
    start.mX = -40.0;
    craft.Reset(start);
    HovercraftState parked;
    parked.mX = 0.0;
    double closest = 1e9;
    for (int step = 0; step < 120 * 10; ++step)
    {
        passer.Update(craft.State());
        craft.Step(passer.InputFor(craft.State(), {parked}), 1.0 / 120.0);
        const HovercraftState& now = craft.State();
        const double gap = std::hypot(now.mX - parked.mX, now.mY - parked.mY);
        if (gap < closest)
            closest = gap;
    }
    if (closest < 2.2 || craft.State().mX < 40.0)
    {
        std::cerr << "rival did not pass a parked craft cleanly: closest " << closest
                  << " m, ended at x=" << craft.State().mX << "\n";
        return 1;
    }
    return 0;
}
