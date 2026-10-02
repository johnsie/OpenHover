// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_TRACK_FILE_H
#define OPENHOVER_TRACK_FILE_H

#include "TrackDefinition.h"

#include <string>

// Text form of an OpenHover v1 track (see docs/track-format.md). One directive per line;
// blank lines and lines starting with '#' are ignored.
std::string SerializeTrack(const TrackDefinition& pTrack);

// Parses a track file. On success returns an empty string and fills pOut; on failure returns a
// message that names the line, and pOut is left unchanged. A successful parse does not imply the
// track is valid: call TrackDefinition::Validate() as well.
std::string ParseTrack(const std::string& pText, TrackDefinition& pOut);

#endif
