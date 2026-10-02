// SPDX-License-Identifier: MIT OR Apache-2.0
#include "StallDetector.h"

#include <iostream>

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
    StallDetector detector;
    // Driving normally never stalls.
    bool stalled = false;
    for (int step = 0; step < 120 * 20; ++step)
        stalled = stalled || detector.Update(step * 0.25, 0.0, 1.0 / 120.0);
    expect(!stalled, "a moving craft is never reported as stalled");

    // Sitting in one place stalls after about three seconds, and only once per three seconds.
    detector.Reset();
    int firstStall = -1;
    int stalls = 0;
    for (int step = 0; step < 120 * 10; ++step)
    {
        if (detector.Update(10.0 + (step % 2) * 0.5, 4.0, 1.0 / 120.0))
        {
            ++stalls;
            if (firstStall < 0)
                firstStall = step;
        }
    }
    expect(firstStall >= 120 * 3 - 2 && firstStall <= 120 * 3 + 2, "a stuck craft stalls after three seconds");
    expect(stalls == 3, "a craft that stays stuck is reported every three seconds");

    // Moving faster than the threshold (6 m in 3 s, about 2 m/s) is not a stall, but crawling
    // slower than that is.
    detector.Reset();
    stalled = false;
    for (int step = 0; step < 120 * 12; ++step)
        stalled = stalled || detector.Update(step * 0.03, 0.0, 1.0 / 120.0); // 3.6 m/s
    expect(!stalled, "a craft moving at 3.6 m/s is not stalled");
    detector.Reset();
    stalled = false;
    for (int step = 0; step < 120 * 12; ++step)
        stalled = stalled || detector.Update(step * 0.01, 0.0, 1.0 / 120.0); // 1.2 m/s
    expect(stalled, "a craft crawling at 1.2 m/s is stalled");

    // A craft that moves, stops, then moves again resets its timer.
    detector.Reset();
    stalled = false;
    for (int step = 0; step < 120 * 2; ++step)
        stalled = stalled || detector.Update(0.0, 0.0, 1.0 / 120.0);
    detector.Update(50.0, 0.0, 1.0 / 120.0);
    for (int step = 0; step < 120 * 2; ++step)
        stalled = stalled || detector.Update(50.0, 0.0, 1.0 / 120.0);
    expect(!stalled, "moving away resets the stall timer");
    return ok ? 0 : 1;
}
