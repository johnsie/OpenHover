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

HovercraftTuning CraftClassTuning(CraftClass pCraftClass)
{
    HovercraftTuning tuning;
    switch (pCraftClass)
    {
    case CraftClass::Balanced:
        break;
    case CraftClass::Sprint:
        tuning.mAcceleration = 32.0;
        tuning.mTurnRate = 2.15;
        tuning.mBoostAcceleration = 48.0;
        tuning.mLinearDrag = 0.48;
        break;
    case CraftClass::Control:
        tuning.mAcceleration = 25.0;
        tuning.mTurnRate = 3.0;
        tuning.mBoostAcceleration = 34.0;
        tuning.mLinearDrag = 0.72;
        break;
    }
    return tuning;
}