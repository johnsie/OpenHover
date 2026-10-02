// SPDX-License-Identifier: MIT OR Apache-2.0
// Every built-in track must take a full-pace rival at least 50 seconds per lap, so a lap is long
// enough to settle in, and no longer than 75 seconds so it stays engaging.
#include "AuthoritativeRace.h"
#include "TrackDefinition.h"

#include <iostream>

int main()
{
    bool ok = true;
    for (int track = 0; track < static_cast<int>(BuiltInTracks().size()); ++track)
    {
        AuthoritativeRace race;
        race.Start({11}, track, 2, false, 1);
        double lapSeconds = 0.0;
        for (int step = 0; step < 60 * 200 && lapSeconds == 0.0; ++step)
        {
            race.Step();
            for (const RaceRacerSnapshot& racer : race.Snapshot().mRacers)
            {
                if (racer.mPlayerId >= 1000000)
                    lapSeconds = racer.mLapTiming.mLastSeconds;
            }
        }
        if (lapSeconds < 50.0 || lapSeconds > 75.0)
        {
            std::cerr << BuiltInTracks()[track].mId << " rival lap took " << lapSeconds
                      << " s, outside 50-75 s\n";
            ok = false;
        }
    }
    return ok ? 0 : 1;
}
