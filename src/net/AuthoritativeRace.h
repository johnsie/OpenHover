// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_AUTHORITATIVE_RACE_H
#define OPENHOVER_AUTHORITATIVE_RACE_H

#include "Hovercraft.h"
#include "Lobby.h"

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
};

struct RaceSnapshot
{
    unsigned int mTick = 0;
    std::vector<RaceRacerSnapshot> mRacers;
};

class AuthoritativeRace
{
public:
    bool Start(const std::vector<LobbyPlayerId>& pPlayerIds, int pTrackIndex);
    bool SubmitInput(const RaceInputCommand& pCommand);
    void Step();
    void Stop();

    bool Active() const { return mActive; }
    RaceSnapshot Snapshot() const;

private:
    struct Racer
    {
        explicit Racer(LobbyPlayerId pPlayerId) : mPlayerId(pPlayerId) {}

        LobbyPlayerId mPlayerId;
        Hovercraft mHovercraft;
        HovercraftInput mInput;
    };

    bool mActive = false;
    unsigned int mTick = 0;
    std::vector<Racer> mRacers;
};

#endif