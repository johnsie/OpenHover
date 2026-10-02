// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Mine.h"

#include <iostream>

int main()
{
    Mine mine = {4.0, 2.0, 1.5};
    HovercraftState state;
    state.mX = 4.5;
    state.mY = 2.0;
    if (!ApplyMine(state, mine) || !mine.mTriggered || state.mSpinOutSeconds < 2.5)
    {
        std::cerr << "mine did not trigger a spinout\n";
        return 1;
    }
    if (ApplyMine(state, mine))
    {
        std::cerr << "triggered mine affected craft twice\n";
        return 1;
    }
    Mine distantMine = {10.0, 10.0, 1.0};
    if (ApplyMine(state, distantMine))
    {
        std::cerr << "distant mine affected craft\n";
        return 1;
    }
    return 0;
}