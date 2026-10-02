// SPDX-License-Identifier: MIT OR Apache-2.0
#include "AuthoritativeRace.h"
#include "Course.h"
#include "TrackDefinition.h"

#include <iostream>
#include <vector>

int main()
{
    AuthoritativeRace race;
    const std::vector<LobbyPlayerId> players = {11, 22};
    if (!race.Start(players, 0) || !race.Active() || race.Start(players, 99))
    {
        std::cerr << "authoritative race did not validate its start state\n";
        return 1;
    }
    RaceInputCommand input;
    input.mPlayerId = 11;
    input.mThrottle = 1.0;
    if (!race.SubmitInput(input) || race.SubmitInput({99, 1.0, 0.0, false, false}))
    {
        std::cerr << "authoritative race did not validate player inputs\n";
        return 1;
    }
    for (int step = 0; step < 120; ++step)
        race.Step();
    const RaceSnapshot snapshot = race.Snapshot();
    if (snapshot.mTick != 120 || snapshot.mRacers.size() != 2
        || snapshot.mRacers[0].mState.mSpeed <= 0.0 || snapshot.mRacers[1].mState.mSpeed != 0.0)
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
    return 0;
}