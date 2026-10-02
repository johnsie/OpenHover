// SPDX-License-Identifier: MIT OR Apache-2.0
#include "AudioFeedback.h"

#include <cmath>
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
    audioFeedback.SetVolume(0.35);
    if (std::fabs(audioFeedback.MenuVolume() - 0.35) > 0.0001
        || std::fabs(audioFeedback.RaceVolume() - 0.35) > 0.0001)
    {
        std::cerr << "audio feedback volume was not retained\n";
        return 1;
    }
    audioFeedback.SetVolume(2.0);
    if (audioFeedback.Volume() != 1.0)
    {
        std::cerr << "audio feedback volume was not clamped\n";
        return 1;
    }
    audioFeedback.SetMenuVolume(0.2);
    audioFeedback.SetRaceVolume(0.8);
    if (std::fabs(audioFeedback.MenuVolume() - 0.2) > 0.0001
        || std::fabs(audioFeedback.RaceVolume() - 0.8) > 0.0001)
    {
        std::cerr << "independent audio levels were not retained\n";
        return 1;
    }
    return 0;
}
