// SPDX-License-Identifier: MIT OR Apache-2.0
#include "PadBindings.h"

#include <SDL2/SDL_gamecontroller.h>

#include <cstdlib>

namespace
{
constexpr int kActionCount = static_cast<int>(PadAction::Count);
const int kDefaults[kActionCount] = {SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X,
                                     SDL_CONTROLLER_BUTTON_Y};
}

PadBindings::PadBindings()
{
    ResetDefaults();
}

void PadBindings::ResetDefaults()
{
    for (int i = 0; i < kActionCount; ++i)
        mButtons[i] = kDefaults[i];
}

int PadBindings::Button(PadAction pAction) const
{
    return mButtons[static_cast<int>(pAction)];
}

bool PadBindings::IsBindable(int pButton)
{
    switch (pButton)
    {
    case SDL_CONTROLLER_BUTTON_A:
    case SDL_CONTROLLER_BUTTON_B:
    case SDL_CONTROLLER_BUTTON_X:
    case SDL_CONTROLLER_BUTTON_Y:
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        return true;
    default:
        return false;
    }
}

bool PadBindings::Rebind(PadAction pAction, int pButton)
{
    if (!IsBindable(pButton))
        return false;
    const int index = static_cast<int>(pAction);
    for (int i = 0; i < kActionCount; ++i)
    {
        if (i != index && mButtons[i] == pButton)
            mButtons[i] = mButtons[index];
    }
    mButtons[index] = pButton;
    return true;
}

const char* PadBindings::ActionName(PadAction pAction)
{
    static const char* const names[kActionCount] = {"PAD JUMP", "PAD FIRE", "PAD RECOVER"};
    return names[static_cast<int>(pAction)];
}

const char* PadBindings::ButtonName(int pButton)
{
    switch (pButton)
    {
    case SDL_CONTROLLER_BUTTON_A: return "A";
    case SDL_CONTROLLER_BUTTON_B: return "B";
    case SDL_CONTROLLER_BUTTON_X: return "X";
    case SDL_CONTROLLER_BUTTON_Y: return "Y";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LB";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RB";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "L STICK";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "R STICK";
    default: return "?";
    }
}

std::string PadBindings::Serialize() const
{
    std::string text;
    for (int i = 0; i < kActionCount; ++i)
    {
        if (i > 0)
            text += ' ';
        text += std::to_string(mButtons[i]);
    }
    return text;
}

bool PadBindings::Parse(const std::string& pText)
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
        mButtons[i] = parsed[i];
    return true;
}
