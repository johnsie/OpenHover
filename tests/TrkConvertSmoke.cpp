// SPDX-License-Identifier: MIT OR Apache-2.0
// Builds a synthetic room-based track file (a ring of sixteen rooms, laid out the way the reader
// expects) and checks reading, converting, and driving the result. No real track data is used.
#include "AuthoritativeRace.h"
#include "TrackFile.h"
#include "TrkConverter.h"
#include "TrkReader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
const double kPi = 3.14159265358979323846;

void PutInt(std::string& pBytes, std::int32_t pValue)
{
    for (int index = 0; index < 4; ++index)
        pBytes += static_cast<char>((static_cast<std::uint32_t>(pValue) >> (8 * index)) & 0xff);
}

struct Corner
{
    std::int32_t mX;
    std::int32_t mY;
};

// A ring of pCount quadrilateral rooms between two radii, in thousandths of a metre.
std::string MakeRingTrack(int pCount, double pInnerRadius, double pOuterRadius, int pStartRoom = 0,
                          bool pHump = false)
{
    std::string bytes(0x60, '\0');
    bytes.replace(3, 20, "synthetic test track");
    PutInt(bytes, 1);
    PutInt(bytes, 50);
    PutInt(bytes, 0);
    PutInt(bytes, 3); // three starting positions, inside the first room
    // The starts sit in room pStartRoom, whose number is stored with each of them.
    const double startAngle = 2.0 * kPi * (pStartRoom + 0.5) / pCount;
    const double startRadius = (pInnerRadius + pOuterRadius) * 0.5;
    for (int index = 0; index < 3; ++index)
    {
        PutInt(bytes, index + 1);
        PutInt(bytes, pStartRoom);
        PutInt(bytes, static_cast<std::int32_t>(std::lround((std::cos(startAngle) * startRadius
                                                             - std::sin(startAngle) * (index - 1) * 1.5) * 1000.0)));
        PutInt(bytes, static_cast<std::int32_t>(std::lround((std::sin(startAngle) * startRadius
                                                             + std::cos(startAngle) * (index - 1) * 1.5) * 1000.0)));
        PutInt(bytes, -3000);
        bytes += std::string(2, '\0');
    }
    for (int room = 0; room < pCount; ++room)
    {
        const double a0 = 2.0 * kPi * room / pCount;
        const double a1 = 2.0 * kPi * (room + 1) / pCount;
        const auto corner = [](double pAngle, double pRadius)
        {
            return Corner{static_cast<std::int32_t>(std::lround(std::cos(pAngle) * pRadius * 1000.0)),
                          static_cast<std::int32_t>(std::lround(std::sin(pAngle) * pRadius * 1000.0))};
        };
        const Corner corners[4] = {corner(a0, pInnerRadius), corner(a1, pInnerRadius), corner(a1, pOuterRadius),
                                   corner(a0, pOuterRadius)};
        std::int32_t minX = corners[0].mX, maxX = corners[0].mX, minY = corners[0].mY, maxY = corners[0].mY;
        for (const Corner& c : corners)
        {
            minX = std::min(minX, c.mX);
            maxX = std::max(maxX, c.mX);
            minY = std::min(minY, c.mY);
            maxY = std::max(maxY, c.mY);
        }
        PutInt(bytes, 4);
        const int floorHeight = pHump && room == 5 ? -2500 : 0; // a pit one room long
        PutInt(bytes, floorHeight);
        PutInt(bytes, floorHeight + 4000);
        PutInt(bytes, minX);
        PutInt(bytes, minY);
        PutInt(bytes, maxX);
        PutInt(bytes, maxY);
        for (int k = 0; k < 4; ++k)
        {
            const Corner& c = corners[k];
            const Corner& n = corners[(k + 1) % 4];
            PutInt(bytes, c.mX);
            PutInt(bytes, c.mY);
            PutInt(bytes, static_cast<std::int32_t>(std::lround(std::hypot(double(n.mX - c.mX), double(n.mY - c.mY)))));
        }
        // Stand-in for the neighbour and texture data between rooms.
        for (int k = 0; k < 12; ++k)
            PutInt(bytes, k % 5);
    }
    bytes += std::string(400, 'p'); // the repeated-byte block
    bytes += std::string(100, '\0');
    return bytes;
}
}

int main()
{
    bool ok = true;
    const auto expect = [&](bool pCondition, const char* pMessage)
    {
        if (!pCondition)
        {
            std::cerr << pMessage << '\n';
            ok = false;
        }
    };

    const std::string file = MakeRingTrack(16, 250.0, 280.0);
    TrkTrack track;
    expect(ReadTrk(file, track).empty(), "a well-formed file is read");
    expect(track.mRooms.size() == 16 && track.mStarts.size() == 3, "all rooms and starts are found");
    expect(std::fabs(track.mRooms[0].mCeiling - 4.0) < 1e-9 && track.mRooms[0].mCorners.size() == 4,
           "room heights and corners are read in metres");
    expect(track.mStarts[0].mRoom == 0 && track.mStarts[0].mZ == -3.0 && track.mStarts[0].mX > 200.0,
           "start positions are read in metres, with the room the file names");

    TrkTrack unchanged;
    unchanged.mRooms.resize(1);
    expect(!ReadTrk("short", unchanged).empty() && unchanged.mRooms.size() == 1, "a tiny file is refused");
    expect(!ReadTrk(std::string(5000, 'x'), unchanged).empty(), "a file without the layout is refused");
    std::string damaged = file;
    damaged[0x60 + 12] = 99; // break the start table's count
    expect(!ReadTrk(damaged, unchanged).empty(), "a damaged start table is refused");
    std::string brokenRoom = file;
    // Corrupt one stored edge length of the first room so it no longer matches its corners.
    const std::size_t firstRoom = 0x60 + 16 + 3 * 22;
    brokenRoom[firstRoom + 28 + 8] = static_cast<char>(brokenRoom[firstRoom + 28 + 8] + 40);
    TrkTrack fewer;
    expect(ReadTrk(brokenRoom, fewer).empty() && fewer.mRooms.size() == 15,
           "a room whose edge lengths do not match its corners is not accepted");

    TrkConvertOptions options;
    options.mName = "Ring Test";
    const TrkConversion conversion = ConvertTrk(track, options);
    expect(conversion.mProblem.empty() && conversion.mBuilt.mOk, "the ring converts to a valid track");
    expect(conversion.mCycle.size() == 16 && conversion.mCycle[0] == conversion.mStartRoom,
           "the loop visits every room and starts in the start room");
    expect(conversion.mLinks.size() == 16, "each room touches the next one round the ring");
    expect(conversion.mBuilt.mTrack.mName == "Ring Test" && conversion.mBuilt.mTrack.mRoadHalfWidth >= 4.0,
           "the track keeps its name and gets a sensible road width");
    TrackDefinition reloaded;
    expect(ParseTrack(SerializeTrack(conversion.mBuilt.mTrack), reloaded).empty() && reloaded.Validate().empty(),
           "the converted track saves and validates as an .ohtrack file");

    // Direction can be flipped, and the same inputs always give the same result.
    TrkConvertOptions reversed = options;
    reversed.mReverse = true;
    const TrkConversion backwards = ConvertTrk(track, reversed);
    expect(backwards.mBuilt.mOk && backwards.mCycle.size() == 16 && backwards.mCycle[1] == conversion.mCycle[15],
           "reversing drives the loop the other way round");
    expect(ConvertTrk(track, options).mCycle == conversion.mCycle, "conversion is deterministic");

    // Pads, mines and hazard zones are switchable.
    expect(!conversion.mBuilt.mTrack.mBoostPads.empty(), "boost pads are on by default");
    TrkTrack wide;
    expect(ReadTrk(MakeRingTrack(10, 500.0, 540.0), wide).empty(), "a wide ring reads");
    TrkConvertOptions plain;
    plain.mName = "Wide";
    plain.mPads = false;
    const TrkConversion noPads = ConvertTrk(wide, plain);
    expect(noPads.mBuilt.mOk && noPads.mBuilt.mTrack.mBoostPads.empty(), "boost pads can be switched off");
    plain.mMines = true;
    plain.mHazards = true;
    const TrkConversion hazardous = ConvertTrk(wide, plain);
    expect(hazardous.mBuilt.mOk && !hazardous.mBuilt.mTrack.mMines.empty()
               && !hazardous.mBuilt.mTrack.mHazardZones.empty(),
           "mines and hazard zones can be switched on");
    expect(noPads.mBuilt.mTrack.mMines.empty() && noPads.mBuilt.mTrack.mHazardZones.empty(),
           "mines and hazard zones are off by default");

    // Floor heights become ground heights, eased into ramps.
    expect(conversion.mHeights.empty() && conversion.mBuilt.mTrack.mGroundHeights.empty(),
           "a track with a flat floor stays flat");
    TrkTrack humped;
    expect(ReadTrk(MakeRingTrack(36, 100.0, 130.0, 0, true), humped).empty(), "a ring with a pit in it reads");
    const TrkConversion hilly = ConvertTrk(humped, options);
    const std::vector<double>& ground = hilly.mBuilt.mTrack.mGroundHeights;
    expect(hilly.mBuilt.mOk && ground.size() == hilly.mBuilt.mTrack.mWaypoints.size(),
           "a ring with a pit gets a ground height at every waypoint");
    expect(!ground.empty() && *std::max_element(ground.begin(), ground.end())
                   - *std::min_element(ground.begin(), ground.end()) > 0.9,
           "the pit becomes a drop in the ground");
    expect(hilly.mBiggestRise > 2.4 && hilly.mBiggestDrop > 2.4, "the floor steps keep their real size");
    bool trapWarning = false;
    for (const std::string& warning : hilly.mWarnings)
        trapWarning = trapWarning || warning.find("TRAPS") != std::string::npos;
    expect(trapWarning, "a pit with walls too high to climb is reported as a trap");
    expect(hilly.mBuilt.mTrack.Validate().empty(), "the stepped track validates");
    TrkConvertOptions levelled = options;
    levelled.mHeights = false;
    expect(ConvertTrk(humped, levelled).mBuilt.mTrack.mGroundHeights.empty(), "ground heights can be switched off");

    // Switching a room off breaks a ring, since there is nowhere to go round it.
    TrkConvertOptions cut = options;
    cut.mExcluded.insert(5);
    expect(!ConvertTrk(track, cut).mProblem.empty(), "switching off a room on a plain ring leaves no loop");
    cut.mExcluded.clear();
    cut.mExcluded.insert(conversion.mStartRoom);
    expect(ConvertTrk(track, cut).mBuilt.mOk, "the start room cannot be switched off");

    // A small ring of many short legs (none long enough for the old start rule) still converts,
    // because the start only has to fit on the road.
    TrkTrack small;
    expect(ReadTrk(MakeRingTrack(22, 60.0, 86.0, 3), small).empty() && small.mRooms.size() == 22,
           "a small ring of 22 rooms is read");
    const TrkConversion smallConversion = ConvertTrk(small, options);
    expect(smallConversion.mProblem.empty() && smallConversion.mBuilt.mOk,
           "a small ring with only short legs converts to a valid track");
    expect(smallConversion.mBuilt.mOk && smallConversion.mBuilt.mTrack.mRoadHalfWidth >= 12.0,
           "its road is as wide as the rooms");

    // The start room comes from the room number the file states, wherever it is in the ring.
    TrkTrack later;
    expect(ReadTrk(MakeRingTrack(16, 250.0, 280.0, 9), later).empty() && later.mStarts[0].mRoom == 9,
           "the room number of the starts is read");
    const TrkConversion fromNine = ConvertTrk(later, options);
    expect(fromNine.mBuilt.mOk && fromNine.mStartRoom == 9 && fromNine.mCycle[0] == 9,
           "the loop starts in the room the file puts the starts in");
    TrkTrack wrongRoom = later;
    for (TrkStart& start : wrongRoom.mStarts)
        start.mRoom = 2; // contradicts the positions, which are in room 9
    expect(!ConvertTrk(wrongRoom, options).mWarnings.empty(), "a start outside its stated room is flagged");

    // The converted track can be raced: a rival finishes a lap.
    std::vector<TrackDefinition> tracks = BuiltInTracks();
    tracks.push_back(conversion.mBuilt.mTrack);
    AuthoritativeRace race;
    race.SetRivalCraftClass(CraftClass::Balanced);
    expect(race.Start({11}, static_cast<int>(tracks.size()) - 1, 1, false, 1, RaceMode::SingleRace, &tracks),
           "a race can start on the converted track");
    bool finished = false;
    for (int step = 0; step < 120 * 300 && !finished; ++step)
    {
        race.Step();
        for (const RaceRacerSnapshot& racer : race.Snapshot().mRacers)
            finished = finished || (racer.mPlayerId >= 1000000 && racer.mProgress.mFinished);
    }
    expect(finished, "a rival drives a lap of the converted track");

    // And a track with hills: the rival drives it too, staying on the ground.
    tracks.push_back(hilly.mBuilt.mTrack);
    AuthoritativeRace hillRace;
    hillRace.SetRivalCraftClass(CraftClass::Balanced);
    expect(hillRace.Start({12}, static_cast<int>(tracks.size()) - 1, 1, false, 1, RaceMode::SingleRace, &tracks),
           "a race can start on the sloping track");
    bool hillFinished = false;
    bool wentUnderground = false;
    const GroundProfile hillGround = hilly.mBuilt.mTrack.Ground();
    for (int step = 0; step < 120 * 300 && !hillFinished; ++step)
    {
        hillRace.Step();
        for (const RaceRacerSnapshot& racer : hillRace.Snapshot().mRacers)
        {
            hillFinished = hillFinished || (racer.mPlayerId >= 1000000 && racer.mProgress.mFinished);
            wentUnderground = wentUnderground
                || racer.mState.mHeight < hillGround.HeightAt(racer.mState.mX, racer.mState.mY) + 0.3;
        }
    }
    expect(hillFinished, "a rival jumps the pit and drives a lap of the stepped track");
    expect(!wentUnderground, "craft stay above the sloping ground");

    expect(TrkTrackNameFromFile("/home/x/Some Track[1-1019].trk") == "Some Track", "name drops the [1-n] suffix");
    expect(TrkTrackNameFromFile("C:\\tracks\\a_b-c.d.trk") == "a b c d", "odd characters become spaces");
    expect(TrkTrackNameFromFile("x.trk").empty() == false && TrkTrackNameFromFile("!!!.trk").empty(),
           "a name with no letters is empty");
    expect(TrkTrackNameFromFile(std::string(50, 'a') + ".trk").size() == 24, "names are at most 24 characters");
    return ok ? 0 : 1;
}
