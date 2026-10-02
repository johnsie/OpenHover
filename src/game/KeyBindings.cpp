// SPDX-License-Identifier: MIT OR Apache-2.0
#include "KeyBindings.h"

#include <SDL2/SDL_scancode.h>

#include <cstdlib>

namespace
{
constexpr int kActionCount = static_cast<int>(BindAction::Count);

const int kDefaults[kActionCount] = {SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A,
                                     SDL_SCANCODE_D, SDL_SCANCODE_LCTRL, SDL_SCANCODE_X};
}

KeyBindings::KeyBindings()
{
    ResetDefaults();
}

void KeyBindings::ResetDefaults()
{
    for (int i = 0; i < kActionCount; ++i)
        mKeys[i] = kDefaults[i];
}

int KeyBindings::Key(BindAction pAction) const
{
    return mKeys[static_cast<int>(pAction)];
}

bool KeyBindings::IsBindable(int pScancode)
{
    if (pScancode <= SDL_SCANCODE_UNKNOWN || pScancode >= SDL_NUM_SCANCODES)
        return false;
    switch (pScancode)
    {
    case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
    case SDL_SCANCODE_BACKSPACE:
    case SDL_SCANCODE_TAB:
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
    case SDL_SCANCODE_UP:
    case SDL_SCANCODE_DOWN:
    case SDL_SCANCODE_LEFT:
    case SDL_SCANCODE_RIGHT:
    case SDL_SCANCODE_R:
        return false;
    default:
        return true;
    }
}

bool KeyBindings::Rebind(BindAction pAction, int pScancode)
{
    if (!IsBindable(pScancode))
        return false;
    const int index = static_cast<int>(pAction);
    for (int i = 0; i < kActionCount; ++i)
    {
        if (i != index && mKeys[i] == pScancode)
            mKeys[i] = mKeys[index];
    }
    mKeys[index] = pScancode;
    return true;
}

const char* KeyBindings::ActionName(BindAction pAction)
{
    static const char* const names[kActionCount] = {"ACCELERATE", "BRAKE", "STEER LEFT",
                                                    "STEER RIGHT", "FIRE", "RECOVER"};
    return names[static_cast<int>(pAction)];
}

std::string KeyBindings::Serialize() const
{
    std::string text;
    for (int i = 0; i < kActionCount; ++i)
    {
        if (i > 0)
            text += ' ';
        text += std::to_string(mKeys[i]);
    }
    return text;
}

bool KeyBindings::Parse(const std::string& pText)
{
    int parsed[kActionCount];
    const char* cursor = pText.c_str();
    for (int i = 0; i < kActionCount; ++i)
    {
        char* end = nullptr;
        const long value = std::strtol(cursor, &end, 10);
        if (end == cursor || !IsBindable(static_cast<int>(value)))
            return false;
        parsed[i] = static_cast<int>(value);
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r')
        ++cursor;
    if (*cursor != '\0')
        return false;
    for (int i = 0; i < kActionCount; ++i)
        for (int j = i + 1; j < kActionCount; ++j)
            if (parsed[i] == parsed[j])
                return false;
    for (int i = 0; i < kActionCount; ++i)
        mKeys[i] = parsed[i];
    return true;
}
