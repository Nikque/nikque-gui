// Colors and shapes of the two designs: "standard" (default, dark) and "paw" (milk tea, pink, round).
// The colors come from the Tailwind CSS v4 palette (SPEC-2 11.7 and 11.8).
#pragma once

#include "gfx_types.h"

#include <array>
#include <cstdint>

namespace ng {

enum class Design : int { Standard = 0, Paw = 1 };

constexpr Color rgb(std::uint32_t hex, float a = 1.0f)
{
	return {((hex >> 16) & 0xff) / 255.0f, ((hex >> 8) & 0xff) / 255.0f, (hex & 0xff) / 255.0f, a};
}

// the same color with another opacity
constexpr Color with_alpha(const Color &c, float a) { return {c.r, c.g, c.b, a}; }

// the colors of a number by stage: a flat color for 0..2, a gem (top, middle, bottom) from 3
struct StageStyle {
	Color top, mid, bottom;
	Color edge;     // the outline of the number
	Color glow;     // the soft light around (stages 3..)
	bool gem;
};

struct Theme {
	Design design;
	Color bg, panel, panel2, text, text_dim, line, accent, accent_text;
	Color board, grid, frame, star;
	Color black_stone, black_rim, white_stone, white_rim;
	float white_rim_px;   // width of the white stone's outline (paw: zinc-500 1.5 px)
	Color last_on_black, last_on_white;
	Color best_frame, good_frame, select_frame;
	Color effect_light;  // light of the rays and particles (stage 7 white, paw pink)
	Color particle_edge; // paw: pink edge on the particles
	Color paw_print;     // paw: the faint prints in the corner squares
	float frame_ratio;    // frame width / cell
	float frame_round;    // corner radius / cell (0: square corners)
	float tab_round;      // corner radius of tabs / row height
	bool hearts;          // the stars of the effects are hearts
	std::array<StageStyle, 8> stage;
	Color exact;   // the values of a search to the end (not in the book)
	Color opening_mark, opening_edge;  // the marks of the moves on the lines of the openings
};

const Theme &theme(Design d);
// the light of the bands crossing a number of a stage (3..)
Color stage_band(const Theme &th, int stage);

}  // namespace ng
