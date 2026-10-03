// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Ground.h"
#include "HazardZone.h"
#include "Hovercraft.h"
#include "RecoveryAssist.h"
#include "RaisedSection.h"
#include "TrackBuilder.h"
#include "TrackDefinition.h"
#include "TrackFile.h"
#include "TrackHash.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
int gFailures = 0;

void Expect(bool pCondition, const char* pMessage)
{
    if (!pCondition)
    {
        std::cerr << "FAIL: " << pMessage << '\n';
        ++gFailures;
    }
}

std::vector<EditorPoint> Pentagon(double pSide)
{
    return {{0, 0}, {pSide, 0}, {pSide, pSide}, {0, pSide}, {-pSide * 0.25, pSide * 0.5}};
}

// Drives a craft along y = 0 from x = 20 with full throttle, applying the ground each step, and
// reports how far it got, its highest point over the ground and whether it was ever blocked.
struct Drive
{
    double mEndX = 0.0;
    double mHighest = 0.0;
    bool mBlocked = false;
    double mFinalHeight = 0.0;
    double mFinalGround = 0.0;
};

Drive DriveAlong(const GroundProfile& pGround, double pJumpAtX, double pSeconds, double pStartX = 20.0,
                 double pStartSpeed = 30.0, double pThrottle = 1.0)
{
    Hovercraft craft;
    HovercraftState start;
    start.mX = pStartX;
    start.mSpeed = pStartSpeed;
    craft.Reset(start);
    craft.ApplyGround(pGround);
    HovercraftInput input;
    input.mThrottle = pThrottle;
    Drive result;
    bool jumped = false;
    for (int step = 0; step < static_cast<int>(pSeconds * 120.0); ++step)
    {
        input.mJump = !jumped && craft.State().mX >= pJumpAtX;
        jumped = jumped || input.mJump;
        craft.Step(input, 1.0 / 120.0);
        result.mBlocked = craft.ApplyGround(pGround) || result.mBlocked;
        result.mHighest = std::max(result.mHighest, craft.State().mHeight - craft.State().mGroundHeight);
    }
    result.mEndX = craft.State().mX;
    result.mFinalHeight = craft.State().mHeight;
    result.mFinalGround = craft.State().mGroundHeight;
    return result;
}
}

int main()
{
    Expect(GroundProfile().Flat() && GroundProfile().HeightAt(5.0, 5.0) == 0.0, "an empty profile is flat zero");

    // A straight road in three stretches: level, a step of 1 up at x = 100, a drop of 4 at x = 200.
    const std::vector<RaceGate> route = {{0, 0, 5}, {100, 0, 5}, {200, 0, 5}, {300, 0, 5}, {300, 100, 5}, {0, 100, 5}};
    const std::vector<double> heights = {0.0, 1.0, -3.0, -2.0, -1.0, 0.0};
    const GroundProfile ground(route, heights);
    Expect(ground.HeightAt(50.0, 0.0) == 0.0 && ground.HeightAt(99.0, 2.0) == 0.0, "ground is level along a stretch");
    Expect(ground.HeightAt(101.0, 0.0) == 1.0 && ground.HeightAt(150.0, 3.0) == 1.0, "ground steps up at the waypoint");
    Expect(ground.HeightAt(201.0, 0.0) == -3.0, "ground steps down at the waypoint");
    Expect(ground.HeightAt(150.0, 3.0) == 1.0 && ground.SegmentHeight(1) == 1.0 && ground.SegmentHeight(7) == 1.0,
           "a stretch's height can be asked for directly");

    // Validation.
    Expect(GroundProfileProblem(route, heights).empty(), "a step of one metre up and a big drop are accepted");
    Expect(GroundProfileProblem(route, {}).empty(), "no heights are accepted");
    Expect(!GroundProfileProblem(route, {0.0, 1.0}).empty(), "one height per waypoint is required");
    Expect(GroundProfileProblem(route, {0.0, 0.0, -10.0, 0.0, 0.0, 0.0}).empty(), "a pit with a wall too high to climb is allowed (a trap)");
    Expect(!GroundProfileProblem(route, {0.0, 80.0, 80.0, 0.0, 0.0, 0.0}).empty(), "heights are limited");
    const std::vector<RaceGate> bow = {{0, 0, 5}, {100, 100, 5}, {100, 0, 5}, {0, 100, 5}};
    Expect(!GroundProfileProblem(bow, {0.0, 0.0, 1.0, 0.0}).empty(), "crossing roads at different heights are refused");
    Expect(GroundProfileProblem(bow, {0.0, 0.0, 0.0, 0.0}).empty(), "crossing roads at one height are accepted");

    // Driving into a step of one metre stops the craft; jumping clears it; a kerb is climbed.
    const Drive blocked = DriveAlong(ground, 1e9, 4.0);
    Expect(blocked.mBlocked && blocked.mEndX < 101.0, "a step of one metre stops a craft that does not jump");
    const Drive jumped = DriveAlong(ground, 100.0 - 30.0 * 0.3, 4.0);
    Expect(!jumped.mBlocked && jumped.mEndX > 101.0 && jumped.mFinalGround == 1.0, "a jump clears a step of one metre");
    const std::vector<double> kerb = {0.0, 0.5, 0.5, 0.5, 0.5, 0.0};
    const Drive kerbBlocked = DriveAlong(GroundProfile(route, kerb), 1e9, 3.0);
    Expect(kerbBlocked.mBlocked && kerbBlocked.mEndX < 101.0, "even a kerb stops a craft that does not jump: it never rises by itself");
    const Drive kerbJumped = DriveAlong(GroundProfile(route, kerb), 100.0 - 30.0 * 0.15, 3.0);
    Expect(!kerbJumped.mBlocked && kerbJumped.mEndX > 101.0 && kerbJumped.mFinalGround == 0.5, "a jump clears a kerb");
    Expect(kerbJumped.mFinalHeight >= 0.5 + 0.9, "the craft lands hovering above the higher ground");
    const std::vector<double> tallStep = {0.0, 1.5, 1.5, 1.5, 1.5, 0.0};
    const Drive tallJumped = DriveAlong(GroundProfile(route, tallStep), 100.0 - 30.0 * 0.3, 3.0);
    Expect(!tallJumped.mBlocked && tallJumped.mEndX > 101.0 && tallJumped.mFinalGround == 1.5, "a jump clears a step of one and a half metres");

    // Driving off a drop: the craft leaves the ground and lands on the lower ground.
    const std::vector<double> dropRoute = {0.0, 0.0, -6.0, -6.0, -6.0, 0.0};
    const GroundProfile dropGround(route, dropRoute);
    const Drive fell = DriveAlong(dropGround, 1e9, 5.0, 150.0);
    Expect(!fell.mBlocked && fell.mEndX > 201.0 && fell.mFinalGround == -6.0, "a craft drives off a drop");
    Expect(fell.mHighest > 3.0, "a craft driven off a drop is in the air for a while");
    Expect(fell.mFinalHeight < -6.0 + 3.0, "and comes to rest on the lower ground");

    // A pit twenty metres long: a jump at its edge carries the craft across, otherwise it falls in.
    const std::vector<RaceGate> pitRoute = {{0, 0, 5}, {100, 0, 5}, {120, 0, 5}, {300, 0, 5}, {300, 100, 5}, {0, 100, 5}};
    const GroundProfile pit(pitRoute, {0.0, -4.0, 0.0, 0.0, 0.0, 0.0});
    const Drive across = DriveAlong(pit, 100.0 - 2.0, 3.0, 60.0);
    Expect(!across.mBlocked && across.mEndX > 125.0 && across.mFinalGround == 0.0, "a jump from the edge of a pit clears it");
    const Drive slow = DriveAlong(pit, 100.0 - 1.0, 3.0, 85.0, 12.0, 0.0);
    Expect(slow.mFinalGround == -4.0 && slow.mEndX < 120.0, "a jump from the edge of a pit at low speed does not reach the far side");
    const Drive inPit = DriveAlong(pit, 1e9, 3.0, 60.0);
    Expect(inPit.mBlocked && inPit.mEndX < 125.0 && inPit.mFinalGround == -4.0, "a craft that does not jump falls into the pit");
    Expect(inPit.mFinalHeight < -4.0 + 2.0, "and rests on the floor of it");

    // Recovery lifts a trapped craft out of the pit and gives it a run-up.
    const Course pitCourse(pitRoute, 5.0);
    HovercraftState trapped;
    trapped.mX = 110.0;
    trapped.mY = 0.0;
    trapped.mHeading = 0.0;
    trapped.mGroundHeight = -4.0;
    trapped.mHeight = -2.8;
    trapped.mInPit = true;
    Expect(RecoverHovercraftToRoute(trapped, pitCourse, {300, 0, 5}, false, &pit) && trapped.mX < 70.0
               && trapped.mGroundHeight == 0.0 && trapped.mHeight > 1.0 && !trapped.mInPit,
           "recovery takes a craft in a pit back along the road, onto the higher ground");
    HovercraftState onRoad;
    onRoad.mX = 110.0;
    onRoad.mGroundHeight = -4.0;
    Expect(!RecoverHovercraftToRoute(onRoad, pitCourse, {300, 0, 5}, false, &pit),
           "a craft on the road but not trapped is not moved");

    // A rival is told to jump a step it cannot climb.
    HovercraftState rival;
    rival.mX = 80.0;
    rival.mSpeed = 30.0;
    rival.mTravelHeading = 0.0;
    Expect(!ShouldJumpGroundStep(rival, ground), "a rival waits until a step up is close");
    rival.mX = 97.0;
    Expect(ShouldJumpGroundStep(rival, ground), "a rival jumps at a step up ahead");
    rival.mX = 20.0;
    Expect(!ShouldJumpGroundStep(rival, ground), "a rival does not jump on level ground");
    rival.mX = 198.0;
    Expect(ShouldJumpGroundStep(rival, GroundProfile(route, {0.0, 0.0, -6.0, -6.0, -6.0, 0.0})),
           "a rival jumps at the edge of a pit");
    Expect(!ShouldJumpGroundStep(rival, GroundProfile(route, {0.0, 0.0, -0.3, -0.3, -0.3, 0.0})),
           "a rival does not jump at a small drop");

    // Placed under the ground, a craft is lifted onto it.
    HovercraftState buried;
    buried.mX = 150.0;
    buried.mHeight = 0.5;
    Hovercraft craft;
    craft.Reset(buried);
    craft.ApplyGround(ground);
    Expect(craft.State().mHeight >= 2.1 && craft.State().mGroundHeight == 1.0,
           "a craft placed under the ground is lifted onto it");

    // Hazards and raised sections measure height above the ground.
    HazardZone zone;
    zone.mRadius = 5.0;
    zone.mSpeedLossPerSecond = 2.0;
    HovercraftState onHill;
    onHill.mSpeed = 10.0;
    onHill.mHeight = 21.2;
    onHill.mGroundHeight = 20.0;
    Expect(ApplyHazardZone(onHill, zone, 0.1), "a hazard still slows a craft driving on high ground");
    HovercraftState airborne = onHill;
    airborne.mHeight = 22.0;
    Expect(!ApplyHazardZone(airborne, zone, 0.1), "a hazard does not catch a craft that jumped over it");

    // Tracks carry the heights through the file, the builder and the hash.
    BuilderOptions options;
    options.mPads = false;
    options.mHeights = {0.0, 0.0, 1.0, 0.0, 0.0};
    const BuiltTrack stepped = BuildTrackFromPoints("Stepped", "Tester", Pentagon(200.0), 6.0, options);
    Expect(stepped.mOk && stepped.mTrack.mGroundHeights.size() == stepped.mTrack.mWaypoints.size(),
           "the builder gives one ground height per waypoint");
    TrackDefinition reloaded;
    Expect(ParseTrack(SerializeTrack(stepped.mTrack), reloaded).empty()
               && reloaded.mGroundHeights == stepped.mTrack.mGroundHeights,
           "ground heights survive saving and loading");
    const BuiltTrack flat = BuildTrackFromPoints("Stepped", "Tester", Pentagon(200.0), 6.0, BuilderOptions());
    Expect(flat.mOk && flat.mTrack.mGroundHeights.empty()
               && SerializeTrack(flat.mTrack).find("ground-height") == std::string::npos,
           "a flat track writes no ground lines");
    Expect(TrackHash(flat.mTrack) != TrackHash(stepped.mTrack), "the hash covers the ground");
    TrackDefinition wrong = stepped.mTrack;
    wrong.mGroundHeights.pop_back();
    Expect(!wrong.Validate().empty(), "a track with a missing ground height is invalid");
    TrackDefinition tooHigh = stepped.mTrack;
    tooHigh.mGroundHeights[1] = 80.0;
    Expect(!tooHigh.Validate().empty(), "a track with a ground height out of range is invalid");
    options.mHeights = {0.0, 0.0, 0.0, 0.0, 0.0};
    options.mHeights[4] = 1.0; // a step right behind the start of a short side
    const BuiltTrack closeStep = BuildTrackFromPoints("Close", "Tester", Pentagon(40.0), 4.0, options);
    Expect(closeStep.mOk || !closeStep.mProblem.empty(), "a step near the start either builds or says why not");

    if (gFailures == 0)
        std::cout << "GroundSmoke ok\n";
    return gFailures == 0 ? 0 : 1;
}
