// The application icons, drawn by program: two discs (standard) and a paw (paw design).
#pragma once

#include <d2d1.h>

namespace ng {

// draws the icon in a square of `size` pixels at (0, 0)
void draw_icon(ID2D1Factory *factory, ID2D1RenderTarget *rt, float size, bool paw);

}  // namespace ng
