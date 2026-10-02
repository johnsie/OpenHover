// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_TRACK_LOADER_H
#define OPENHOVER_TRACK_LOADER_H

#include "TrackDefinition.h"

#include <string>
#include <vector>

// Largest track file the loader will read.
constexpr long kMaximumTrackFileBytes = 1 << 20;
// Most custom tracks loaded from one folder.
constexpr int kMaximumCustomTracks = 32;

// Reads every "*.ohtrack" file in pDirectory (in name order) and returns the ones that parse and
// pass TrackDefinition::Validate(). A file is skipped, with a one-line reason appended to
// pMessages, when it is unreadable, too large, invalid, or reuses an id or name from pExisting or
// from an earlier file. A missing directory simply yields no tracks.
// With pAllowSharedNames, a track whose id or name matches another is still loaded (two people
// can legitimately publish tracks with the same name); only an exact duplicate (same id and same
// content) is skipped. Without it, any id or name clash is skipped.
std::vector<TrackDefinition> LoadCustomTracks(const std::string& pDirectory,
                                              const std::vector<TrackDefinition>& pExisting,
                                              std::vector<std::string>& pMessages,
                                              bool pAllowSharedNames = false);

// Tracks downloaded from a race room are kept in a folder of their own, one file per track, named
// by the track's hash, so the same track is never stored twice.
constexpr int kMaximumDownloadedTracks = 64;

// Creates pDirectory (and any missing parents) if it does not exist. Returns true if it exists
// afterwards.
bool EnsureDirectory(const std::string& pDirectory);

// Number of "*.ohtrack" files in pDirectory.
int CountTrackFiles(const std::string& pDirectory);

// Writes pText as <pDirectory>/<pHash>.ohtrack unless that file already exists or the folder
// already holds kMaximumDownloadedTracks files. Returns true if the track is now on disk.
bool SaveDownloadedTrack(const std::string& pDirectory, const std::string& pHash,
                         const std::string& pText);

#endif
