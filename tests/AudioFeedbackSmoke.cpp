// SPDX-License-Identifier: MIT OR Apache-2.0
#include "AudioFeedback.h"

#include <iostream>

int main()
{
    AudioFeedback audioFeedback;
    if (!audioFeedback.Enabled())
    {
        std::cerr << "audio feedback should default to enabled\n";
        return 1;
    }

    audioFeedback.SetEnabled(false);
    if (audioFeedback.Enabled())
    {
        std::cerr << "audio feedback did not disable\n";
        return 1;
    }

    audioFeedback.SetEnabled(true);
    if (!audioFeedback.Enabled())
    {
        std::cerr << "audio feedback did not enable\n";
        return 1;
    }
    return 0;
}