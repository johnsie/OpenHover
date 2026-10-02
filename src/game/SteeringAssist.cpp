// SPDX-License-Identifier: MIT OR Apache-2.0
#include "SteeringAssist.h"

#include <cmath>

namespace
{
const double kPi = 3.14159265358979323846;

double Clamp(double pValue, double pMinimum, double pMaximum)
{
    return pValue < pMinimum ? pMinimum : (pValue > pMaximum ? pMaximum : pValue);
}

double WrapAngle(double pAngle)
{
    while (pAngle > kPi)
        pAngle -= 2.0 * kPi;
    while (pAngle < -kPi)
        pAngle += 2.0 * kPi;
    return pAngle;
}

double HeadingError(const HovercraftState& pState, const RaceGate& pTarget)
{
    return WrapAngle(std::atan2(pTarget.mY - pState.mY, pTarget.mX - pState.mX)
                     - pState.mHeading);
}
}

HovercraftInput ApplySteeringAssist(const HovercraftInput& pInput, const HovercraftState& pState,
                                    const RaceGate& pTarget, double pStrength, double pBlend)
{
    HovercraftInput assistedInput = pInput;
    const double deltaX = pTarget.mX - pState.mX;
    const double deltaY = pTarget.mY - pState.mY;
    if (deltaX == 0.0 && deltaY == 0.0)
        return assistedInput;

    const double suggestedSteering = Clamp(HeadingError(pState, pTarget) * pStrength, -1.0, 1.0);
    const double blend = Clamp(pBlend, 0.0, 1.0);
    assistedInput.mSteering = Clamp(pInput.mSteering * (1.0 - blend) + suggestedSteering * blend,
                                    -1.0, 1.0);
    return assistedInput;
}

HovercraftInput ApplyBrakingAssist(const HovercraftInput& pInput, const HovercraftState& pState,
                                   const RaceGate& pTarget, double pTurnThreshold,
                                   double pMinimumSpeed)
{
    HovercraftInput assistedInput = pInput;
    if (pInput.mThrottle <= 0.0 || pState.mSpeed < pMinimumSpeed)
        return assistedInput;

    const double deltaX = pTarget.mX - pState.mX;
    const double deltaY = pTarget.mY - pState.mY;
    if (deltaX * deltaX + deltaY * deltaY <= pTarget.mRadius * pTarget.mRadius)
        return assistedInput;

    const double threshold = Clamp(pTurnThreshold, 0.0, kPi);
    const double turnAmount = std::fabs(HeadingError(pState, pTarget));
    if (turnAmount <= threshold)
        return assistedInput;

    const double turnRatio = Clamp((turnAmount - threshold) / (kPi - threshold), 0.0, 1.0);
    assistedInput.mThrottle = pInput.mThrottle * (1.0 - turnRatio);
    return assistedInput;
}