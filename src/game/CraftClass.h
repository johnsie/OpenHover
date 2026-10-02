// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_CRAFT_CLASS_H
#define OPENHOVER_CRAFT_CLASS_H

#include "Hovercraft.h"

enum class CraftClass
{
    Balanced,
    Sprint,
    Control
};

CraftClass NextCraftClass(CraftClass pCraftClass);
const char* CraftClassName(CraftClass pCraftClass);
HovercraftTuning CraftClassTuning(CraftClass pCraftClass);

#endif