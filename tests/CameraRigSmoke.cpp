// SPDX-License-Identifier: MIT OR Apache-2.0
#include "CameraRig.h"

#include <cmath>
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
    CameraRig rig;
    rig.Reset(0.0);
    bool ok = Expect(rig.Update(1.0, 0.016, CameraMotion::Standard) == 1.0, "standard locks heading")
        && Expect(rig.Update(2.0, 0.016, CameraMotion::Reduced) == 2.0, "reduced locks heading")
        && Expect(CameraRig::SpeedZoomScale(CameraMotion::Reduced) == 0.0, "reduced disables zoom")
        && Expect(CameraRig::SpeedZoomScale(CameraMotion::Smooth) == 1.0, "smooth keeps zoom");
    rig.Reset(0.0);
    const double first = rig.Update(1.0, 0.016, CameraMotion::Smooth);
    ok = Expect(first > 0.0 && first < 1.0, "smooth lags behind craft") && ok;
    double heading = first;
    for (int i = 0; i < 300; ++i)
        heading = rig.Update(1.0, 0.016, CameraMotion::Smooth);
    ok = Expect(std::fabs(heading - 1.0) < 0.001, "smooth converges") && ok;
    rig.Reset(3.0);
    const double wrapped = rig.Update(-3.0, 0.016, CameraMotion::Smooth);
    ok = Expect(wrapped > 3.0 || wrapped < -3.0, "smooth turns the short way across pi") && ok;
    rig.Reset(0.0);
    const double rise = rig.UpdateRise(4.0, 0.016, CameraMotion::Standard);
    ok = Expect(rise > 0.0 && rise < 4.0 * 0.7, "rise eases toward craft height") && ok;
    double settled = rise;
    for (int i = 0; i < 600; ++i)
        settled = rig.UpdateRise(4.0, 0.016, CameraMotion::Standard);
    ok = Expect(std::fabs(settled - 2.8) < 0.01, "rise settles at 70 percent of height") && ok;
    rig.Reset(0.0);
    ok = Expect(rig.UpdateRise(4.0, 0.016, CameraMotion::Reduced) < rise, "reduced rises slower") && ok;
    ok = Expect(NextCameraMotion(CameraMotion::Standard, -1) == CameraMotion::Reduced, "cycle back wraps")
        && Expect(NextCameraMotion(CameraMotion::Reduced, 1) == CameraMotion::Standard, "cycle forward wraps")
        && ok;
    return ok ? 0 : 1;
}
