// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_GROUND_H
#define OPENHOVER_GROUND_H

#include "Race.h"

#include <string>
#include <vector>

struct HovercraftState;

// The height of the ground along a track, in level stretches with sudden steps between them. A
// track gives a height for each stretch of route (the one from a waypoint to the next); the ground
// is level across it and steps up or down where one stretch meets the next, across the road at the
// waypoint. Off the road it takes the height of the nearest part of the route. A track with no
// heights is flat at zero.
class GroundProfile
{
public:
    GroundProfile() = default;
    GroundProfile(const std::vector<RaceGate>& pRoute, const std::vector<double>& pHeights);

    bool Flat() const { return mRoute.empty(); }
    double HeightAt(double pX, double pY) const;
    // The height of the stretch from waypoint pIndex to the next (zero when flat).
    double SegmentHeight(std::size_t pIndex) const;

private:
    std::vector<RaceGate> mRoute;
    std::vector<double> mHeights;
};

// To get onto a higher floor a craft has to be at least this far above it; resting on the old floor
// it is 1.2 above that, so any step up of more than a quarter metre has to be jumped (the craft
// never rises by itself). A step up over kMaximumStepUp is beyond any jump: the wall of a pit, a trap.
constexpr double kStepClearance = 0.95;
constexpr double kMaximumStepUp = 2.1;
constexpr double kMaximumGroundHeight = 60.0;

// A short reason the heights cannot be used, or an empty string. Heights must be empty or match the
// route and stay within range, and two parts of the route that cross must be at the same height (a
// bridge carries the crossing, not a change of level). Steps of any size are allowed: a drop is a
// fall, and a wall too high to climb makes a trap.
std::string GroundProfileProblem(const std::vector<RaceGate>& pRoute, const std::vector<double>& pHeights);

// Works out the ground under a craft after it has moved. A step up the craft is not high enough to
// clear stops it: it is put back where it was and bounces (returns true), so the player has to jump
// up steps. Otherwise the ground height is recorded, and a craft left under the ground is lifted
// onto it. A step down needs nothing: the craft just falls.
bool ResolveGroundStep(HovercraftState& pState, const GroundProfile& pGround);

// True when a rival should jump: a step up or a pit lies just ahead.
bool ShouldJumpGroundStep(const HovercraftState& pState, const GroundProfile& pGround);

#endif
