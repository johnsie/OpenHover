// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_TRK_CONVERTER_H
#define OPENHOVER_TRK_CONVERTER_H

#include "TrackBuilder.h"
#include "TrkReader.h"

#include <set>
#include <string>
#include <vector>

// Two rooms that share part of an edge, and so can be driven between.
struct TrkLink
{
    int mA = 0;
    int mB = 0;
    double mMidX = 0.0; // middle of the shared stretch, metres
    double mMidY = 0.0;
    double mWidth = 0.0; // length of the shared stretch, metres
};

struct TrkConvertOptions
{
    std::string mName;
    bool mHeights = true;          // carry the rooms' floor heights over as ground height
    bool mPads = true;             // boost pads on long straights
    bool mMines = false;           // mines on long straights
    bool mHazards = false;         // slowing zones on long straights
    bool mReverse = false;         // drive the loop the other way round
    std::set<int> mExcluded;       // rooms the player has switched off (never the start room)
};

struct TrkConversion
{
    std::string mProblem;                  // why no track could be made; empty on success
    std::vector<std::string> mWarnings;    // things lost or changed in the conversion
    std::vector<TrkLink> mLinks;
    std::vector<int> mCycle;               // rooms in driving order, starting at the start room
    int mStartRoom = -1;
    std::vector<EditorPoint> mRoute;       // the loop through those rooms, in metres
    double mHalfWidth = 8.0;
    std::vector<double> mHeights;          // ground height of the stretch from each route point to the next
    double mBiggestRise = 0.0;             // the highest step up and the deepest drop on the route
    double mBiggestDrop = 0.0;
    BuiltTrack mBuilt;                     // the track that would be written
};

// Turns a read track into an OpenHover track: works out which rooms touch, finds a loop through the
// start room, follows it as a route, and builds a complete track from that route with the same
// builder the track editor uses. Everything is deterministic for the same inputs.
TrkConversion ConvertTrk(const TrkTrack& pTrack, const TrkConvertOptions& pOptions);

// Name for a converted track from a file name: the stem, without a "[1-123]" style suffix, with
// anything but letters and digits turned into spaces, at most 24 characters.
std::string TrkTrackNameFromFile(const std::string& pPath);

#endif
