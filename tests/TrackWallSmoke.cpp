// SPDX-License-Identifier: MIT OR Apache-2.0
// Fires a craft at full throttle from many places and headings on every built-in track and checks
// that the walls always keep it on the road: no gaps to slip through at corners or junctions.
#include "Course.h"
#include "Hovercraft.h"
#include "TrackDefinition.h"
#include "WallCollision.h"

#include <cmath>
#include <iostream>

int main()
{
    const double kPi = 3.14159265358979323846;
    int escapes = 0;
    int trials = 0;
    for (const TrackDefinition& track : BuiltInTracks())
    {
        const Course course(track.mWaypoints, track.mRoadHalfWidth);
        const std::size_t count = track.mWaypoints.size();
        for (std::size_t index = 0; index < count; ++index)
        {
            const RaceGate& start = track.mWaypoints[index];
            const RaceGate& end = track.mWaypoints[(index + 1) % count];
            const double length = std::hypot(end.mX - start.mX, end.mY - start.mY);
            for (double along = 0.0; along < length; along += 20.0)
            {
                const double fraction = along / length;
                for (int heading = 0; heading < 8; ++heading)
                {
                    Hovercraft craft;
                    HovercraftState state;
                    state.mX = start.mX + (end.mX - start.mX) * fraction;
                    state.mY = start.mY + (end.mY - start.mY) * fraction;
                    state.mHeading = heading * kPi / 4.0;
                    state.mTravelHeading = state.mHeading;
                    state.mSpeed = 20.0;
                    craft.Reset(state);
                    HovercraftInput input;
                    input.mThrottle = 1.0;
                    input.mSteering = (heading % 2 == 0) ? 0.0 : 0.3;
                    ++trials;
                    bool escaped = false;
                    for (int step = 0; step < 120 * 5 && !escaped; ++step)
                    {
                        craft.Step(input, 1.0 / 120.0);
                        HovercraftState current = craft.State();
                        if (ResolveCourseWallCollision(current, course))
                            craft.Reset(current);
                        const HovercraftState& after = craft.State();
                        double roadX = 0.0;
                        double roadY = 0.0;
                        course.ProjectToRoad(after.mX, after.mY, roadX, roadY);
                        escaped = std::hypot(after.mX - roadX, after.mY - roadY)
                            > track.mRoadHalfWidth + 0.5;
                    }
                    if (escaped)
                    {
                        ++escapes;
                        if (escapes <= 5)
                            std::cerr << track.mId << " craft escaped the walls near waypoint " << index
                                      << " along=" << along << " heading=" << heading << "\n";
                    }
                }
            }
        }
    }
    if (escapes > 0)
    {
        std::cerr << escapes << " of " << trials << " runs escaped the walls\n";
        return 1;
    }
    return trials > 100 ? 0 : 1;
}
