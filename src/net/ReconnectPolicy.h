// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_RECONNECT_POLICY_H
#define OPENHOVER_RECONNECT_POLICY_H

// Decides when a client that lost an established server connection should try again. A first
// connection that never succeeded is not retried: that is "server unavailable", not a dropped
// session. Delays grow after each failed attempt and the policy gives up after a fixed number.
class ReconnectPolicy
{
public:
    static constexpr int kMaxAttempts = 5;

    // Forget everything, e.g. when the player opens multiplayer afresh or leaves it.
    void Reset();
    void OnConnected();
    // Records a lost connection. Returns true when another attempt has been scheduled.
    bool OnLost(double pNowSeconds);
    // True once, when a scheduled attempt is due.
    bool TakeDueAttempt(double pNowSeconds);
    bool Pending() const { return mPending; }
    // Number of the attempt that is scheduled or under way, starting at 1.
    int Attempt() const { return mAttempts; }
    double SecondsUntilNext(double pNowSeconds) const;

private:
    bool mEverConnected = false;
    bool mPending = false;
    int mAttempts = 0;
    double mNextAttemptSeconds = 0.0;
};

#endif
