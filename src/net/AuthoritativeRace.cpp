// SPDX-License-Identifier: MIT OR Apache-2.0
#include "AuthoritativeRace.h"

#include "TrackDefinition.h"
#include "WallCollision.h"

#include <cmath>

bool AuthoritativeRace::Start(const std::vector<LobbyPlayerId>& pPlayerIds, int pTrackIndex)
{
    const std::vector<TrackDefinition>& tracks = BuiltInTracks();
    if (pPlayerIds.empty() || pTrackIndex < 0 || pTrackIndex >= static_cast<int>(tracks.size()))
        return false;
    mRacers.clear();
    const TrackDefinition& track = tracks[pTrackIndex];
    mCourse.reset(new Course(track.mWaypoints, track.mRoadHalfWidth));
    const RaceGate finish = track.Finish();
    const RaceGate next = track.mWaypoints.size() > 1 ? track.mWaypoints[1] : finish;
    double forwardX = next.mX - finish.mX;
    double forwardY = next.mY - finish.mY;
    const double length = std::sqrt(forwardX * forwardX + forwardY * forwardY);
    if (length > 0.0)
    {
        forwardX /= length;
        forwardY /= length;
    }
    else
    {
        forwardX = 1.0;
        forwardY = 0.0;
    }
    const double sideX = -forwardY;
    const double sideY = forwardX;
    for (int index = 0; index < static_cast<int>(pPlayerIds.size()); ++index)
    {
        Racer racer(pPlayerIds[index]);
        HovercraftState spawn;
        spawn.mX = finish.mX + forwardX * (7.0 - index / 3 * 4.0) + sideX * ((index % 3 - 1) * 3.2);
        spawn.mY = finish.mY + forwardY * (7.0 - index / 3 * 4.0) + sideY * ((index % 3 - 1) * 3.2);
        spawn.mHeading = std::atan2(forwardY, forwardX);
        spawn.mTravelHeading = spawn.mHeading;
        racer.mHovercraft.Reset(spawn);
        mRacers.push_back(racer);
    }
    mTick = 0;
    mActive = true;
    return true;
}

bool AuthoritativeRace::SubmitInput(const RaceInputCommand& pCommand)
{
    if (!mActive)
        return false;
    for (Racer& racer : mRacers)
    {
        if (racer.mPlayerId != pCommand.mPlayerId)
            continue;
        racer.mInput.mThrottle = pCommand.mThrottle;
        racer.mInput.mSteering = pCommand.mSteering;
        racer.mInput.mJump = pCommand.mJump;
        racer.mInput.mReverseFacing = pCommand.mReverseFacing;
        return true;
    }
    return false;
}

void AuthoritativeRace::Step()
{
    if (!mActive)
        return;
    for (Racer& racer : mRacers)
    {
        racer.mHovercraft.Step(racer.mInput, 1.0 / 120.0);
        HovercraftState state = racer.mHovercraft.State();
        if (ResolveCourseWallCollision(state, *mCourse))
            racer.mHovercraft.Reset(state);
    }
    ++mTick;
}

void AuthoritativeRace::Stop()
{
    mActive = false;
    mCourse.reset();
    mRacers.clear();
}

RaceSnapshot AuthoritativeRace::Snapshot() const
{
    RaceSnapshot snapshot;
    snapshot.mTick = mTick;
    for (const Racer& racer : mRacers)
        snapshot.mRacers.push_back({racer.mPlayerId, racer.mHovercraft.State()});
    return snapshot;
}