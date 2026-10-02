// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_KEY_BINDINGS_H
#define OPENHOVER_KEY_BINDINGS_H

#include <string>

enum class BindAction
{
    Accelerate,
    Brake,
    SteerLeft,
    SteerRight,
    Fire,
    Recover,
    Count
};

// One remappable keyboard key per action, stored as SDL scancodes. Fixed alternates (arrow keys,
// Shift, Up to jump) stay available regardless of these bindings so a bad remap never locks a
// player out of the controls.
class KeyBindings
{
public:
    KeyBindings();
    void ResetDefaults();
    int Key(BindAction pAction) const;
    // Binds the key; if another action used it, that action receives this action's old key.
    // Returns false for an invalid scancode or one reserved for menus and chat.
    bool Rebind(BindAction pAction, int pScancode);
    static bool IsBindable(int pScancode);
    static const char* ActionName(BindAction pAction);

    std::string Serialize() const;
    // Replaces the bindings only when the text is complete and valid; otherwise leaves them
    // unchanged and returns false.
    bool Parse(const std::string& pText);

private:
    int mKeys[static_cast<int>(BindAction::Count)];
};

#endif
