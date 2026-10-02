// SPDX-License-Identifier: MIT OR Apache-2.0
#include "CraftClass.h"

#include <iostream>

int main()
{
    const HovercraftTuning balanced = CraftClassTuning(CraftClass::Balanced);
    const HovercraftTuning sprint = CraftClassTuning(CraftClass::Sprint);
    const HovercraftTuning control = CraftClassTuning(CraftClass::Control);
    if (sprint.mAcceleration <= balanced.mAcceleration || sprint.mTurnRate >= balanced.mTurnRate
        || control.mTurnRate <= balanced.mTurnRate || control.mAcceleration >= balanced.mAcceleration)
    {
        std::cerr << "craft class handling trade-offs are not distinct\n";
        return 1;
    }
    if (NextCraftClass(CraftClass::Balanced) != CraftClass::Sprint
        || NextCraftClass(CraftClass::Sprint) != CraftClass::Control
        || NextCraftClass(CraftClass::Control) != CraftClass::Balanced)
    {
        std::cerr << "craft class selection did not cycle\n";
        return 1;
    }
    if (CraftClassDescription(CraftClass::Balanced)[0] == '\0'
        || CraftClassDescription(CraftClass::Sprint)[0] == '\0'
        || CraftClassDescription(CraftClass::Control)[0] == '\0')
    {
        std::cerr << "craft classes did not provide handling descriptions\n";
        return 1;
    }
    return 0;
}
