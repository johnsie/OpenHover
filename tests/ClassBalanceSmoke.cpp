// SPDX-License-Identifier: MIT OR Apache-2.0
// Checks that the three craft classes stay competitive with each other on every built-in track,
// using the AI driver as a repeatable reference. The classes should feel different (each at least a
// couple of percent away from Balanced) but no class may be so slow or so fast that the choice
// is made for the player.
#include "AuthoritativeRace.h"
#include "TrackDefinition.h"

#include <iostream>

namespace
{
// Seconds the AI takes for one lap in the given class, or 0 if it never finishes one.
double LapSeconds(int pTrack, CraftClass pCraftClass)
{
    AuthoritativeRace race;
    race.SetRivalCraftClass(pCraftClass);
    race.Start({11}, pTrack, 2, false, 1);
    for (int step = 0; step < 120 * 300; ++step)
    {
        race.Step();
        if (step % 30 != 0)
            continue;
        for (const RaceRacerSnapshot& racer : race.Snapshot().mRacers)
        {
            if (racer.mPlayerId >= 1000000 && racer.mLapTiming.mLastSeconds > 0.0)
                return racer.mLapTiming.mLastSeconds;
        }
    }
    return 0.0;
}
}

int main()
{
    bool ok = true;
    for (int track = 0; track < static_cast<int>(BuiltInTracks().size()); ++track)
    {
        const double balanced = LapSeconds(track, CraftClass::Balanced);
        const double sprint = LapSeconds(track, CraftClass::Sprint);
        const double control = LapSeconds(track, CraftClass::Control);
        const std::string& id = BuiltInTracks()[track].mId;
        if (balanced <= 0.0 || sprint <= 0.0 || control <= 0.0)
        {
            std::cerr << id << ": a class could not finish a lap\n";
            ok = false;
            continue;
        }
        const double sprintRatio = sprint / balanced;
        const double controlRatio = control / balanced;
        if (sprintRatio < 0.88 || sprintRatio > 1.0 || controlRatio < 1.02 || controlRatio > 1.15)
        {
            std::cerr << id << ": class lap times are out of balance (Sprint " << sprintRatio
                      << "x, Control " << controlRatio << "x of Balanced)\n";
            ok = false;
        }
        if (sprint >= control)
        {
            std::cerr << id << ": Sprint must out-run Control for the AI's fixed line\n";
            ok = false;
        }
    }
    return ok ? 0 : 1;
}
