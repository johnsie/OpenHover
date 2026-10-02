// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_TRACK_HASH_H
#define OPENHOVER_TRACK_HASH_H

#include "TrackDefinition.h"

#include <string>

// SHA-256 (lower-case hex) of the track's canonical text form. Two players have the same track
// exactly when their hashes match, whatever comments or spacing their files contain.
std::string TrackHash(const TrackDefinition& pTrack);

// Online lobbies send track ids and names as protocol fields, so only plain text is allowed:
// letters, digits, space, '.', '_' and '-', 1 to 24 characters, no leading or trailing space.
bool IsOnlineSafeTrackText(const std::string& pText);
bool IsOnlineSafeTrack(const TrackDefinition& pTrack);

// Bounds that keep a track someone else uploaded from stalling or confusing a race server: item
// counts and coordinate and size ranges. Returns an empty string if the track is within them,
// otherwise a short reason. Run it in addition to TrackDefinition::Validate().
std::string CheckOnlineTrackLimits(const TrackDefinition& pTrack);

#endif
