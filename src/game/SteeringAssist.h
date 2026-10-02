// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_STEERING_ASSIST_H
#define OPENHOVER_STEERING_ASSIST_H

#include "Hovercraft.h"
#include "Race.h"

HovercraftInput ApplySteeringAssist(const HovercraftInput& pInput, const HovercraftState& pState,
                                    const RaceGate& pTarget, double pStrength = 1.2,
                                    double pBlend = 0.35);
HovercraftInput ApplyBrakingAssist(const HovercraftInput& pInput, const HovercraftState& pState,
                                   const RaceGate& pTarget, double pTurnThreshold = 0.65,
                                   double pMinimumSpeed = 8.0);

#endif