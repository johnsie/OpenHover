// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_PAD_BINDINGS_H
#define OPENHOVER_PAD_BINDINGS_H

#include <string>

enum class PadAction
{
    Jump,
    Fire,
    Recover,
    Count
};

// One remappable gamepad button per action, stored as SDL_GameControllerButton values. Steering
// and throttle stay on the stick and triggers; menu navigation buttons are never bindable.
class PadBindings
{
public:
    PadBindings();
    void ResetDefaults();
    int Button(PadAction pAction) const;
    // A button already used by another action is swapped onto this action's old button.
    bool Rebind(PadAction pAction, int pButton);
    static bool IsBindable(int pButton);
    static const char* ActionName(PadAction pAction);
    static const char* ButtonName(int pButton);

    std::string Serialize() const;
    bool Parse(const std::string& pText);

private:
    int mButtons[static_cast<int>(PadAction::Count)];
};

#endif
