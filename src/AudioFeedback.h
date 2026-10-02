// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_AUDIO_FEEDBACK_H
#define OPENHOVER_AUDIO_FEEDBACK_H

#include <SDL.h>

class AudioFeedback
{
public:
    bool Initialize();
    void Shutdown();
    void SetEnabled(bool pEnabled);
    bool Enabled() const { return mEnabled; }
    void PlayMenuMove();
    void PlayMenuConfirm();
    void PlayBoost();
    void PlayImpact();
    void PlayCheckpoint();

private:
    void QueueTone(double pStartFrequency, double pEndFrequency, double pSeconds, double pVolume);

    SDL_AudioDeviceID mDevice = 0;
    bool mEnabled = true;
};

#endif