// SPDX-License-Identifier: MIT OR Apache-2.0
#include "CraftClass.h"

CraftClass NextCraftClass(CraftClass pCraftClass)
{
    switch (pCraftClass)
    {
    case CraftClass::Balanced:
        return CraftClass::Sprint;
    case CraftClass::Sprint:
        return CraftClass::Control;
    case CraftClass::Control:
        return CraftClass::Balanced;
    }
    return CraftClass::Balanced;
}

const char* CraftClassName(CraftClass pCraftClass)
{
    switch (pCraftClass)
    {
    case CraftClass::Balanced:
        return "Balanced";
    case CraftClass::Sprint:
        return "Sprint";
    case CraftClass::Control:
        return "Control";
    }
    return "Balanced";
}

const char* CraftClassDescription(CraftClass pCraftClass)
{
    switch (pCraftClass)
    {
    case CraftClass::Balanced:
        return "VERSATILE SPEED AND HANDLING";
    case CraftClass::Sprint:
        return "FASTEST WITH WIDER TURNS";
    case CraftClass::Control:
        return "SHARP TURNS LOWER TOP SPEED";
    }
    return "VERSATILE SPEED AND HANDLING";
}

HovercraftTuning CraftClassTuning(CraftClass pCraftClass)
{
    HovercraftTuning tuning;
    switch (pCraftClass)
    {
    case CraftClass::Balanced:
        break;
    case CraftClass::Sprint:
        tuning.mAcceleration = 30.0;
        tuning.mTurnRate = 2.15;
        tuning.mBoostAcceleration = 30.0;
        tuning.mMaximumSpeed = 38.0;
        tuning.mBoostMaximumSpeed = 44.0;
        break;
    case CraftClass::Control:
        tuning.mAcceleration = 26.0;
        tuning.mTurnRate = 3.0;
        tuning.mBoostAcceleration = 26.0;
        tuning.mLinearDrag = 0.5;
        tuning.mMaximumSpeed = 33.0;
        tuning.mBoostMaximumSpeed = 40.0;
        break;
    }
    return tuning;
}
