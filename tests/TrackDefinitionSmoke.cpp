// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackDefinition.h"

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
        if (!track.Validate().empty() || !ids.insert(track.mId).second
            || track.Checkpoints().size() + 1 != track.mWaypoints.size()
            || track.mProvenance.mAuthor.empty() || track.mProvenance.mLicense.empty()
            || track.mProvenance.mAssetOrigin.empty()
            || track.mRoadRed == track.mWallRed && track.mRoadGreen == track.mWallGreen
                && track.mRoadBlue == track.mWallBlue)
        {
            std::cerr << "built-in track was invalid\n";
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