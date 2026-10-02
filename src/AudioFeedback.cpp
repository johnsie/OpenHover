// SPDX-License-Identifier: MIT OR Apache-2.0
#include "AudioFeedback.h"

#include <cmath>
#include <vector>

namespace
{
const int kSampleRate = 44100;
const double kTau = 6.28318530717958647692;
}

bool AudioFeedback::Initialize()
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return false;

    SDL_AudioSpec specification;
    SDL_zero(specification);
    specification.freq = kSampleRate;
    specification.format = AUDIO_F32SYS;
    specification.channels = 1;
    specification.samples = 1024;
    mDevice = SDL_OpenAudioDevice(nullptr, 0, &specification, nullptr, 0);
    if (mDevice == 0)
        return false;

    SDL_PauseAudioDevice(mDevice, 0);
    return true;
}

void AudioFeedback::Shutdown()
{
    if (mDevice != 0)
    {
        SDL_CloseAudioDevice(mDevice);
        mDevice = 0;
    }
}

void AudioFeedback::SetEnabled(bool pEnabled)
{
    mEnabled = pEnabled;
    if (!mEnabled && mDevice != 0)
        SDL_ClearQueuedAudio(mDevice);
}

void AudioFeedback::SetVolume(double pVolume)
{
    SetMenuVolume(pVolume);
    SetRaceVolume(pVolume);
}

void AudioFeedback::SetMenuVolume(double pVolume)
{
    mMenuVolume = pVolume < 0.0 ? 0.0 : pVolume > 1.0 ? 1.0 : pVolume;
}

void AudioFeedback::SetRaceVolume(double pVolume)
{
    mRaceVolume = pVolume < 0.0 ? 0.0 : pVolume > 1.0 ? 1.0 : pVolume;
    if (mMenuVolume == 0.0 && mRaceVolume == 0.0 && mDevice != 0)
        SDL_ClearQueuedAudio(mDevice);
}

void AudioFeedback::PlayMenuMove()
{
    QueueTone(680.0, 780.0, 0.04, 0.12, mMenuVolume);
}

void AudioFeedback::PlayMenuConfirm()
{
    QueueTone(440.0, 880.0, 0.09, 0.16, mMenuVolume);
}

void AudioFeedback::PlayBoost()
{
    QueueTone(160.0, 440.0, 0.12, 0.18, mRaceVolume);
}

void AudioFeedback::PlayImpact()
{
    QueueTone(150.0, 70.0, 0.10, 0.18, mRaceVolume);
}

void AudioFeedback::PlayCheckpoint()
{
    QueueTone(660.0, 990.0, 0.14, 0.16, mRaceVolume);
}

void AudioFeedback::QueueTone(double pStartFrequency, double pEndFrequency, double pSeconds,
                               double pVolume, double pMixVolume)
{
    if (!mEnabled || mDevice == 0
        || SDL_GetQueuedAudioSize(mDevice) > kSampleRate * sizeof(float) / 2)
        return;

    const int sampleCount = static_cast<int>(pSeconds * kSampleRate);
    std::vector<float> samples(sampleCount);
    double phase = 0.0;
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const double progress = static_cast<double>(sample) / sampleCount;
        const double frequency = pStartFrequency + (pEndFrequency - pStartFrequency) * progress;
        const double envelope = std::sin(kTau * progress * 0.5);
        phase += kTau * frequency / kSampleRate;
        samples[sample] = static_cast<float>(std::sin(phase) * envelope * pVolume * pMixVolume);
    }
    SDL_QueueAudio(mDevice, samples.data(), static_cast<Uint32>(samples.size() * sizeof(float)));
}
