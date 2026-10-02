// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RivalNames.h"

#include <iostream>
#include <set>

int main()
{
    bool ok = true;
    const auto expect = [&](bool pCondition, const char* pMessage)
    {
        if (!pCondition)
        {
            std::cerr << pMessage << '\n';
            ok = false;
        }
    };
    expect(RivalNamePoolSize() == 8, "pool has eight names");
    expect(RivalName(0) == "Anne Droid" && RivalName(2) == "Optimus Prime", "named roster");
    expect(RivalName(7) == "Cooper", "Cooper is in the pool");
    expect(RivalName(8) == "Anne Droid 2", "out-of-range index stays printable");
    expect(RivalName(-1) == "Anne Droid", "negative index is safe");

    std::set<std::string> everPicked;
    bool varied = false;
    const std::vector<int> reference = PickRivalNames(7, 1);
    for (unsigned int seed = 1; seed <= 200; ++seed)
    {
        const std::vector<int> picked = PickRivalNames(7, seed);
        std::set<int> unique(picked.begin(), picked.end());
        expect(picked.size() == 7 && unique.size() == 7, "rivals in one race never share a name");
        for (int index : picked)
        {
            expect(index >= 0 && index < RivalNamePoolSize(), "pick stays inside the pool");
            everPicked.insert(RivalName(index));
        }
        expect(picked == PickRivalNames(7, seed), "the same seed gives the same names");
        varied = varied || picked != reference;
    }
    expect(varied, "different seeds give different rival lineups");
    expect(everPicked.size() == 8, "every name is eventually used, including Cooper");
    expect(PickRivalNames(20, 5).size() == 8, "more rivals than names is limited to the pool");
    int perClass[3] = {0, 0, 0};
    for (int index = 0; index < RivalNamePoolSize(); ++index)
        ++perClass[static_cast<int>(RivalCraftClass(index))];
    expect(perClass[0] >= 2 && perClass[1] >= 2 && perClass[2] >= 2, "every craft class is driven by at least two names");
    expect(RivalCraftClass(2) == CraftClass::Sprint && RivalCraftClass(1) == CraftClass::Control,
           "Optimus Prime drives Sprint and Data drives Control");
    expect(RivalCraftClass(-1) == CraftClass::Balanced && RivalCraftClass(99) == CraftClass::Balanced,
           "an out-of-range name falls back to Balanced");
    expect(PickRivalNames(0, 5).empty() && PickRivalNames(-3, 5).empty(), "no rivals, no names");
    return ok ? 0 : 1;
}
