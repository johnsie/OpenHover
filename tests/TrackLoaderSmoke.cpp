// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackFile.h"
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

    for (const char* file : {"b-good", "a-garbage", "c-duplicate-id", "d-clash-with-builtin-name",
                             "e-invalid"})
        std::remove((base + "/" + file + ".ohtrack").c_str());
    std::remove((base + "/ignored.txt").c_str());
    rmdir(base.c_str());
    return ok ? 0 : 1;
}
