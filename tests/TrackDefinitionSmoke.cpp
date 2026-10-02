// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackDefinition.h"

#include <cmath>
#include <iostream>
#include <set>

int main()
{
    const std::vector<TrackDefinition>& tracks = BuiltInTracks();
    if (tracks.size() != 3)
    {
        std::cerr << "expected three built-in tracks\n";
        return 1;
    }

    std::set<std::string> ids;
    for (const TrackDefinition& track : tracks)
    {
        double lapLength = 0.0;
        for (int waypoint = 0; waypoint < static_cast<int>(track.mWaypoints.size()); ++waypoint)
        {
            const RaceGate& start = track.mWaypoints[waypoint];
            const RaceGate& end = track.mWaypoints[(waypoint + 1) % track.mWaypoints.size()];
            const double deltaX = end.mX - start.mX;
            const double deltaY = end.mY - start.mY;
            lapLength += std::sqrt(deltaX * deltaX + deltaY * deltaY);
        }
        if (!track.Validate().empty() || !ids.insert(track.mId).second
            || track.Checkpoints().size() != 4
            || track.mProvenance.mAuthor.empty() || track.mProvenance.mLicense.empty()
            || track.mProvenance.mAssetOrigin.empty()
            || track.mWaypoints.front().mRadius < track.mRoadHalfWidth
            || track.mCheckpoints.front().mRadius < track.mRoadHalfWidth
            || lapLength < 480.0
            || track.mRoadRed == track.mWallRed && track.mRoadGreen == track.mWallGreen
                && track.mRoadBlue == track.mWallBlue)
        {
            std::cerr << "built-in track was invalid\n";
            return 1;
        }
        if ((track.mId == "harbor-loop" && !track.mRaisedSections.empty())
            || (track.mId == "glass-switchback" && track.mRaisedSections.size() < 8)
            || (track.mId == "velocity-ring"
                && (track.mRaisedSections.size() < 5 || track.mHazardZones.size() < 4)))
        {
            std::cerr << "built-in track did not preserve its intended obstacle profile\n";
            return 1;
        }
        const double requiredGateClearance = track.mRoadHalfWidth + 2.0 - 0.0001;
        const auto checkGateClearance = [&track, requiredGateClearance](const RaceGate& gate)
        {
            double nearestDistance = -1.0;
            for (const RaceGate& waypoint : track.mWaypoints)
            {
                const double deltaX = gate.mX - waypoint.mX;
                const double deltaY = gate.mY - waypoint.mY;
                const double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
                if (nearestDistance < 0.0 || distance < nearestDistance)
                    nearestDistance = distance;
            }
            return nearestDistance >= requiredGateClearance;
        };
        if (!checkGateClearance(track.Finish()))
        {
            std::cerr << "finish gate could be bypassed at a turn\n";
            return 1;
        }
        for (const RaceGate& checkpoint : track.Checkpoints())
        {
            if (!checkGateClearance(checkpoint))
            {
                std::cerr << "checkpoint gate could be bypassed at a turn\n";
                return 1;
            }
        }
    }

    TrackDefinition invalid;
    invalid.mId = "invalid";
    invalid.mName = "Invalid";
    if (invalid.Validate().empty())
    {
        std::cerr << "invalid track was accepted\n";
        return 1;
    }
    invalid.mWaypoints = {{0.0, 0.0, 6.0}, {100.0, 0.0, 6.0}, {100.0, 100.0, 6.0}, {0.0, 100.0, 6.0}};
    invalid.mCheckpoints = {{100.0, 0.0, 6.0}, {100.0, 100.0, 6.0},
                            {0.0, 100.0, 6.0}, {0.0, 0.0, 6.0}};
    invalid.mRoadHalfWidth = 6.0;
    invalid.mProvenance = {"OpenHover contributors", "CC BY 4.0", "Original test data"};
    if (!invalid.Validate().empty())
    {
        std::cerr << "valid provenance metadata was rejected\n";
        return 1;
    }
    TrackDefinition offRoute = invalid;
    offRoute.mCheckpoints[1] = {400.0, 400.0, 6.0};
    TrackDefinition reordered = invalid;
    std::swap(reordered.mCheckpoints[0], reordered.mCheckpoints[2]);
    TrackDefinition duplicate = invalid;
    duplicate.mWaypoints[1] = duplicate.mWaypoints[0];
    if (offRoute.Validate().empty() || reordered.Validate().empty() || duplicate.Validate().empty())
    {
        std::cerr << "off-route, out-of-order, or degenerate route was accepted\n";
        return 1;
    }
    TrackDefinition crossing = invalid;
    crossing.mWaypoints = {{0.0, 0.0, 6.0}, {100.0, 100.0, 6.0}, {100.0, 0.0, 6.0}, {0.0, 100.0, 6.0}};
    crossing.mCheckpoints = {{100.0, 100.0, 6.0}, {100.0, 0.0, 6.0}, {0.0, 100.0, 6.0}, {0.0, 0.0, 6.0}};
    TrackDefinition sealed = crossing;
    sealed.mRaisedSections = {{50.0, 50.0, 16.0, 6.0, 0.0, 1.5, true}};
    if (crossing.Validate().empty() || !sealed.Validate().empty())
    {
        std::cerr << "unsealed crossing accepted or sealed crossing rejected: "
                  << sealed.Validate() << "\n";
        return 1;
    }
    TrackDefinition narrowStart = invalid;
    narrowStart.mRoadHalfWidth = 2.0; // starting grid is 3.2 either side of the centreline
    for (RaceGate& gate : narrowStart.mWaypoints)
        gate.mRadius = 6.0;
    TrackDefinition mineOnGrid = invalid;
    mineOnGrid.mMines = {{0.0, 1.0, 1.0}};
    if (narrowStart.Validate().empty() || mineOnGrid.Validate().empty())
    {
        std::cerr << "unsafe starting grid was accepted\n";
        return 1;
    }
    TrackDefinition strayPad = invalid;
    strayPad.mBoostPads = {{300.0, 300.0, 1.0}};
    TrackDefinition strayMine = invalid;
    strayMine.mMines = {{-300.0, 0.0, 1.0}};
    TrackDefinition strayHazard = invalid;
    strayHazard.mHazardZones = {{0.0, 300.0, 1.0, 1.0}};
    if (strayPad.Validate().empty() || strayMine.Validate().empty() || strayHazard.Validate().empty())
    {
        std::cerr << "off-road pad, mine, or hazard was accepted\n";
        return 1;
    }
    invalid.mProvenance.mAssetOrigin.clear();
    if (invalid.Validate().empty())
    {
        std::cerr << "missing provenance metadata was accepted\n";
        return 1;
    }
    return 0;
}