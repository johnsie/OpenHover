// SPDX-License-Identifier: MIT OR Apache-2.0
#include "InputRecording.h"

#include <iostream>

int main()
{
    InputRecording recording;
    HovercraftInput first;
    first.mThrottle = 1.0;
    first.mBoost = true;
    first.mJump = true;
    HovercraftInput second;
    second.mSteering = -0.5;
    recording.Record(first);
    recording.Record(second);

    HovercraftInput replayed;
    if (recording.FrameCount() != 2 || !recording.InputAt(0, replayed)
        || replayed.mThrottle != 1.0 || !replayed.mBoost || !replayed.mJump || !recording.InputAt(1, replayed)
        || replayed.mSteering != -0.5 || recording.InputAt(2, replayed))
    {
        std::cerr << "input recording did not replay exact simulation inputs\n";
        return 1;
    }

    recording.Clear();
    if (!recording.Empty() || recording.FrameCount() != 0)
    {
        std::cerr << "input recording did not clear\n";
        return 1;
    }
    return 0;
}