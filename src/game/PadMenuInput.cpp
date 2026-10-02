// SPDX-License-Identifier: MIT OR Apache-2.0
#include "PadMenuInput.h"

#include <SDL2/SDL_gamecontroller.h>
#include <SDL2/SDL_keycode.h>

int PadMenuKey(int pButton, PadMenuContext pContext)
{
    if (pButton == SDL_CONTROLLER_BUTTON_START)
        return SDLK_ESCAPE;
    if (pContext == PadMenuContext::Driving)
        return 0;
    if (pButton == SDL_CONTROLLER_BUTTON_B)
        return SDLK_ESCAPE;
    if (pContext == PadMenuContext::LobbyList)
        return 0;
    switch (pButton)
    {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return SDLK_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return SDLK_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return SDLK_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return SDLK_RIGHT;
    case SDL_CONTROLLER_BUTTON_A:
        return SDLK_RETURN;
    default:
        return 0;
    }
}
