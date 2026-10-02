// SPDX-License-Identifier: MIT OR Apache-2.0
// Drives AuthoritativeRace with the input streams a lossy, laggy, mixed-rate network would
// deliver and checks that the authoritative result stays valid and reproducible.
#include "AuthoritativeRace.h"

#include <cmath>
#include <deque>
#include <iostream>
#include <vector>

namespace
{
struct Link
{
    int mDelayTicks = 0;
    int mSendEveryTicks = 1;
    int mDropPercent = 0;
};

// Deterministic pseudo-random loss so runs are repeatable.
class Lcg
{
public:
    unsigned int Next()
    {
        mState = mState * 1664525u + 1013904223u;
        return mState >> 16;
    }

private:
    unsigned int mState = 12345u;
};

RaceSnapshot Run(const Link& pLinkA, const Link& pLinkB, int pTicks)
{
    AuthoritativeRace race;
    race.Start({11, 22}, 0, 3, false);
    struct Pending
    {
        int mDeliverTick;
        RaceInputCommand mCommand;
    };
    std::deque<Pending> queue;
    Lcg random;
    for (int tick = 0; tick < pTicks; ++tick)
    {
        for (int player = 0; player < 2; ++player)
        {
            const Link& link = player == 0 ? pLinkA : pLinkB;
            if (tick % link.mSendEveryTicks != 0)
                continue;
            if (static_cast<int>(random.Next() % 100) < link.mDropPercent)
                continue;
            RaceInputCommand command;
            command.mPlayerId = player == 0 ? 11 : 22;
            command.mThrottle = 1.0;
            command.mSteering = ((tick / 90) % 2 == 0) ? 0.2 : -0.2;
            queue.push_back({tick + link.mDelayTicks, command});
        }
        while (!queue.empty() && queue.front().mDeliverTick <= tick)
        {
            race.SubmitInput(queue.front().mCommand);
            queue.pop_front();
        }
        race.Step();
    }
    return race.Snapshot();
}

bool Finite(const RaceSnapshot& pSnapshot)
{
    for (const RaceRacerSnapshot& racer : pSnapshot.mRacers)
    {
        if (!std::isfinite(racer.mState.mX) || !std::isfinite(racer.mState.mY)
            || !std::isfinite(racer.mState.mSpeed) || !std::isfinite(racer.mState.mHeading))
            return false;
    }
    return true;
}
}

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
    const int ticks = 1200;
    const Link clean{0, 1, 0};
    const Link impaired{12, 3, 30};

    const RaceSnapshot first = Run(impaired, clean, ticks);
    const RaceSnapshot second = Run(impaired, clean, ticks);
    expect(Finite(first), "impaired input produced non-finite state");
    expect(first.mRacers.size() == 2 && first.mRacers[0].mState.mSpeed > 0.0,
           "a racer with lossy, delayed input still drives from held input");
    bool identical = first.mRacers.size() == second.mRacers.size();
    for (std::size_t i = 0; identical && i < first.mRacers.size(); ++i)
        identical = first.mRacers[i].mState.mX == second.mRacers[i].mState.mX
            && first.mRacers[i].mState.mY == second.mRacers[i].mState.mY;
    expect(identical, "identical network conditions must give an identical authoritative result");

    const RaceSnapshot silent = Run({0, 1000000, 0}, clean, ticks);
    expect(Finite(silent) && silent.mRacers[1].mState.mSpeed > 0.0,
           "a racer sending one input does not stall the other");

    const RaceSnapshot slow = Run({0, 20, 0}, clean, ticks);
    expect(Finite(slow) && slow.mRacers[0].mState.mSpeed > 0.0,
           "a low input rate keeps the last held input active");

    const RaceSnapshot allLost = Run({0, 1, 100}, {0, 1, 100}, ticks);
    expect(Finite(allLost) && allLost.mRacers[0].mState.mSpeed == 0.0
               && allLost.mTick == static_cast<unsigned int>(ticks),
           "total loss leaves racers parked while the race keeps ticking");
    return ok ? 0 : 1;
}
