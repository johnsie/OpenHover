// SPDX-License-Identifier: MIT OR Apache-2.0
#include "PracticeGuide.h"

#include <iostream>

namespace
{
bool Expect(bool pCondition, const char* pMessage)
{
    if (!pCondition)
        std::cerr << pMessage << '\n';
    return pCondition;
}
}

int main()
{
    PracticeGuide guide;
    guide.Reset(true);
    bool ok = Expect(guide.Step() == PracticeStep::Accelerate, "practice starts with acceleration")
        && Expect(guide.TotalStepCount() == 7, "weapon practice has seven steps");
    guide.ObserveCheckpoint();
    ok = Expect(guide.Step() == PracticeStep::Accelerate, "future actions do not skip teaching") && ok;
    guide.ObserveInput(1.0, 0.0, false);
    guide.ObserveInput(0.0, -0.8, false);
    guide.ObserveCheckpoint();
    guide.ObserveBoost();
    guide.ObserveInput(0.0, 0.0, true);
    guide.ObserveRecovery();
    guide.ObserveFire();
    ok = Expect(guide.Step() == PracticeStep::Complete, "all practice actions complete the guide") && ok;
    ok = Expect(guide.CompletedStepCount() == 7, "completed count includes every weapon step") && ok;

    guide.Reset(false);
    guide.ObserveInput(1.0, 0.0, false);
    guide.ObserveInput(0.0, 0.8, false);
    guide.ObserveCheckpoint();
    guide.ObserveBoost();
    guide.ObserveInput(0.0, 0.0, true);
    guide.ObserveRecovery();
    ok = Expect(guide.Step() == PracticeStep::Complete, "weapons-off practice skips firing") && ok;
    ok = Expect(guide.TotalStepCount() == 6, "weapons-off practice reports six steps") && ok;
    return ok ? 0 : 1;
}
