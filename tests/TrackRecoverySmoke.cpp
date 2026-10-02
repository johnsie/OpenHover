// SPDX-License-Identifier: MIT OR Apache-2.0
// Walks every built-in track and drops a craft well off the road at many places and headings;
// recovery must always put it back on the road, facing along the route.
#include "Course.h"
#include "RecoveryAssist.h"
#include "TrackDefinition.h"

#include <cmath>
#include <iostream>

namespace
{
const double kPi = 3.14159265358979323846;

double WrapAngle(double pAngle)
{
    while (pAngle > kPi)
        pAngle -= 2.0 * kPi;
    while (pAngle < -kPi)
        pAngle += 2.0 * kPi;
    return pAngle;
}
}

int main()
{
    int failures = 0;
    int trials = 0;
    for (const TrackDefinition& track : BuiltInTracks())
    {
        const Course course(track.mWaypoints, track.mRoadHalfWidth);
        const std::vector<RaceGate> checkpoints = track.Checkpoints();
        const std::size_t count = track.mWaypoints.size();
        for (std::size_t index = 0; index < count; ++index)
        {
            const RaceGate& start = track.mWaypoints[index];
            const RaceGate& end = track.mWaypoints[(index + 1) % count];
            const double length = std::hypot(end.mX - start.mX, end.mY - start.mY);
            const double routeHeading = std::atan2(end.mY - start.mY, end.mX - start.mX);
            for (double along = 0.0; along < length; along += 25.0)
            {
                const double fraction = along / length;
                const double roadX = start.mX + (end.mX - start.mX) * fraction;
                const double roadY = start.mY + (end.mY - start.mY) * fraction;
                for (int side = -1; side <= 1; side += 2)
                {
                    for (int heading = 0; heading < 4; ++heading)
                    {
                        HovercraftState state;
                        state.mX = roadX - std::sin(routeHeading) * 40.0 * side;
                        state.mY = roadY + std::cos(routeHeading) * 40.0 * side;
                        state.mHeading = heading * kPi / 2.0;
                        state.mTravelHeading = state.mHeading;
                        state.mSpeed = 30.0;
                        if (course.IsOnRoad(state.mX, state.mY))
                            continue; // another stretch of road is nearby; nothing to recover from
                        const RaceGate& target = checkpoints[(index * checkpoints.size()) / count];
                        ++trials;
                        if (!RecoverHovercraftToRoute(state, course, target) || !course.IsOnRoad(state.mX, state.mY)
                            || !std::isfinite(state.mHeading))
                        {
                            ++failures;
                            if (failures <= 5)
                                std::cerr << track.mId << " recovery failed near waypoint " << index
                                          << " at along=" << along << "\n";
                            continue;
                        }
                        // Facing roughly along the route, not at a wall.
                        double nearestHeading = routeHeading;
                        double nearestDistance = 1e18;
                        for (std::size_t other = 0; other < count; ++other)
                        {
                            const RaceGate& a = track.mWaypoints[other];
                            const RaceGate& b = track.mWaypoints[(other + 1) % count];
                            const double midX = (a.mX + b.mX) * 0.5;
                            const double midY = (a.mY + b.mY) * 0.5;
                            const double distance = std::hypot(state.mX - midX, state.mY - midY);
                            if (distance < nearestDistance)
                            {
                                nearestDistance = distance;
                                nearestHeading = std::atan2(b.mY - a.mY, b.mX - a.mX);
                            }
                        }
                        if (std::fabs(WrapAngle(state.mHeading - nearestHeading)) > kPi * 0.75)
                        {
                            ++failures;
                            if (failures <= 5)
                                std::cerr << track.mId << " recovery faced backwards near waypoint "
                                          << index << "\n";
                        }
                    }
                }
            }
        }
    }
    if (failures > 0)
    {
        std::cerr << failures << " of " << trials << " recoveries failed\n";
        return 1;
    }
    return trials > 100 ? 0 : 1;
}
