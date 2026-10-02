// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_PROTOCOL_H
#define OPENHOVER_PROTOCOL_H

constexpr int kOpenHoverProtocolVersion = 7;
constexpr int kOpenHoverContentVersion = 2;

// A room's track index when the room runs a custom track uploaded by its host. The room's
// snapshot entry then carries the track's SHA-256 hash.
constexpr int kCustomTrackIndex = 1000;
// Largest custom track (canonical text) the server accepts, in bytes.
constexpr int kMaximumTrackUploadBytes = 32768;
// Hex characters per TRACKDATA / TRACKCHUNK line.
constexpr int kTrackChunkHexCharacters = 1500;

#endif
