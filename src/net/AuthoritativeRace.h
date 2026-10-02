// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_AUTHORITATIVE_RACE_H
#define OPENHOVER_AUTHORITATIVE_RACE_H

#include "Course.h"
#include "Hovercraft.h"
#include "LapTiming.h"
#include "Lobby.h"
#include "RacePosition.h"

#include <memory>
#include <vector>

struct RaceInputCommand
{
    LobbyPlayerId mPlayerId = 0;
    double mThrottle = 0.0;
    double mSteering = 0.0;
    bool mJump = false;
    bool mReverseFacing = false;
};

struct RaceRacerSnapshot
{
    LobbyPlayerId mPlayerId = 0;
    HovercraftState mState;
    RaceProgress mProgress;
    LapTiming mLapTiming;
    int mPosition = 0;
};

struct RaceSnapshot
{
    unsigned int mTick = 0;
    int mTargetLaps = 0;
    std::vector<RaceRacerSnapshot> mRacers;
};

class AuthoritativeRace
{
public:
    bool Start(const std::vector<LobbyPlayerId>& pPlayerIds, int pTrackIndex, int pTargetLaps = 3);
    bool SubmitInput(const RaceInputCommand& pCommand);
    void Step();
    void Stop();

    bool Active() const { return mActive; }
    bool Complete() const;
    RaceSnapshot Snapshot() const;

private:
    struct Racer
    {
        Racer(LobbyPlayerId pPlayerId, const std::vector<RaceGate>& pWaypoints,
              const RaceGate& pFinish, int pTargetLaps)
            : mPlayerId(pPlayerId), mRace(pWaypoints, pFinish, pTargetLaps) {}

        LobbyPlayerId mPlayerId;
        Hovercraft mHovercraft;
        HovercraftInput mInput;
            Race mRace;
            LapTimer mLapTimer;
    };

    bool mActive = false;
    unsigned int mTick = 0;
    int mTargetLaps = 0;
    std::unique_ptr<Course> mCourse;
    std::vector<Racer> mRacers;
};

#endif