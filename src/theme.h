// Colors and shapes of the two designs: "standard" (default, dark) and "paw" (milk tea, pink, round).
// The colors come from the Tailwind CSS v4 palette (SPEC-2 11.7 and 11.8).
#pragma once

#include <d2d1_1.h>

#include <array>
#include <cstdint>

namespace ng {

enum class Design : int { Standard = 0, Paw = 1 };

constexpr D2D1_COLOR_F rgb(std::uint32_t hex, float a = 1.0f)
{
	return {((hex >> 16) & 0xff) / 255.0f, ((hex >> 8) & 0xff) / 255.0f, (hex & 0xff) / 255.0f, a};
}

// the colors of a number by stage: a flat color for 0..2, a gem (top, middle, bottom) from 3
struct StageStyle {
	D2D1_COLOR_F top, mid, bottom;
	D2D1_COLOR_F edge;     // the outline of the number
	D2D1_COLOR_F glow;     // the soft light around (stages 3..)
	bool gem;
};

struct Theme {
	Design design;
	D2D1_COLOR_F bg, panel, panel2, text, text_dim, line, accent, accent_text;
	D2D1_COLOR_F board, grid, frame, star;
	D2D1_COLOR_F black_stone, black_rim, white_stone, white_rim;
	float white_rim_px;   // width of the white stone's outline (paw: zinc-500 1.5 px)
	D2D1_COLOR_F last_on_black, last_on_white;
	D2D1_COLOR_F best_frame, good_frame, select_frame;
	D2D1_COLOR_F effect_light;  // light of the rays and particles (stage 7 white, paw pink)
	D2D1_COLOR_F particle_edge; // paw: pink edge on the particles
	D2D1_COLOR_F paw_print;     // paw: the faint prints in the corner squares
	float frame_ratio;    // frame width / cell
	float frame_round;    // corner radius / cell (0: square corners)
	float tab_round;      // corner radius of tabs / row height
	bool hearts;          // the stars of the effects are hearts
	std::array<StageStyle, 8> stage;
};

const Theme &theme(Design d);

}  // namespace ng
