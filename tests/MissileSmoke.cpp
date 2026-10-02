// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Missile.h"

#include <iostream>

int main()
{
    Course course({{0.0, 0.0, 1.0}, {80.0, 0.0, 1.0}}, 12.0);
    HovercraftState source;
    source.mHeight = 2.3;
    Missile missile;
    if (!missile.Fire(source) || missile.Fire(source))
    {
        std::cerr << "missile did not enforce one active launch\n";
        return 1;
    }
    if (missile.Ready() || missile.RechargeFraction() >= 1.0)
    {
        std::cerr << "missile did not enter recharge after launch\n";
        return 1;
    }
    const double launchHeight = missile.State().mHeight;
    const double launchSpeed = missile.State().mSpeed;
    missile.Step(0.1, course);
    if (missile.State().mHeight != launchHeight || missile.State().mSpeed <= launchSpeed)
    {
        std::cerr << "missile did not retain height and accelerate\n";
        return 1;
    }
    HovercraftState reverseSource;
    reverseSource.mReverseFacing = true;
    Missile reverseMissile;
    if (!reverseMissile.Fire(reverseSource) || reverseMissile.State().mX >= 0.0)
    {
        std::cerr << "reverse-facing craft did not launch missile backward\n";
        return 1;
    }
    HovercraftState target = missile.State();
    if (!missile.ApplyHit(target) || target.mSpinOutSeconds <= 0.0 || missile.Active())
    {
        std::cerr << "missile hit did not trigger a spin out\n";
        return 1;
    }
    Missile lifetimeMissile;
    if (!lifetimeMissile.Fire(source))
    {
        std::cerr << "missile did not launch for lifetime test\n";
        return 1;
    }
    for (int step = 0; step < 52; ++step)
        lifetimeMissile.Step(0.1, course);
    if (!lifetimeMissile.Active() || !lifetimeMissile.Sinking() ||
        lifetimeMissile.State().mHeight >= launchHeight)
    {
        std::cerr << "missile did not sink after its extended flight\n";
        return 1;
    }
    for (int step = 0; step < 20; ++step)
        lifetimeMissile.Step(0.1, course);
    if (lifetimeMissile.Active())
    {
        std::cerr << "sinking missile did not expire\n";
        return 1;
    }
    for (int step = 0; step < 100; ++step)
        missile.Step(0.1, course);
    if (!missile.Ready() || missile.RechargeFraction() != 1.0)
    {
        std::cerr << "missile did not recharge after its cooldown\n";
        return 1;
    }
    return 0;
}