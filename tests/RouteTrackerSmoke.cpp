// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RouteTracker.h"
#include "TrackBuilder.h"
#include "TrackDefinition.h"

#include <cmath>
#include <iostream>

namespace
{
// Drives a point round the route centreline, at pStep units per update, for pLaps laps.
void Drive(const TrackDefinition& pTrack, double pStep, int pLaps, bool& pOk, double& pFinal, double& pLength,
           double& pLargestJump)
{
    const Course course(pTrack.mWaypoints, pTrack.mRoadHalfWidth);
    RouteTracker tracker;
    double previous = 0.0;
    bool first = true;
    pLargestJump = 0.0;
    const std::size_t count = pTrack.mWaypoints.size();
    pLength = course.RouteLength();
    for (int lap = 0; lap < pLaps; ++lap)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            const RaceGate& a = pTrack.mWaypoints[index];
            const RaceGate& b = pTrack.mWaypoints[(index + 1) % count];
            const double length = std::hypot(b.mX - a.mX, b.mY - a.mY);
            for (double along = 0.0; along < length; along += pStep)
            {
                const double fraction = along / length;
                const double distance = tracker.Update(course, a.mX + (b.mX - a.mX) * fraction,
                                                       a.mY + (b.mY - a.mY) * fraction);
                if (!first)
                    pLargestJump = std::fmax(pLargestJump, std::fabs(distance - previous));
                first = false;
                previous = distance;
            }
        }
    }
    pFinal = tracker.Distance();
    (void)pOk;
}
}

int main()
{
    bool ok = true;
    const auto expect = [&](bool pCondition, const char* pMessage)
    {
        if (!pCondition)
        {
            std::cerr << pMessage << '\n';
            ok = false;
        }
    };
    for (const TrackDefinition& track : BuiltInTracks())
    {
        double finalDistance = 0.0;
        double length = 0.0;
        double largestJump = 0.0;
        Drive(track, 2.0, 3, ok, finalDistance, length, largestJump);
        expect(std::fabs(finalDistance - 3.0 * length) < 6.0 + 1.0,
               "three laps of driving measure three route lengths");
        expect(largestJump < 4.0, "distance never jumps, even where roads cross or double back");
    }
    // Two laps of a figure eight: the crossing must not confuse the tracker.
    std::vector<EditorPoint> eight;
    for (int index = 0; index < 16; ++index)
    {
        const double angle = 2.0 * 3.14159265358979 * index / 16.0;
        eight.push_back({std::sin(angle) * 260.0, std::sin(2.0 * angle) * 140.0});
    }
    const BuiltTrack figureEight = BuildTrackFromPoints("Eight", "t", eight, 8.0);
    expect(figureEight.mOk, "figure eight builds");
    double finalDistance = 0.0;
    double length = 0.0;
    double largestJump = 0.0;
    Drive(figureEight.mTrack, 1.5, 2, ok, finalDistance, length, largestJump);
    expect(std::fabs(finalDistance - 2.0 * length) < 5.0, "two laps of a figure eight measure two lengths");
    expect(largestJump < 3.0, "no jump at the crossing");
    // A craft running behind another reads as behind.
    const TrackDefinition& track = BuiltInTracks()[0];
    const Course course(track.mWaypoints, track.mRoadHalfWidth);
    RouteTracker ahead;
    RouteTracker behind;
    const RaceGate& a = track.mWaypoints[0];
    const RaceGate& b = track.mWaypoints[1];
    ahead.Update(course, a.mX + (b.mX - a.mX) * 0.5, a.mY + (b.mY - a.mY) * 0.5);
    behind.Update(course, a.mX + (b.mX - a.mX) * 0.1, a.mY + (b.mY - a.mY) * 0.1);
    expect(ahead.Distance() > behind.Distance(), "the craft further along the road has the larger distance");
    return ok ? 0 : 1;
}
