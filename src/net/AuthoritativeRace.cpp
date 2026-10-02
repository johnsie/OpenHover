// SPDX-License-Identifier: MIT OR Apache-2.0
#include "AuthoritativeRace.h"
#include "RivalNames.h"

#include "RacerCollision.h"
#include "SteeringAssist.h"
#include "TrackDefinition.h"
#include "WallCollision.h"

#include <cmath>
#include <random>
#include <utility>

namespace
{
const LobbyPlayerId kFirstAiPlayerId = 1000000;

bool ShouldJumpRaisedSection(const HovercraftState& pState,
                             const std::vector<RaisedSection>& pSections)
{
    const double travelX = std::cos(pState.mTravelHeading);
    const double travelY = std::sin(pState.mTravelHeading);
    for (const RaisedSection& section : pSections)
    {
        const double forwardX = std::cos(section.mHeading);
        const double forwardY = std::sin(section.mHeading);
        const double sideX = -forwardY;
        const double sideY = forwardX;
        const double deltaX = section.mX - pState.mX;
        const double deltaY = section.mY - pState.mY;
        const double forwardDistance = deltaX * travelX + deltaY * travelY;
        const double sidewaysDistance = std::fabs(deltaX * sideX + deltaY * sideY);
        const double jumpLead = std::fmax(4.0, std::fabs(pState.mSpeed) * 0.4);
        if (forwardDistance > 0.0 && forwardDistance <= section.mHalfLength + jumpLead
            && sidewaysDistance <= section.mHalfWidth + 0.9)
        {
            return true;
        }
    }
    return false;
}
}

bool AuthoritativeRace::Start(const std::vector<LobbyPlayerId>& pPlayerIds, int pTrackIndex,
                              int pTargetLaps, bool pWeaponsAllowed, int pRivalCount,
                              RaceMode pRaceMode, const std::vector<TrackDefinition>* pTracks)
{
    // The server passes its own list (built-in plus installed custom tracks); tests use the
    // built-in tracks.
    const std::vector<TrackDefinition>& tracks = pTracks != nullptr ? *pTracks : BuiltInTracks();
    if (pPlayerIds.empty() || pTrackIndex < 0 || pTrackIndex >= static_cast<int>(tracks.size())
        || pTargetLaps < 1)
        return false;
    mRacers.clear();
    const TrackDefinition& track = tracks[pTrackIndex];
    mCourse.reset(new Course(track.mWaypoints, track.mRoadHalfWidth));
    mCheckpoints = track.Checkpoints();
    mBoostPads = track.mBoostPads;
    mHazardZones = track.mHazardZones;
    mMines = track.mMines;
    mRaisedSections = track.mRaisedSections;
    const RaceGate finish = track.Finish();
    mFinish = finish;
    const RaceGate turn = track.mWaypoints.empty() ? finish : track.mWaypoints.front();
    double forwardX = turn.mX - finish.mX;
    double forwardY = turn.mY - finish.mY;
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
    std::vector<LobbyPlayerId> racerIds = pPlayerIds;
    if (RaceModeUsesRivals(pRaceMode))
    {
        // An AI racer's id carries its display-name pool index, so every client shows the same
        // names without any extra protocol field, and no two rivals share a name.
        std::random_device entropy;
        for (int nameIndex : PickRivalNames(pRivalCount, entropy()))
            racerIds.push_back(kFirstAiPlayerId + static_cast<LobbyPlayerId>(nameIndex));
    }
    for (int index = 0; index < static_cast<int>(racerIds.size()); ++index)
    {
        Racer racer(racerIds[index], mCheckpoints, finish, pTargetLaps);
        HovercraftState spawn;
        spawn.mX = finish.mX + forwardX * (7.0 - index / 3 * 4.0) + sideX * ((index % 3 - 1) * 3.2);
        spawn.mY = finish.mY + forwardY * (7.0 - index / 3 * 4.0) + sideY * ((index % 3 - 1) * 3.2);
        spawn.mHeading = std::atan2(forwardY, forwardX);
        spawn.mTravelHeading = spawn.mHeading;
        racer.mHovercraft.Reset(spawn);
        if (index >= static_cast<int>(pPlayerIds.size()))
            racer.mRivalController.reset(new RivalController(track.mWaypoints));
        mRacers.push_back(std::move(racer));
    }
    mTick = 0;
    mTargetLaps = pTargetLaps;
    mWeaponsAllowed = pWeaponsAllowed;
    mRaceMode = pRaceMode;
    mRaceStart.Reset();
    mRaceStart.Begin();
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
        racer.mInput.mFire = pCommand.mFire;
        racer.mRecoverRequested = pCommand.mRecover;
        racer.mSteeringAssistEnabled = pCommand.mSteeringAssist;
        racer.mBrakingAssistEnabled = pCommand.mBrakingAssist;
        if (pCommand.mCraftClass >= static_cast<int>(CraftClass::Balanced)
            && pCommand.mCraftClass <= static_cast<int>(CraftClass::Control)
            && racer.mCraftClass != static_cast<CraftClass>(pCommand.mCraftClass))
        {
            racer.mCraftClass = static_cast<CraftClass>(pCommand.mCraftClass);
            racer.mHovercraft.SetTuning(CraftClassTuning(racer.mCraftClass));
        }
        return true;
    }
    return false;
}

void AuthoritativeRace::Step()
{
    if (!mActive)
        return;
    mRaceStart.Update(1.0 / 120.0);
    if (!mRaceStart.Started())
    {
        ++mTick;
        return;
    }
    for (Racer& racer : mRacers)
    {
        std::vector<HovercraftState> others;
        if (racer.mRivalController)
        {
            for (const Racer& other : mRacers)
            {
                if (&other != &racer)
                    others.push_back(other.mHovercraft.State());
            }
        }
        if (racer.mRecoverRequested)
        {
            const bool forced = racer.mRivalController != nullptr;
            const RaceProgress& progress = racer.mRace.Progress();
            const RaceGate& target = progress.mNextCheckpoint < static_cast<int>(mCheckpoints.size())
                ? mCheckpoints[progress.mNextCheckpoint] : mFinish;
            HovercraftState state = racer.mHovercraft.State();
            if (RecoverHovercraftToRoute(state, *mCourse, target, forced))
                racer.mHovercraft.Reset(state);
            racer.mRecoverRequested = false;
        }
        if (racer.mRivalController)
        {
            racer.mRivalController->Update(racer.mHovercraft.State());
            racer.mInput = racer.mRivalController->InputFor(racer.mHovercraft.State(), others);
            racer.mInput.mJump = ShouldJumpRaisedSection(racer.mHovercraft.State(), mRaisedSections);
        }
        HovercraftInput input = racer.mInput;
        if (!racer.mRivalController)
        {
            const RaceProgress& progress = racer.mRace.Progress();
            const RaceGate& target = progress.mNextCheckpoint < static_cast<int>(mCheckpoints.size())
                ? mCheckpoints[progress.mNextCheckpoint] : mFinish;
            if (racer.mSteeringAssistEnabled)
                input = ApplySteeringAssist(input, racer.mHovercraft.State(), target);
            if (racer.mBrakingAssistEnabled)
                input = ApplyBrakingAssist(input, racer.mHovercraft.State(), target);
        }
        racer.mHovercraft.Step(input, 1.0 / 120.0);
    }

    for (std::size_t first = 0; first < mRacers.size(); ++first)
    {
        for (std::size_t second = first + 1; second < mRacers.size(); ++second)
        {
            HovercraftState firstState = mRacers[first].mHovercraft.State();
            HovercraftState secondState = mRacers[second].mHovercraft.State();
            if (ResolveRacerCollision(firstState, secondState))
            {
                mRacers[first].mHovercraft.Reset(firstState);
                mRacers[second].mHovercraft.Reset(secondState);
            }
        }
    }

    for (Racer& racer : mRacers)
    {
        HovercraftState state = racer.mHovercraft.State();
        ResolveCourseWallCollision(state, *mCourse);
        for (const BoostPad& pad : mBoostPads)
            ApplyBoostPad(state, pad);
        state.mSurfaceHeight = 0.0;
        for (const RaisedSection& section : mRaisedSections)
        {
            LandOnRaisedSection(state, section);
            ResolveRaisedSectionCollision(state, section);
        }
        for (Mine& mine : mMines)
            ApplyMine(state, mine);
        for (const HazardZone& zone : mHazardZones)
            ApplyHazardZone(state, zone, 1.0 / 120.0);
        racer.mHovercraft.Reset(state);
        // A rival wedged against a wall (after a collision or a missile, say) puts itself back on
        // the road, just as a player would with the recovery button.
        if (racer.mRivalController && racer.mStallDetector.Update(state.mX, state.mY, 1.0 / 120.0))
            racer.mRecoverRequested = true;
        racer.mRace.Update(state.mX, state.mY, 1.0 / 120.0);
        racer.mLapTimer.Update(racer.mRace.Progress());
    }
    if (mWeaponsAllowed)
    {
        for (Racer& racer : mRacers)
        {
            if (racer.mInput.mFire)
                racer.mMissile.Fire(racer.mHovercraft.State());
            racer.mMissile.Step(1.0 / 120.0, *mCourse);
        }
        for (std::size_t owner = 0; owner < mRacers.size(); ++owner)
        {
            for (std::size_t target = 0; target < mRacers.size(); ++target)
            {
                if (owner == target)
                    continue;
                HovercraftState state = mRacers[target].mHovercraft.State();
                if (mRacers[owner].mMissile.ApplyHit(state))
                    mRacers[target].mHovercraft.Reset(state);
            }
        }
    }
    ++mTick;
}

void AuthoritativeRace::Stop()
{
    mActive = false;
    mCourse.reset();
    mCheckpoints.clear();
    mFinish = RaceGate();
    mBoostPads.clear();
    mHazardZones.clear();
    mMines.clear();
    mRaisedSections.clear();
    mRacers.clear();
    mTargetLaps = 0;
    mWeaponsAllowed = false;
    mRaceMode = RaceMode::SingleRace;
    mRaceStart.Reset();
}

bool AuthoritativeRace::Complete() const
{
    if (!mActive || mRacers.empty() || !RaceModeHasFinish(mRaceMode))
        return false;
    for (const Racer& racer : mRacers)
    {
        if (!racer.mRace.Progress().mFinished)
            return false;
    }
    return true;
}

RaceSnapshot AuthoritativeRace::Snapshot() const
{
    RaceSnapshot snapshot;
    snapshot.mTick = mTick;
    snapshot.mTargetLaps = mTargetLaps;
    snapshot.mStartLights = mRaceStart.LightsLit();
    snapshot.mCountdownSeconds = mRaceStart.SecondsRemaining();
    snapshot.mCountdownActive = mRaceStart.CountdownActive();
    std::vector<RaceProgress> progresses;
    for (const Racer& racer : mRacers)
        progresses.push_back(racer.mRace.Progress());
    for (int index = 0; index < static_cast<int>(mRacers.size()); ++index)
    {
        const Racer& racer = mRacers[index];
        snapshot.mRacers.push_back({racer.mPlayerId, racer.mHovercraft.State(), racer.mCraftClass,
                        racer.mRace.Progress(),
                                    racer.mLapTimer.Timing(), CalculateRacePosition(progresses, index)});
        if (racer.mMissile.Active())
            snapshot.mMissiles.push_back({racer.mPlayerId, racer.mMissile.State()});
    }
    for (const Mine& mine : mMines)
        snapshot.mMineTriggered.push_back(mine.mTriggered);
    return snapshot;
}