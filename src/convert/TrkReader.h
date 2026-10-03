// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_TRK_READER_H
#define OPENHOVER_TRK_READER_H

#include <string>
#include <utility>
#include <vector>

// A reader for the room-based track files of another hover racing game, for converting tracks whose
// author has the rights to them. It understands only the layout seen in the sample it was written
// from (see TrkReader.cpp); anything else is reported as a problem rather than guessed at.

struct TrkStart
{
    int mIndex = 0;
    int mRoom = -1; // index (in file order) of the room this start is in, as the file states it
    double mX = 0.0; // metres
    double mY = 0.0;
    double mZ = 0.0;
};

struct TrkRoom
{
    double mFloor = 0.0;   // metres
    double mCeiling = 0.0; // metres
    std::vector<std::pair<double, double>> mCorners; // metres, in order round the room
};

struct TrkTrack
{
    std::vector<TrkStart> mStarts;
    std::vector<TrkRoom> mRooms;
};

// Reads pBytes (the whole file). Returns an empty string and fills pOut on success, otherwise a
// short reason. The free-text description in the file is deliberately not read.
std::string ReadTrk(const std::string& pBytes, TrkTrack& pOut);

#endif
