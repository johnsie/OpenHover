// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackFile.h"
#include "TrackHash.h"

#include <iostream>
#include <set>

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
    std::set<std::string> hashes;
    for (const TrackDefinition& track : BuiltInTracks())
    {
        const std::string hash = TrackHash(track);
        expect(hash.size() == 64, "hash is 64 hex characters");
        expect(hash == TrackHash(track), "hash is stable");
        hashes.insert(hash);
        TrackDefinition reloaded;
        expect(ParseTrack(SerializeTrack(track), reloaded).empty() && TrackHash(reloaded) == hash,
               "a track saved and reloaded has the same hash");
        // Comments and spacing in a file do not change which track it is.
        TrackDefinition annotated;
        expect(ParseTrack("# comment\n\n" + SerializeTrack(track) + "\n# end\n", annotated).empty()
                   && TrackHash(annotated) == hash,
               "comments do not change the hash");
        TrackDefinition moved = track;
        moved.mWaypoints[3].mX += 0.001;
        expect(TrackHash(moved) != hash, "moving a waypoint changes the hash");
        TrackDefinition widened = track;
        widened.mRoadHalfWidth += 0.01;
        expect(TrackHash(widened) != hash, "changing the road width changes the hash");
        TrackDefinition extra = track;
        extra.mMines.push_back(Mine());
        expect(TrackHash(extra) != hash, "adding a mine changes the hash");
        TrackDefinition renamed = track;
        renamed.mId += "x";
        expect(TrackHash(renamed) != hash, "the id is part of the hash");
        expect(IsOnlineSafeTrack(track), "built-in tracks are online safe");
    }
    expect(hashes.size() == BuiltInTracks().size(), "different tracks have different hashes");
    expect(IsOnlineSafeTrackText("My Track-2.0_x"), "plain names are safe");
    expect(!IsOnlineSafeTrackText("") && !IsOnlineSafeTrackText(" lead") && !IsOnlineSafeTrackText("trail ")
               && !IsOnlineSafeTrackText("a|b") && !IsOnlineSafeTrackText("a,b")
               && !IsOnlineSafeTrackText("line\nbreak") && !IsOnlineSafeTrackText("caf\xc3\xa9")
               && !IsOnlineSafeTrackText(std::string(25, 'a')),
           "protocol delimiters, non-ASCII and over-long text are rejected");
    return ok ? 0 : 1;
}
