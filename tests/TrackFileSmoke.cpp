// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackFile.h"

#include <iostream>

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
    for (const TrackDefinition& original : BuiltInTracks())
    {
        const std::string text = SerializeTrack(original);
        TrackDefinition loaded;
        const std::string error = ParseTrack(text, loaded);
        expect(error.empty(), "built-in track text did not parse");
        expect(loaded.Validate().empty(), "round-tripped track did not validate");
        expect(SerializeTrack(loaded) == text, "serialize/parse/serialize must be stable");
        expect(loaded.mId == original.mId && loaded.mName == original.mName
                   && loaded.mWaypoints.size() == original.mWaypoints.size()
                   && loaded.mCheckpoints.size() == original.mCheckpoints.size()
                   && loaded.mRaisedSections.size() == original.mRaisedSections.size()
                   && loaded.mMines.size() == original.mMines.size()
                   && loaded.mHazardZones.size() == original.mHazardZones.size()
                   && loaded.mBoostPads.size() == original.mBoostPads.size(),
               "round trip changed the track contents");
        expect(loaded.mWaypoints.back().mX == original.mWaypoints.back().mX
                   && loaded.mRoadHalfWidth == original.mRoadHalfWidth,
               "numbers must survive exactly");
    }

    TrackDefinition scratch;
    scratch.mId = "keep";
    const auto rejects = [&](const std::string& pText, const char* pFragment)
    {
        const std::string error = ParseTrack(pText, scratch);
        expect(!error.empty() && error.find(pFragment) != std::string::npos, pFragment);
        expect(scratch.mId == "keep", "a rejected file must not change the output");
    };
    rejects("", "empty");
    rejects("id x\n", "first line");
    rejects("openhover-track 2\n", "first line");
    rejects("openhover-track 1\nbogus 1 2\n", "line 2: unknown directive");
    rejects("openhover-track 1\nwaypoint 1 2\n", "needs 3 numbers");
    rejects("openhover-track 1\nwaypoint 1 two 3\n", "needs numbers");
    rejects("openhover-track 1\nroad-half-width nan\n", "needs numbers");

    TrackDefinition commented;
    const std::string error = ParseTrack("# my track\n\nopenhover-track 1\nid demo\nname Demo Track\n"
                                         "waypoint 0 0 6\n", commented);
    expect(error.empty() && commented.mId == "demo" && commented.mName == "Demo Track"
               && commented.mWaypoints.size() == 1,
           "comments, blank lines and names with spaces are accepted");
    expect(!commented.Validate().empty(), "an incomplete track parses but does not validate");
    return ok ? 0 : 1;
}
