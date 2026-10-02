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
    void SetVolume(double pVolume);
    double Volume() const { return mRaceVolume; }
    void SetMenuVolume(double pVolume);
    void SetRaceVolume(double pVolume);
    double MenuVolume() const { return mMenuVolume; }
    double RaceVolume() const { return mRaceVolume; }
    void PlayMenuMove();
    void PlayMenuConfirm();
    void PlayBoost();
    void PlayImpact();
    void PlayCheckpoint();

private:
    void QueueTone(double pStartFrequency, double pEndFrequency, double pSeconds,
                   double pVolume, double pMixVolume);

    SDL_AudioDeviceID mDevice = 0;
    bool mEnabled = true;
    double mMenuVolume = 1.0;
    double mRaceVolume = 1.0;
};

#endif
