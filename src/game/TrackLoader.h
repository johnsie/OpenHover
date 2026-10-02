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
std::vector<TrackDefinition> LoadCustomTracks(const std::string& pDirectory,
                                              const std::vector<TrackDefinition>& pExisting,
                                              std::vector<std::string>& pMessages);

#endif
