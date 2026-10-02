// SPDX-License-Identifier: MIT OR Apache-2.0
// Command-line checker for OpenHover track files: OpenHoverTrackCheck <file> [...]
// Prints one line per file and exits non-zero if any file is unreadable or invalid.
#include "TrackFile.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: OpenHoverTrackCheck <track-file> [...]\n";
        return 2;
    }
    int failures = 0;
    for (int index = 1; index < argc; ++index)
    {
        std::ifstream file(argv[index], std::ios::binary);
        if (!file)
        {
            std::cout << argv[index] << ": cannot read file\n";
            ++failures;
            continue;
        }
        std::ostringstream text;
        text << file.rdbuf();
        TrackDefinition track;
        std::string problem = ParseTrack(text.str(), track);
        if (problem.empty())
            problem = track.Validate();
        if (problem.empty())
            std::cout << argv[index] << ": ok (" << track.mName << ", " << track.mWaypoints.size()
                      << " waypoints)\n";
        else
        {
            std::cout << argv[index] << ": INVALID: " << problem << "\n";
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
