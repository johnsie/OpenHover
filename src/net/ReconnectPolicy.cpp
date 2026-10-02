// SPDX-License-Identifier: MIT OR Apache-2.0
#include "ReconnectPolicy.h"

namespace
{
const double kDelaySeconds[ReconnectPolicy::kMaxAttempts] = {1.0, 2.0, 4.0, 8.0, 15.0};
}

void ReconnectPolicy::Reset()
{
    mEverConnected = false;
    mPending = false;
    mAttempts = 0;
    mNextAttemptSeconds = 0.0;
}

void ReconnectPolicy::OnConnected()
{
    mEverConnected = true;
    mPending = false;
    mAttempts = 0;
}

bool ReconnectPolicy::OnLost(double pNowSeconds)
{
    mPending = false;
    if (!mEverConnected || mAttempts >= kMaxAttempts)
        return false;
    mNextAttemptSeconds = pNowSeconds + kDelaySeconds[mAttempts];
    ++mAttempts;
    mPending = true;
    return true;
}

bool ReconnectPolicy::TakeDueAttempt(double pNowSeconds)
{
    if (!mPending || pNowSeconds < mNextAttemptSeconds)
        return false;
    mPending = false;
    return true;
}

double ReconnectPolicy::SecondsUntilNext(double pNowSeconds) const
{
    if (!mPending || pNowSeconds >= mNextAttemptSeconds)
        return 0.0;
    return mNextAttemptSeconds - pNowSeconds;
}
