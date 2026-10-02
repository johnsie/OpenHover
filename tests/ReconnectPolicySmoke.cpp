// SPDX-License-Identifier: MIT OR Apache-2.0
#include "ReconnectPolicy.h"

#include <iostream>

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
    ReconnectPolicy policy;
    expect(!policy.OnLost(0.0), "a connection that never succeeded is not retried");
    policy.OnConnected();
    expect(policy.OnLost(10.0), "a dropped session schedules a retry");
    expect(policy.Attempt() == 1 && policy.Pending(), "first attempt pending");
    expect(!policy.TakeDueAttempt(10.5), "attempt is not due early");
    expect(policy.SecondsUntilNext(10.5) > 0.4 && policy.SecondsUntilNext(10.5) < 0.6, "countdown");
    expect(policy.TakeDueAttempt(11.0), "attempt is due after the delay");
    expect(!policy.TakeDueAttempt(12.0), "attempt is taken once");
    expect(policy.OnLost(11.0) && policy.Attempt() == 2, "second attempt scheduled");
    expect(!policy.TakeDueAttempt(12.5) && policy.TakeDueAttempt(13.0), "delay doubles to two seconds");
    for (int attempt = 3; attempt <= ReconnectPolicy::kMaxAttempts; ++attempt)
        expect(policy.OnLost(100.0) && policy.Attempt() == attempt, "attempts continue to the limit");
    expect(!policy.OnLost(200.0) && !policy.Pending(), "policy gives up after the last attempt");
    policy.OnConnected();
    expect(policy.Attempt() == 0 && policy.OnLost(300.0) && policy.Attempt() == 1,
           "success resets the attempt count");
    policy.Reset();
    expect(!policy.OnLost(400.0), "reset forgets the earlier session");
    return ok ? 0 : 1;
}
