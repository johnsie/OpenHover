// SPDX-License-Identifier: MIT OR Apache-2.0
#include "CameraRig.h"

#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kSmoothRate = 7.0;
constexpr double kRiseFraction = 0.7;

double WrapAngle(double pAngle)
{
    while (pAngle > kPi)
        pAngle -= 2.0 * kPi;
    while (pAngle < -kPi)
        pAngle += 2.0 * kPi;
    return pAngle;
}
}

CameraMotion NextCameraMotion(CameraMotion pMotion, int pDirection)
{
    const int next = (static_cast<int>(pMotion) + pDirection + 3) % 3;
    return static_cast<CameraMotion>(next);
}

const char* CameraMotionName(CameraMotion pMotion)
{
    switch (pMotion)
    {
    case CameraMotion::Smooth:
        return "SMOOTH";
    case CameraMotion::Reduced:
        return "REDUCED";
    default:
        return "STANDARD";
    }
}

void CameraRig::Reset(double pHeading)
{
    mHeading = pHeading;
    mHeight = 0.0;
}

double CameraRig::Update(double pCraftHeading, double pFrameSeconds, CameraMotion pMotion)
{
    if (pMotion != CameraMotion::Smooth)
    {
        mHeading = pCraftHeading;
        return mHeading;
    }
    const double blend = 1.0 - std::exp(-kSmoothRate * (pFrameSeconds > 0.1 ? 0.1 : pFrameSeconds));
    mHeading = WrapAngle(mHeading + WrapAngle(pCraftHeading - mHeading) * blend);
    return mHeading;
}

double CameraRig::UpdateRise(double pCraftHeight, double pFrameSeconds, CameraMotion pMotion)
{
    const double rate = pMotion == CameraMotion::Reduced ? 2.5 : 5.0;
    const double blend = 1.0 - std::exp(-rate * (pFrameSeconds > 0.1 ? 0.1 : pFrameSeconds));
    mHeight += (pCraftHeight - mHeight) * blend;
    return mHeight * kRiseFraction;
}

double CameraRig::SpeedZoomScale(CameraMotion pMotion)
{
    return pMotion == CameraMotion::Reduced ? 0.0 : 1.0;
}
