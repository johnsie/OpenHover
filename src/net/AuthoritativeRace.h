// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_AUTHORITATIVE_RACE_H
#define OPENHOVER_AUTHORITATIVE_RACE_H

#include "Course.h"
#include "BoostPad.h"
#include "CraftClass.h"
#include "HazardZone.h"
#include "Hovercraft.h"
#include "LapTiming.h"
#include "Lobby.h"
#include "Mine.h"
#include "Missile.h"
#include "RacePosition.h"
#include "RaceStart.h"
#include "RaceMode.h"
#include "RecoveryAssist.h"
#include "RaisedSection.h"
#include "RivalController.h"

#include <memory>
#include <vector>

struct RaceInputCommand
{
    LobbyPlayerId mPlayerId = 0;
    double mThrottle = 0.0;
    double mSteering = 0.0;
    bool mJump = false;
    bool mReverseFacing = false;
    bool mFire = false;
    bool mRecover = false;
    bool mSteeringAssist = false;
    bool mBrakingAssist = false;
    int mCraftClass = static_cast<int>(CraftClass::Balanced);
};

struct RaceRacerSnapshot
{
    LobbyPlayerId mPlayerId = 0;
    HovercraftState mState;
    CraftClass mCraftClass = CraftClass::Balanced;
    RaceProgress mProgress;
    LapTiming mLapTiming;
    int mPosition = 0;
};

struct RaceMissileSnapshot
{
    LobbyPlayerId mPlayerId = 0;
    HovercraftState mState;
};

struct RaceSnapshot
{
    unsigned int mTick = 0;
    int mTargetLaps = 0;
    int mStartLights = 0;
    int mCountdownSeconds = 0;
    bool mCountdownActive = false;
    std::vector<RaceRacerSnapshot> mRacers;
    std::vector<RaceMissileSnapshot> mMissiles;
    std::vector<bool> mMineTriggered;
};

class AuthoritativeRace
{
public:
    bool Start(const std::vector<LobbyPlayerId>& pPlayerIds, int pTrackIndex, int pTargetLaps = 3,
               bool pWeaponsAllowed = true, int pRivalCount = 0,
               RaceMode pRaceMode = RaceMode::SingleRace);
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
        CraftClass mCraftClass = CraftClass::Balanced;
        Missile mMissile;
        std::unique_ptr<RivalController> mRivalController;
        bool mRecoverRequested = false;
        bool mSteeringAssistEnabled = false;
        bool mBrakingAssistEnabled = false;
        Race mRace;
        LapTimer mLapTimer;
    };

    bool mActive = false;
    unsigned int mTick = 0;
    int mTargetLaps = 0;
    bool mWeaponsAllowed = false;
    RaceMode mRaceMode = RaceMode::SingleRace;
    RaceStart mRaceStart;
    std::unique_ptr<Course> mCourse;
    std::vector<RaceGate> mCheckpoints;
    RaceGate mFinish;
    std::vector<BoostPad> mBoostPads;
    std::vector<HazardZone> mHazardZones;
    std::vector<Mine> mMines;
    std::vector<RaisedSection> mRaisedSections;
    std::vector<Racer> mRacers;
};

#endif