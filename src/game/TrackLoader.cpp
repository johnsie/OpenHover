// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackLoader.h"

#include "TrackFile.h"

#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

namespace
{
const char* const kExtension = ".ohtrack";

bool HasExtension(const std::string& pName)
{
    const std::string extension = kExtension;
    return pName.size() > extension.size()
        && pName.compare(pName.size() - extension.size(), extension.size(), extension) == 0;
}

std::vector<std::string> ListTrackFiles(const std::string& pDirectory)
{
    std::vector<std::string> names;
#ifdef _WIN32
    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA((pDirectory + "\\*").c_str(), &found);
    if (handle != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && HasExtension(found.cFileName))
                names.push_back(found.cFileName);
        } while (FindNextFileA(handle, &found));
        FindClose(handle);
    }
#else
    if (DIR* directory = opendir(pDirectory.c_str()))
    {
        while (const dirent* entry = readdir(directory))
        {
            const std::string name = entry->d_name;
            if (HasExtension(name))
                names.push_back(name);
        }
        closedir(directory);
    }
#endif
    std::sort(names.begin(), names.end());
    return names;
}

bool ReadFile(const std::string& pPath, std::string& pOut)
{
    FILE* file = std::fopen(pPath.c_str(), "rb");
    if (file == nullptr)
        return false;
    pOut.clear();
    char chunk[4096];
    std::size_t read = 0;
    while ((read = std::fread(chunk, 1, sizeof(chunk), file)) > 0)
    {
        pOut.append(chunk, read);
        if (static_cast<long>(pOut.size()) > kMaximumTrackFileBytes)
        {
            std::fclose(file);
            return false;
        }
    }
    std::fclose(file);
    return true;
}

bool Taken(const std::vector<TrackDefinition>& pTracks, const TrackDefinition& pCandidate)
{
    for (const TrackDefinition& track : pTracks)
    {
        if (track.mId == pCandidate.mId || track.mName == pCandidate.mName)
            return true;
    }
    return false;
}
}

std::vector<TrackDefinition> LoadCustomTracks(const std::string& pDirectory,
                                              const std::vector<TrackDefinition>& pExisting,
                                              std::vector<std::string>& pMessages)
{
    std::vector<TrackDefinition> loaded;
    std::vector<TrackDefinition> known = pExisting;
    for (const std::string& name : ListTrackFiles(pDirectory))
    {
        if (static_cast<int>(loaded.size()) >= kMaximumCustomTracks)
        {
            pMessages.push_back(name + ": skipped, too many custom tracks");
            continue;
        }
        std::string text;
        if (!ReadFile(pDirectory + "/" + name, text))
        {
            pMessages.push_back(name + ": skipped, cannot read file or file too large");
            continue;
        }
        TrackDefinition track;
        std::string problem = ParseTrack(text, track);
        if (problem.empty())
            problem = track.Validate();
        if (problem.empty() && Taken(known, track))
            problem = "id or name is already used by another track";
        if (!problem.empty())
        {
            pMessages.push_back(name + ": skipped, " + problem);
            continue;
        }
        known.push_back(track);
        loaded.push_back(track);
    }
    return loaded;
}
