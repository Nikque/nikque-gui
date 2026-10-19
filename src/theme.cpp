#include "theme.h"

namespace ng {

namespace {

constexpr D2D1_COLOR_F kOutline = rgb(0x09090b);  // the thick black outline of the numbers

// SPEC-2 11.8: the same number colors in both designs
constexpr std::array<StageStyle, 8> kStages = {{
	{rgb(0xffffff), rgb(0xffffff), rgb(0xffffff), kOutline, rgb(0xffffff, 0), false},  // 0 white
	{rgb(0x53eafd), rgb(0x53eafd), rgb(0x53eafd), kOutline, rgb(0x53eafd, 0), false},  // 1 cyan-300
	{rgb(0x9de504), rgb(0x9de504), rgb(0x9de504), kOutline, rgb(0x9de504, 0), false},  // 2 lime-400
	// 3 rose quartz: pink-100 -> pink-300 -> pink-400
	{rgb(0xfce7f3), rgb(0xfda5d5), rgb(0xfb64b6), kOutline, rgb(0xfda5d5), true},
	// 4 amethyst: purple-200 -> purple-500 -> purple-600
	{rgb(0xe9d5fe), rgb(0xaa4eff), rgb(0x9810fa), kOutline, rgb(0xc27aff), true},
	// 5 ruby: rose-200 -> rose-500 -> rose-600
	{rgb(0xfecdd3), rgb(0xfe2657), rgb(0xec003f), kOutline, rgb(0xff637e), true},
	// 6 gold: yellow-100 -> amber-300 -> amber-400
	{rgb(0xfef9c2), rgb(0xffd230), rgb(0xf9bc06), kOutline, rgb(0xffd230), true},
	// 7 black diamond: white outline, white light
	{rgb(0x52525c), rgb(0x18181b), rgb(0x09090b), rgb(0xffffff), rgb(0xffffff), true},
}};

const Theme kStandard = [] {
	Theme t{};
	t.design = Design::Standard;
	t.bg = rgb(0x16181d);
	t.panel = rgb(0x1f232a);
	t.panel2 = rgb(0x262b33);
	t.text = rgb(0xe6e6e6);
	t.text_dim = rgb(0x9f9fa9);    // zinc-400
	t.line = rgb(0x2c313a);
	t.accent = rgb(0x00bcff);      // sky-400
	t.accent_text = rgb(0x09090b);
	t.board = rgb(0x0684c7);       // sky-600
	t.grid = rgb(0x024a70);        // sky-900
	t.frame = rgb(0x024a70);
	t.star = rgb(0x024a70);
	t.black_stone = rgb(0x101113);
	t.black_rim = rgb(0x52525c);   // a faint light on the edge
	t.white_stone = rgb(0xf1f1f1);
	t.white_rim = rgb(0x9f9fa9);   // a faint shade on the edge
	t.white_rim_px = 1.0f;
	t.last_on_black = rgb(0x53eafd);  // cyan-300
	t.last_on_white = rgb(0x0092b8);  // cyan-600
	t.best_frame = rgb(0xffffff, 0.95f);
	t.good_frame = rgb(0xffffff, 0.45f);
	t.select_frame = rgb(0x53eafd);
	t.effect_light = rgb(0xffffff);
	t.particle_edge = rgb(0xffffff, 0);
	t.paw_print = rgb(0, 0);
	t.frame_ratio = 0.06f;
	t.frame_round = 0.0f;
	t.tab_round = 0.18f;
	t.hearts = false;
	t.stage = kStages;
	return t;
}();

const Theme kPaw = [] {
	Theme t{};
	t.design = Design::Paw;
	t.bg = rgb(0xf7efe6);          // milk tea
	t.panel = rgb(0xefe2d4);       // beige
	t.panel2 = rgb(0xe8d6c4);
	t.text = rgb(0x4a3730);        // cocoa
	t.text_dim = rgb(0x71717b);    // zinc-500 (no brown)
	t.line = rgb(0xdcc7b4);
	t.accent = rgb(0xfb64b6);      // pink-400
	t.accent_text = rgb(0xffffff);
	t.board = rgb(0xfdf3d9);       // milk yellow
	t.grid = rgb(0xfb64b6);        // pink-400
	t.frame = rgb(0xfccee8);       // pink-200
	t.star = rgb(0xfb64b6);
	t.black_stone = rgb(0x2e2422);
	t.black_rim = rgb(0x52525c);   // zinc-600
	t.white_stone = rgb(0xfffaf6);
	t.white_rim = rgb(0x71717b);   // zinc-500: the white stone is about as light as the board
	t.white_rim_px = 1.5f;
	t.last_on_black = rgb(0xfffaf6);  // milk
	t.last_on_white = rgb(0xfb64b6);  // a milk dot would not show on a milk stone
	t.best_frame = rgb(0xec4899, 0.95f);  // pink-500
	t.good_frame = rgb(0xec4899, 0.45f);
	t.select_frame = rgb(0x8e51ff);       // violet-500
	t.effect_light = rgb(0xfb64b6);
	t.particle_edge = rgb(0xfb64b6);
	t.paw_print = rgb(0xfda5d5, 0.35f);   // pink-300
	t.frame_ratio = 0.22f;
	t.frame_round = 0.35f;
	t.tab_round = 0.5f;
	t.hearts = true;
	t.stage = kStages;
	return t;
}();

}  // namespace

const Theme &theme(Design d)
{
	return d == Design::Paw ? kPaw : kStandard;
}

}  // namespace ng
