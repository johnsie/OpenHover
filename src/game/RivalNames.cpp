// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RivalNames.h"

#include <algorithm>
#include <random>

namespace
{
const char* const kNames[] = {"Anne Droid", "Data", "Optimus Prime", "Davros",
                              "Tiktok", "Kryten", "Roomba", "Cooper"};
constexpr int kNameCount = static_cast<int>(sizeof(kNames) / sizeof(kNames[0]));
}

int RivalNamePoolSize()
{
    return kNameCount;
}

std::string RivalName(int pPoolIndex)
{
    if (pPoolIndex < 0)
        pPoolIndex = 0;
    std::string name = kNames[pPoolIndex % kNameCount];
    if (pPoolIndex >= kNameCount)
        name += " " + std::to_string(pPoolIndex / kNameCount + 1);
    return name;
}

CraftClass RivalCraftClass(int pPoolIndex)
{
    // Anne Droid, Data, Optimus Prime, Davros, Tiktok, Kryten, Roomba, Cooper.
    static const CraftClass classes[kNameCount] = {
        CraftClass::Balanced, CraftClass::Control, CraftClass::Sprint, CraftClass::Balanced,
        CraftClass::Sprint, CraftClass::Control, CraftClass::Balanced, CraftClass::Sprint};
    if (pPoolIndex < 0 || pPoolIndex >= kNameCount)
        return CraftClass::Balanced;
    return classes[pPoolIndex];
}

std::vector<int> PickRivalNames(int pCount, unsigned int pSeed)
{
    std::vector<int> pool(kNameCount);
    for (int index = 0; index < kNameCount; ++index)
        pool[index] = index;
    std::mt19937 random(pSeed);
    // Fisher-Yates written out, so the result does not depend on the standard library's shuffle.
    for (int index = kNameCount - 1; index > 0; --index)
    {
        const int other = static_cast<int>(random() % static_cast<unsigned int>(index + 1));
        std::swap(pool[index], pool[other]);
    }
    pool.resize(std::max(0, std::min(pCount, kNameCount)));
    return pool;
}
