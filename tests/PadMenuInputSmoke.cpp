// SPDX-License-Identifier: MIT OR Apache-2.0
#include "PadMenuInput.h"

#include <SDL2/SDL_gamecontroller.h>
#include <SDL2/SDL_keycode.h>

#include <iostream>

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
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_DPAD_UP, PadMenuContext::Menu) == SDLK_UP, "dpad up");
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_A, PadMenuContext::Menu) == SDLK_RETURN, "A confirms");
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_B, PadMenuContext::Menu) == SDLK_ESCAPE, "B goes back");
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_START, PadMenuContext::Menu) == SDLK_ESCAPE, "start pauses");
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_START, PadMenuContext::Driving) == SDLK_ESCAPE,
           "start pauses while driving");
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_B, PadMenuContext::Driving) == 0, "B is gameplay while driving");
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_A, PadMenuContext::Driving) == 0, "A is ignored while driving");
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_DPAD_DOWN, PadMenuContext::Driving) == 0, "dpad ignored driving");
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_A, PadMenuContext::LobbyList) == 0, "lobby handles A itself");
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_B, PadMenuContext::LobbyList) == SDLK_ESCAPE, "lobby B backs out");
    expect(PadMenuKey(SDL_CONTROLLER_BUTTON_X, PadMenuContext::Menu) == 0, "X is not a menu button");
    return ok ? 0 : 1;
}
