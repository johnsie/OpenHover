// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_PAD_MENU_INPUT_H
#define OPENHOVER_PAD_MENU_INPUT_H

enum class PadMenuContext
{
    Driving,    // a race is under way: only Start (pause) is a menu button
    Menu,       // front menus, pause menu and results: full navigation
    LobbyList   // lobby already handles D-pad, A and X itself: only Start and B go back
};

// Maps a gamepad button to the SDL keycode the keyboard menus already understand (arrows for the
// D-pad, Enter for A, Escape for Start and B). Returns 0 when the button is not a menu button in
// this context. B stays free for gameplay while driving.
int PadMenuKey(int pButton, PadMenuContext pContext);

#endif
