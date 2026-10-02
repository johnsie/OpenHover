// SPDX-License-Identifier: MIT OR Apache-2.0
#include "KeyBindings.h"

#include <SDL2/SDL_scancode.h>

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
    KeyBindings keys;
    bool ok = Expect(keys.Key(BindAction::Accelerate) == SDL_SCANCODE_W, "default accelerate is W")
        && Expect(keys.Key(BindAction::Recover) == SDL_SCANCODE_X, "default recover is X");
    ok = Expect(keys.Rebind(BindAction::Fire, SDL_SCANCODE_SPACE), "space is bindable")
        && Expect(keys.Key(BindAction::Fire) == SDL_SCANCODE_SPACE, "fire rebinds") && ok;
    ok = Expect(keys.Rebind(BindAction::Brake, SDL_SCANCODE_W), "stealing a key is allowed")
        && Expect(keys.Key(BindAction::Brake) == SDL_SCANCODE_W, "brake takes W")
        && Expect(keys.Key(BindAction::Accelerate) == SDL_SCANCODE_S, "conflict swaps keys") && ok;
    ok = Expect(!keys.Rebind(BindAction::Fire, SDL_SCANCODE_ESCAPE), "escape is reserved")
        && Expect(!keys.Rebind(BindAction::Fire, SDL_SCANCODE_UP), "arrow keys are reserved")
        && Expect(!keys.Rebind(BindAction::Fire, -1), "invalid scancode rejected")
        && Expect(keys.Key(BindAction::Fire) == SDL_SCANCODE_SPACE, "rejected rebind changes nothing")
        && ok;

    KeyBindings loaded;
    ok = Expect(loaded.Parse(keys.Serialize()), "serialized bindings parse")
        && Expect(loaded.Key(BindAction::Fire) == SDL_SCANCODE_SPACE, "round trip keeps fire")
        && Expect(loaded.Key(BindAction::Accelerate) == SDL_SCANCODE_S, "round trip keeps swap")
        && ok;
    KeyBindings untouched;
    ok = Expect(!untouched.Parse("26 22 4 7"), "short text rejected")
        && Expect(!untouched.Parse("26 26 4 7 224 27"), "duplicates rejected")
        && Expect(!untouched.Parse("26 22 4 7 224 27 9"), "trailing data rejected")
        && Expect(!untouched.Parse("abc"), "garbage rejected")
        && Expect(untouched.Key(BindAction::Accelerate) == SDL_SCANCODE_W, "failed parse changes nothing")
        && ok;
    untouched.Rebind(BindAction::Recover, SDL_SCANCODE_C);
    untouched.ResetDefaults();
    ok = Expect(untouched.Key(BindAction::Recover) == SDL_SCANCODE_X, "reset restores defaults") && ok;
    return ok ? 0 : 1;
}
