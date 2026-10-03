// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_PIXEL_FONT_H
#define OPENHOVER_PIXEL_FONT_H

#include <string>

// OpenHover's built-in 5x7 pixel font, drawn as quads with OpenGL immediate mode in the current
// colour. (0, 0) is the top left of an orthographic screen projection; pScale is the size of one
// font pixel. Only letters, digits, space, '_', '-' and '?' have glyphs; the font is
// case-insensitive in effect (lower-case letters have their own shapes).
void DrawPixelText(const char* pText, int pLeft, int pTop, int pScale);

// Width in screen pixels that DrawPixelText would use for pText at pScale.
int PixelTextWidth(const std::string& pText, int pScale);

#endif
