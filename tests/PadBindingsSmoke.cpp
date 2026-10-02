// SPDX-License-Identifier: MIT OR Apache-2.0
#include "PadBindings.h"

#include <SDL2/SDL_gamecontroller.h>

#include <iostream>

namespace
{
bool Expect(bool pCondition, const char* pMessage)
{
    if (!pCondition)
        std::cerr << pMessage << '\n';
    return pCondition;
}
}

int main()
{
    PadBindings pad;
    bool ok = Expect(pad.Button(PadAction::Jump) == SDL_CONTROLLER_BUTTON_B, "default jump is B")
        && Expect(pad.Button(PadAction::Recover) == SDL_CONTROLLER_BUTTON_Y, "default recover is Y");
    ok = Expect(pad.Rebind(PadAction::Fire, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER), "RB bindable")
        && Expect(pad.Button(PadAction::Fire) == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "fire rebinds")
        && ok;
    ok = Expect(pad.Rebind(PadAction::Jump, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER), "stealing allowed")
        && Expect(pad.Button(PadAction::Fire) == SDL_CONTROLLER_BUTTON_B, "conflict swaps buttons")
        && ok;
    ok = Expect(!pad.Rebind(PadAction::Jump, SDL_CONTROLLER_BUTTON_START), "start is reserved")
        && Expect(!pad.Rebind(PadAction::Jump, SDL_CONTROLLER_BUTTON_DPAD_UP), "dpad is reserved")
        && Expect(pad.Button(PadAction::Jump) == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                  "rejected rebind changes nothing") && ok;
    PadBindings loaded;
    ok = Expect(loaded.Parse(pad.Serialize()), "round trip parses")
        && Expect(loaded.Button(PadAction::Jump) == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "round trip keeps jump")
        && ok;
    PadBindings untouched;
    ok = Expect(!untouched.Parse("1 1 3"), "duplicates rejected")
        && Expect(!untouched.Parse("1 0"), "short text rejected")
        && Expect(!untouched.Parse("1 0 3 4"), "trailing data rejected")
        && Expect(!untouched.Parse("1 0 6"), "start rejected")
        && Expect(untouched.Button(PadAction::Jump) == SDL_CONTROLLER_BUTTON_B, "failed parse changes nothing")
        && ok;
    untouched.Rebind(PadAction::Fire, SDL_CONTROLLER_BUTTON_A);
    untouched.ResetDefaults();
    ok = Expect(untouched.Button(PadAction::Fire) == SDL_CONTROLLER_BUTTON_X, "reset restores defaults") && ok;
    return ok ? 0 : 1;
}
