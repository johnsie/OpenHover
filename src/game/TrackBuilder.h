// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_TRACK_BUILDER_H
#define OPENHOVER_TRACK_BUILDER_H

#include "TrackDefinition.h"

#include <string>
#include <vector>

struct EditorPoint
{
    double mX = 0.0;
    double mY = 0.0;
};

struct BuiltTrack
{
    bool mOk = false;
    // When not ok, a short reason the player can act on (for example "ADD AT LEAST 5 POINTS").
    std::string mProblem;
    TrackDefinition mTrack;
};

// A track id from a display name: lower case letters and digits with single hyphens.
std::string MakeTrackId(const std::string& pName);

// Turns the corner points a player placed (a closed loop, in driving order) into a complete,
// valid track: it adds a start straight on the longest side, picks four checkpoints, bridges every
// crossing, adds boost pads on long straights, and runs the same validation as any track file.
BuiltTrack BuildTrackFromPoints(const std::string& pName, const std::string& pAuthor,
                                const std::vector<EditorPoint>& pPoints, double pRoadHalfWidth);

#endif
