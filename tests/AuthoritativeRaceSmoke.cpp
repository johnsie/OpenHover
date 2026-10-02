// SPDX-License-Identifier: MIT OR Apache-2.0
#include "AuthoritativeRace.h"
#include "Course.h"
#include "RivalNames.h"
#include "TrackDefinition.h"

#include <iostream>
#include <set>
#include <vector>

int main()
{
    AuthoritativeRace race;
    const std::vector<LobbyPlayerId> players = {11, 22};
    if (!race.Start(players, 0) || !race.Active() || race.Complete() || race.Start(players, 99))
    {
        std::cerr << "authoritative race did not validate its start state\n";
        return 1;
    }
    RaceInputCommand input;
    input.mPlayerId = 11;
    input.mThrottle = 1.0;
    input.mFire = true;
    if (!race.SubmitInput(input) || race.SubmitInput({99, 1.0, 0.0, false, false}))
    {
        std::cerr << "authoritative race did not validate player inputs\n";
        return 1;
    }
    for (int step = 0; step < 120; ++step)
        race.Step();
    if (race.Snapshot().mRacers[0].mState.mSpeed != 0.0)
    {
        std::cerr << "authoritative racer moved before the start countdown finished\n";
        return 1;
    }
    for (int step = 0; step < 720; ++step)
        race.Step();
    const RaceSnapshot snapshot = race.Snapshot();
    if (snapshot.mTick != 840 || snapshot.mTargetLaps != 3 || snapshot.mRacers.size() != 2
        || snapshot.mRacers[0].mState.mSpeed <= 0.0 || snapshot.mRacers[1].mState.mSpeed != 0.0
        || snapshot.mRacers[0].mProgress.mElapsedSeconds <= 0.0 || snapshot.mRacers[0].mPosition != 1
        || snapshot.mRacers[1].mPosition != 2 || snapshot.mMissiles.empty())
    {
        std::cerr << "authoritative race did not advance only the submitted input\n";
        return 1;
    }
    if (!race.Start({11}, 0) || !race.SubmitInput({11, 1.0, 1.0, false, false}))
    {
        std::cerr << "authoritative race did not start wall collision scenario\n";
        return 1;
    }
    const TrackDefinition& track = BuiltInTracks()[0];
    Course course(track.mWaypoints, track.mRoadHalfWidth);
    const HovercraftState spawnState = race.Snapshot().mRacers[0].mState;
    if (!course.IsOnRoad(spawnState.mX, spawnState.mY))
    {
        std::cerr << "authoritative racer spawned outside the course at (" << spawnState.mX
                  << ", " << spawnState.mY << ")\n";
        return 1;
    }
    for (int step = 0; step < 1800; ++step)
    {
        race.Step();
        const HovercraftState state = race.Snapshot().mRacers[0].mState;
        if (!course.IsOnRoad(state.mX, state.mY))
        {
            std::cerr << "authoritative racer escaped the course wall at step " << step
                      << " (" << state.mX << ", " << state.mY << ")\n";
            return 1;
        }
    }
    race.Stop();
    if (race.Active() || !race.Snapshot().mRacers.empty())
    {
        std::cerr << "authoritative race did not stop cleanly\n";
        return 1;
    }
    AuthoritativeRace rivalRace;
    if (!rivalRace.Start({11}, 0, 3, false, 2, RaceMode::SingleRace)
        || rivalRace.Snapshot().mRacers.size() != 3)
    {
        std::cerr << "authoritative race did not create configured AI rivals\n";
        return 1;
    }
    // An AI rival lined up behind a parked player shoots it: the player is spun out at some point
    // in the first twenty seconds of racing, and the missile is replicated like any other.
    {
        AuthoritativeRace shooting;
        // Rival classes vary with their names; fix one so the scenario does not depend on which
        // names are drawn.
        shooting.SetRivalCraftClass(CraftClass::Balanced);
        if (!shooting.Start({11}, 0, 3, true, 3, RaceMode::SingleRace))
        {
            std::cerr << "authoritative race did not start the AI weapons scenario\n";
            return 1;
        }
        bool playerSpunOut = false;
        bool aiMissileSeen = false;
        for (int step = 0; step < 120 * 28 && !(playerSpunOut && aiMissileSeen); ++step)
        {
            shooting.Step();
            const RaceSnapshot snapshot = shooting.Snapshot();
            for (const RaceMissileSnapshot& missile : snapshot.mMissiles)
                aiMissileSeen = aiMissileSeen || missile.mPlayerId >= 1000000;
            playerSpunOut = playerSpunOut || snapshot.mRacers[0].mState.mSpinOutSeconds > 0.0;
        }
        if (!aiMissileSeen)
        {
            std::cerr << "no AI rival fired a missile at the parked player\n";
            return 1;
        }
        if (!playerSpunOut)
        {
            std::cerr << "an AI missile never hit the parked player\n";
            return 1;
        }
        AuthoritativeRace peaceful;
        peaceful.Start({11}, 0, 3, false, 3, RaceMode::SingleRace);
        for (int step = 0; step < 120 * 28; ++step)
        {
            peaceful.Step();
            if (!peaceful.Snapshot().mMissiles.empty())
            {
                std::cerr << "missiles appeared in a race with weapons off\n";
                return 1;
            }
        }
    }
    // With weapons on, rivals shoot each other and the player, yet none may end up stuck: a full
    // grid of seven rivals finishes one lap of every track well inside two minutes.
    for (int track = 0; track < 3; ++track)
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            AuthoritativeRace grid;
            grid.Start({11}, track, 1, true, 7, RaceMode::SingleRace);
            for (int step = 0; step < 120 * 110; ++step)
                grid.Step();
            int finished = 0;
            for (const RaceRacerSnapshot& racer : grid.Snapshot().mRacers)
            {
                if (racer.mPlayerId >= 1000000 && racer.mProgress.mFinished)
                    ++finished;
            }
            if (finished != 7)
            {
                std::cerr << "only " << finished << " of 7 rivals finished track " << track
                          << " with weapons on\n";
                return 1;
            }
        }
    }
    for (int attempt = 0; attempt < 50; ++attempt)
    {
        AuthoritativeRace fullGrid;
        if (!fullGrid.Start({11}, 0, 3, false, 7, RaceMode::SingleRace))
        {
            std::cerr << "authoritative race did not start a full rival grid\n";
            return 1;
        }
        std::set<LobbyPlayerId> rivalIds;
        for (const RaceRacerSnapshot& racer : fullGrid.Snapshot().mRacers)
        {
            if (racer.mPlayerId >= 1000000)
                rivalIds.insert(racer.mPlayerId);
        }
        for (const RaceRacerSnapshot& racer : fullGrid.Snapshot().mRacers)
        {
            if (racer.mPlayerId >= 1000000
                && racer.mCraftClass != RivalCraftClass(static_cast<int>(racer.mPlayerId - 1000000)))
            {
                std::cerr << "an AI rival is not driving the craft class that goes with its name\n";
                return 1;
            }
        }
        if (rivalIds.size() != 7 || *rivalIds.rbegin() >= 1000000 + RivalNamePoolSize())
        {
            std::cerr << "authoritative rivals must carry distinct rival-name ids\n";
            return 1;
        }
    }
    return 0;
}
