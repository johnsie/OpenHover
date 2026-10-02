// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackFile.h"
#include "TrackHash.h"
#include "TrackLoader.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
void Write(const std::string& pPath, const std::string& pText)
{
    std::ofstream(pPath, std::ios::binary) << pText;
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
    char pattern[] = "/tmp/openhover-trackloader-XXXXXX";
    const char* directory = mkdtemp(pattern);
    if (directory == nullptr)
    {
        std::cerr << "cannot create a temporary directory\n";
        return 1;
    }
    const std::string base = directory;

    TrackDefinition custom = BuiltInTracks()[0];
    custom.mId = "my-track";
    custom.mName = "My Track";
    Write(base + "/b-good.ohtrack", SerializeTrack(custom));
    Write(base + "/a-garbage.ohtrack", "not a track");
    TrackDefinition duplicate = custom;
    duplicate.mName = "Other Name";
    Write(base + "/c-duplicate-id.ohtrack", SerializeTrack(duplicate));
    TrackDefinition clash = BuiltInTracks()[0];
    clash.mId = "different-id";
    Write(base + "/d-clash-with-builtin-name.ohtrack", SerializeTrack(clash));
    TrackDefinition broken = custom;
    broken.mId = "broken";
    broken.mName = "Broken";
    broken.mCheckpoints.pop_back();
    Write(base + "/e-invalid.ohtrack", SerializeTrack(broken));
    Write(base + "/ignored.txt", SerializeTrack(custom));

    std::vector<std::string> messages;
    const std::vector<TrackDefinition> tracks = LoadCustomTracks(base, BuiltInTracks(), messages);
    expect(tracks.size() == 1 && tracks[0].mId == "my-track", "only the one valid, unique track loads");
    expect(messages.size() == 4, "each rejected .ohtrack file explains itself");
    bool namedFiles = true;
    for (const char* file : {"a-garbage", "c-duplicate-id", "d-clash-with-builtin-name", "e-invalid"})
    {
        bool found = false;
        for (const std::string& message : messages)
            found = found || message.find(file) != std::string::npos;
        namedFiles = namedFiles && found;
    }
    expect(namedFiles, "messages name the offending files");

    std::vector<std::string> none;
    expect(LoadCustomTracks(base + "/does-not-exist", BuiltInTracks(), none).empty() && none.empty(),
           "a missing folder is not an error");

    // Downloaded tracks: saved by hash, never twice, capped, and loaded back with shared names.
    const std::string cache = base + "/nested/downloaded";
    expect(EnsureDirectory(cache), "nested folders are created");
    const std::string customText = SerializeTrack(custom);
    const std::string customHash = TrackHash(custom);
    expect(SaveDownloadedTrack(cache, customHash, customText), "a downloaded track is saved");
    expect(SaveDownloadedTrack(cache, customHash, customText) && CountTrackFiles(cache) == 1,
           "saving the same track again keeps one file");
    expect(!SaveDownloadedTrack(cache, "not-a-hash", customText) && !SaveDownloadedTrack(cache, customHash, ""),
           "a bad hash or empty text is not saved");
    TrackDefinition sameName = BuiltInTracks()[1];
    sameName.mId = "friends-two";
    sameName.mName = "My Track";
    TrackDefinition sameNameAgain = BuiltInTracks()[2];
    sameNameAgain.mId = "friends-three";
    sameNameAgain.mName = "My Track";
    expect(SaveDownloadedTrack(cache, TrackHash(sameName), SerializeTrack(sameName))
               && SaveDownloadedTrack(cache, TrackHash(sameNameAgain), SerializeTrack(sameNameAgain)),
           "two different tracks may share a name");
    std::vector<std::string> cacheMessages;
    const std::vector<TrackDefinition> reloaded = LoadCustomTracks(cache, BuiltInTracks(), cacheMessages, true);
    expect(reloaded.size() == 3 && cacheMessages.empty(), "downloaded tracks with shared names all load back");
    std::vector<std::string> strictMessages;
    expect(LoadCustomTracks(cache, BuiltInTracks(), strictMessages, false).size() == 1,
           "the strict loader still rejects a name clash");
    for (int index = 0; CountTrackFiles(cache) < kMaximumDownloadedTracks; ++index)
    {
        TrackDefinition filler = custom;
        filler.mId = "filler-" + std::to_string(index);
        SaveDownloadedTrack(cache, TrackHash(filler), SerializeTrack(filler));
    }
    TrackDefinition overflow = custom;
    overflow.mId = "one-too-many";
    expect(!SaveDownloadedTrack(cache, TrackHash(overflow), SerializeTrack(overflow)),
           "a full cache refuses further tracks");
    expect(CountTrackFiles(cache) == kMaximumDownloadedTracks, "the cache never exceeds its cap");
    std::string removeCommand = "rm -rf '" + base + "/nested'";
    if (std::system(removeCommand.c_str()) != 0)
        std::cerr << "could not remove cache folder\n";

    // Limits for tracks uploaded to a race server.
    expect(CheckOnlineTrackLimits(custom).empty(), "a normal track is within the online limits");
    TrackDefinition tooWide = custom;
    tooWide.mRoadHalfWidth = 31.0;
    TrackDefinition tooFar = custom;
    tooFar.mWaypoints[0].mX = 9000.0;
    TrackDefinition tooMany = custom;
    while (tooMany.mWaypoints.size() <= 256)
        tooMany.mWaypoints.push_back(custom.mWaypoints[0]);
    TrackDefinition tooManyMines = custom;
    tooManyMines.mMines.assign(65, Mine());
    expect(!CheckOnlineTrackLimits(tooWide).empty() && !CheckOnlineTrackLimits(tooFar).empty()
               && !CheckOnlineTrackLimits(tooMany).empty() && !CheckOnlineTrackLimits(tooManyMines).empty(),
           "tracks outside the online limits are refused");

    for (const char* file : {"b-good", "a-garbage", "c-duplicate-id", "d-clash-with-builtin-name",
                             "e-invalid"})
        std::remove((base + "/" + file + ".ohtrack").c_str());
    std::remove((base + "/ignored.txt").c_str());
    rmdir(base.c_str());
    return ok ? 0 : 1;
}
