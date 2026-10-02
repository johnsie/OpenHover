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
    }

    TrackDefinition invalid;
    invalid.mId = "invalid";
    invalid.mName = "Invalid";
    if (invalid.Validate().empty())
    {
        std::cerr << "invalid track was accepted\n";
        return 1;
    }
    invalid.mWaypoints = {{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, {1.0, 1.0, 1.0}, {0.0, 1.0, 1.0}};
    invalid.mCheckpoints = {{1.0, 0.0, 1.0}, {1.0, 1.0, 1.0},
                            {0.0, 1.0, 1.0}, {0.0, 0.0, 1.0}};
    invalid.mRoadHalfWidth = 1.0;
    invalid.mProvenance = {"OpenHover contributors", "CC BY 4.0", "Original test data"};
    if (!invalid.Validate().empty())
    {
        std::cerr << "valid provenance metadata was rejected\n";
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