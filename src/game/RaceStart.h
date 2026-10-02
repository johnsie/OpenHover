// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RACE_START_H
#define OPENHOVER_RACE_START_H

class RaceStart
{
public:
    explicit RaceStart(double pSeconds = 3.0);

    void Reset();
    void Begin();
    void Update(double pSeconds);
    bool Ready() const { return !mStarted && !mCountingDown; }
    bool CountdownActive() const { return mCountingDown && !mStarted; }
    bool Started() const { return mStarted; }
    int LightsLit() const;

private:
    double mDuration;
    double mRemaining;
    bool mCountingDown = false;
    bool mStarted = false;
};

#endif