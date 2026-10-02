// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackLoader.h"

#include "TrackFile.h"
#include "TrackHash.h"

#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
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
                                              std::vector<std::string>& pMessages,
                                              bool pAllowSharedNames)
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
        if (problem.empty() && pAllowSharedNames)
        {
            const std::string hash = TrackHash(track);
            for (const TrackDefinition& other : known)
            {
                if (TrackHash(other) == hash)
                    problem = "same track is already loaded";
            }
        }
        else if (problem.empty() && Taken(known, track))
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

bool EnsureDirectory(const std::string& pDirectory)
{
    if (pDirectory.empty())
        return false;
    // Create each missing component in turn.
    for (std::size_t index = 1; index <= pDirectory.size(); ++index)
    {
        if (index != pDirectory.size() && pDirectory[index] != '/' && pDirectory[index] != '\\')
            continue;
        const std::string part = pDirectory.substr(0, index);
#ifdef _WIN32
        _mkdir(part.c_str());
#else
        mkdir(part.c_str(), 0755);
#endif
    }
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(pDirectory.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat info;
    return stat(pDirectory.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

int CountTrackFiles(const std::string& pDirectory)
{
    return static_cast<int>(ListTrackFiles(pDirectory).size());
}

bool SaveDownloadedTrack(const std::string& pDirectory, const std::string& pHash,
                         const std::string& pText)
{
    if (pHash.size() != 64 || pText.empty() || static_cast<long>(pText.size()) > kMaximumTrackFileBytes
        || !EnsureDirectory(pDirectory))
        return false;
    for (char c : pHash)
    {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    }
    const std::string path = pDirectory + "/" + pHash + kExtension;
    if (FILE* existing = std::fopen(path.c_str(), "rb"))
    {
        std::fclose(existing);
        return true;
    }
    if (CountTrackFiles(pDirectory) >= kMaximumDownloadedTracks)
        return false;
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr)
        return false;
    const bool written = std::fwrite(pText.data(), 1, pText.size(), file) == pText.size();
    std::fclose(file);
    if (!written)
        std::remove(path.c_str());
    return written;
}
