// SPDX-License-Identifier: MIT OR Apache-2.0
#include "GhostLibrary.h"

#include <iostream>

namespace
{
InputRecording Recording(int pFrames, double pSteering)
{
    InputRecording recording;
    for (int frame = 0; frame < pFrames; ++frame)
    {
        HovercraftInput input;
        input.mThrottle = 1.0 / 3.0;
        input.mSteering = pSteering + frame * 1e-9;
        input.mJump = frame % 5 == 0;
        input.mFire = frame % 7 == 0;
        input.mReverseFacing = frame % 11 == 0;
        recording.Record(input);
    }
    return recording;
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
    GhostLibrary library;
    expect(library.Find("harbor-loop", 0) == nullptr, "empty library has no ghost");
    expect(library.Submit("harbor-loop", 0, 70.0, Recording(40, 0.2)), "first run is stored");
    expect(!library.Submit("harbor-loop", 0, 71.0, Recording(40, 0.4)), "slower run is not stored");
    expect(!library.Submit("harbor-loop", 0, 70.0, Recording(40, 0.4)), "equal run is not stored");
    expect(library.Submit("harbor-loop", 0, 65.5, Recording(50, 0.3)), "faster run replaces it");
    expect(library.BestSeconds("harbor-loop", 0) == 65.5, "best time tracks the fastest run");
    expect(library.Find("harbor-loop", 0)->FrameCount() == 50, "ghost is the fastest run");
    expect(library.Submit("harbor-loop", 1, 80.0, Recording(10, 0.1)), "each craft class has its own ghost");
    expect(library.Submit("velocity-ring", 0, 90.0, Recording(10, 0.1)), "each track has its own ghost");
    expect(!library.Submit("velocity-ring", 1, 0.0, Recording(10, 0.1)), "zero time rejected");
    expect(!library.Submit("velocity-ring", 1, 50.0, InputRecording()), "empty recording rejected");
    expect(library.Count() == 3, "three ghosts stored");

    const std::string text = library.Serialize(201);
    GhostLibrary loaded;
    expect(loaded.Parse(text, 201), "saved ghosts load");
    expect(loaded.Count() == 3 && loaded.BestSeconds("harbor-loop", 0) == 65.5, "times survive a round trip");
    const InputRecording* original = library.Find("harbor-loop", 0);
    const InputRecording* copy = loaded.Find("harbor-loop", 0);
    bool identical = copy != nullptr && copy->FrameCount() == original->FrameCount();
    for (std::size_t frame = 0; identical && frame < original->FrameCount(); ++frame)
    {
        HovercraftInput a;
        HovercraftInput b;
        original->InputAt(frame, a);
        copy->InputAt(frame, b);
        identical = a.mThrottle == b.mThrottle && a.mSteering == b.mSteering && a.mJump == b.mJump
            && a.mFire == b.mFire && a.mReverseFacing == b.mReverseFacing && a.mBoost == b.mBoost;
    }
    expect(identical, "inputs replay bit for bit after loading");

    GhostLibrary wrongVersion;
    expect(!wrongVersion.Parse(text, 202) && wrongVersion.Count() == 0,
           "ghosts from another content or physics version are discarded");
    GhostLibrary damaged;
    damaged.Submit("keep", 0, 10.0, Recording(3, 0.1));
    expect(!damaged.Parse(text.substr(0, text.size() / 2), 201), "truncated file rejected");
    expect(!damaged.Parse(text + "extra", 201), "trailing data rejected");
    expect(!damaged.Parse("garbage", 201), "garbage rejected");
    expect(damaged.Count() == 1 && damaged.Find("keep", 0) != nullptr, "failed load changes nothing");
    return ok ? 0 : 1;
}
