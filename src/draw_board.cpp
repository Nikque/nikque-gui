// The board: frame, grid, stones, and the values of the moves with the colors and effects of the book's
// accuracy stage (SPEC-2 11.8). A square is drawn on its own (the effects redraw only their squares).
#include <d2d1_1helper.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

#include "app.h"
#include "lang.h"

namespace ng {

namespace {

constexpr float kTau = static_cast<float>(2 * std::numbers::pi);

float frac(double x) { return static_cast<float>(x - std::floor(x)); }
float wave(double t, double period, double phase = 0) { return std::sin(kTau * static_cast<float>(t / period + phase)); }

std::wstring value_text(int v) { return std::to_wstring(v); }

float value_em(const Layout &L, size_t len) { return std::round(L.cell * (len <= 2 ? 0.46f : 0.38f) * 2) / 2; }

struct GlowSpec {
	D2D1_COLOR_F color;
	float radius;
};

// the soft light (and from stage 5 the white light on the edge) of a stage
int glow_specs(const Theme &th, int stage, int cell, GlowSpec out[2])
{
	if (stage < 3) return 0;
	const float c = static_cast<float>(cell);
	const D2D1_COLOR_F white = rgb(0xffffff);
	switch (stage) {
	case 3: out[0] = {th.stage[3].glow, c * 0.055f}; return 1;
	case 4: out[0] = {th.stage[4].glow, c * 0.075f}; return 1;
	case 5: out[0] = {th.stage[5].glow, c * 0.08f}; out[1] = {white, c * 0.02f}; return 2;
	case 6: out[0] = {th.stage[6].glow, c * 0.09f}; out[1] = {white, c * 0.022f}; return 2;
	default: out[0] = {th.effect_light, c * 0.12f}; out[1] = {white, c * 0.025f}; return 2;
	}
}

// the four star points: on the crossings of the lines 2 and 6
D2D1_POINT_2F star_point(const Layout &L, int i)
{
	const int kx = (i & 1) ? 6 : 2, ky = (i & 2) ? 6 : 2;
	const float h = L.line / 2.0f;
	return {L.bx + L.frame + kx * (L.cell + L.line) + h, L.by + L.frame + ky * (L.cell + L.line) + h};
}

void draw_star_points(App &app, const Theme &th, const D2D1_RECT_F *limit)
{
	const Layout &L = app.lay_;
	auto *dc = app.gfx_.dc();
	const float r = th.design == Design::Paw ? L.cell * 0.13f : std::max(2.0f, L.cell * 0.06f);
	for (int i = 0; i < 4; ++i) {
		const D2D1_POINT_2F p = star_point(L, i);
		if (limit && (p.x + r < limit->left || p.x - r > limit->right || p.y + r < limit->top || p.y - r > limit->bottom))
			continue;
		if (th.design == Design::Paw) {
			dc->SetTransform(D2D1::Matrix3x2F::Scale(r * 2, r * 2) * D2D1::Matrix3x2F::Translation(p.x, p.y));
			dc->FillGeometry(app.gfx_.paw(), app.gfx_.brush(th.star));
			dc->SetTransform(D2D1::Matrix3x2F::Identity());
		} else {
			dc->FillEllipse(D2D1::Ellipse(p, r, r), app.gfx_.brush(th.star));
		}
	}
}

void fill_shape(ID2D1DeviceContext *dc, ID2D1Geometry *g, ID2D1Brush *b, const D2D1_MATRIX_3X2_F &m)
{
	dc->SetTransform(m);
	dc->FillGeometry(g, b);
}

void draw_value(App &app, const Theme &th, const D2D1_RECT_F &rc, const CellView &v, double t)
{
	Gfx &gx = app.gfx_;
	auto *dc = gx.dc();
	const Layout &L = app.lay_;
	const float c = static_cast<float>(L.cell);
	const int k = v.stage;
	const Effects eff = app.effects_;
	const bool fx = eff != Effects::Off && k >= 3;
	if (eff == Effects::Subdued) t = 0.8;
	const StageStyle &st = th.stage[k];
	const std::wstring text = value_text(v.value);
	const float em = value_em(L, text.size());
	const NumberShape &s = gx.shape(text, em);
	const float w = s.bounds.right - s.bounds.left, h = s.bounds.bottom - s.bounds.top;
	const float cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2 - (k == 0 && v.level ? c * 0.04f : 0);
	const D2D1_MATRIX_3X2_F I = D2D1::Matrix3x2F::Identity();

	// stage 7: light rays turning behind the number
	if (fx && k >= 7) {
		auto *b = gx.brush(th.effect_light);
		b->SetOpacity(0.2f + 0.06f * wave(t, 1.2));
		fill_shape(dc, gx.rays(), b,
		           D2D1::Matrix3x2F::Scale(c * 0.75f, c * 0.75f) *
		               D2D1::Matrix3x2F::Rotation(static_cast<float>(std::fmod(t * 22.0, 360.0))) *
		               D2D1::Matrix3x2F::Translation(cx, cy));
		dc->SetTransform(I);
	}

	float scale = 1;
	if (fx && k == 6) scale = 1 + 0.035f * wave(t, 1.6);
	if (fx && k == 7) scale = 1 + 0.05f * wave(t, 1.2);
	const D2D1_MATRIX_3X2_F M = D2D1::Matrix3x2F::Translation(-(s.bounds.left + w / 2), -(s.bounds.top + h / 2)) *
	                            D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(cx, cy);

	// the glow
	if (fx) {
		GlowSpec gs[2];
		const int n = glow_specs(th, k, L.cell, gs);
		const float base[8] = {0, 0, 0, 0.55f, 0.7f, 0.8f, 0.85f, 0.4f};
		const float amp[8] = {0, 0, 0, 0.3f, 0.3f, 0.2f, 0.15f, 0.25f};
		for (int i = 0; i < n; ++i) {
			const Glow *g = gx.glow(text, em, gs[i].color, gs[i].radius);
			if (!g) continue;
			float op = i == 0 ? base[k] + amp[k] * wave(t, 2.6 - 0.2 * k) : 0.75f;
			if (k == 7 && i == 0) {
				// a short flash every few seconds
				const float f = frac(t / 4.5);
				if (f < 0.08f) op += 0.6f * std::sin(f / 0.08f * static_cast<float>(std::numbers::pi));
			}
			const D2D1_SIZE_F bs = g->bitmap->GetSize();
			dc->SetTransform(M);
			dc->DrawBitmap(g->bitmap.Get(), D2D1::RectF(g->offset.x, g->offset.y, g->offset.x + bs.width, g->offset.y + bs.height),
			               std::clamp(op, 0.0f, 1.0f), D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
		}
	}

	// outline, then the fill over its inner half
	dc->SetTransform(M);
	dc->DrawGeometry(s.geom.Get(), gx.brush(st.edge), em * 0.17f, gx.round_join());
	if (st.gem) {
		const D2D1_GRADIENT_STOP stops[3] = {{0, st.top}, {0.5f, st.mid}, {1, st.bottom}};
		auto *g = gx.gradient(100 + k + (th.design == Design::Paw ? 10 : 0), stops, 3);
		g->SetStartPoint({0, s.bounds.top});
		g->SetEndPoint({0, s.bounds.bottom});
		dc->FillGeometry(s.geom.Get(), g);
	} else {
		dc->FillGeometry(s.geom.Get(), gx.brush(st.mid));
	}

	if (fx) {
		// a cut facet of light on the upper left
		const D2D1_GRADIENT_STOP gloss[4] = {
			{0, rgb(0xffffff, 0.55f)}, {0.47f, rgb(0xffffff, 0.2f)}, {0.5f, rgb(0xffffff, 0)}, {1, rgb(0xffffff, 0)}};
		// the black diamond keeps its black: less light on it
		const float shine = k == 7 ? 0.45f : 1.0f;
		auto *gl = gx.gradient(200, gloss, 4);
		gl->SetOpacity(shine);
		gl->SetStartPoint({s.bounds.left, s.bounds.top});
		gl->SetEndPoint({s.bounds.left + w * 0.55f, s.bounds.top + h * 0.75f});
		dc->FillGeometry(s.geom.Get(), gl);

		// bands of light crossing the number
		const D2D1_GRADIENT_STOP band[3] = {{0, rgb(0xffffff, 0)}, {0.5f, rgb(0xffffff, 0.8f)}, {1, rgb(0xffffff, 0)}};
		const D2D1_GRADIENT_STOP rainbow[8] = {
			{0, rgb(0xff6467, 0)},      {0.18f, rgb(0xff6467, 0.6f)}, {0.32f, rgb(0xffdf20, 0.6f)},
			{0.46f, rgb(0x7bf1a8, 0.6f)}, {0.6f, rgb(0x53eafd, 0.6f)}, {0.74f, rgb(0x8ec5ff, 0.6f)},
			{0.86f, rgb(0xf4a8ff, 0.6f)}, {1, rgb(0xf4a8ff, 0)}};
		const float period[8] = {0, 0, 0, 3.4f, 2.8f, 1.9f, 1.7f, 1.5f};
		const int bands = k == 3 ? 1 : 2;
		for (int i = 0; i < bands; ++i) {
			const bool rb = k == 6 && i == 1;
			auto *g = rb ? gx.gradient(202, rainbow, 8) : gx.gradient(201, band, 3);
			const float ph = frac(t / period[k] + i * 0.5);
			const float bx = s.bounds.left - w * 0.6f + ph * w * 2.2f, bw = w * (rb ? 0.4f : 0.22f);
			g->SetStartPoint({bx - bw, s.bounds.top});
			g->SetEndPoint({bx + bw, s.bounds.bottom});
			g->SetOpacity(shine);
			dc->FillGeometry(s.geom.Get(), g);
		}
	}
	dc->SetTransform(I);

	if (fx && k >= 4) {
		// small sparkles twinkling on the number
		const float pos[3][2] = {{0.18f, 0.22f}, {0.82f, 0.38f}, {0.45f, 0.86f}};
		for (int i = 0; i < 3; ++i) {
			float a = std::max(0.0f, wave(t, 1.4, i * 0.33));
			a = a * a * a;
			if (eff == Effects::Subdued) a = i == 0 ? 0.9f : 0;
			if (a < 0.02f) continue;
			const float x = cx + (pos[i][0] - 0.5f) * w * scale, y = cy + (pos[i][1] - 0.5f) * h * scale;
			const float sz = em * 0.32f * (0.6f + 0.4f * a);
			auto *b = gx.brush(rgb(0xffffff));
			b->SetOpacity(a);
			fill_shape(dc, gx.sparkle(), b, D2D1::Matrix3x2F::Scale(sz, sz) * D2D1::Matrix3x2F::Translation(x, y));
		}
		dc->SetTransform(I);
	}
	if (fx && k >= 5) {
		// stars (hearts in the paw design) around the number
		const float pos[5][2] = {{0.17f, 0.19f}, {0.84f, 0.2f}, {0.83f, 0.82f}, {0.18f, 0.8f}, {0.5f, 0.11f}};
		const int n = k == 5 ? 2 : k == 6 ? 3 : 5;
		const D2D1_COLOR_F col = k == 7 ? th.effect_light : st.top;
		for (int i = 0; i < n; ++i) {
			const float a = eff == Effects::Subdued ? 0.85f : 0.35f + 0.65f * std::abs(wave(t, 1.8, i * 0.27));
			const float sz = c * (th.hearts ? 0.15f : 0.14f) * (0.8f + 0.2f * a);
			const D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F::Scale(sz, sz) *
			                            D2D1::Matrix3x2F::Translation(rc.left + pos[i][0] * c, rc.top + pos[i][1] * c);
			auto *b = gx.brush(col);
			b->SetOpacity(a);
			fill_shape(dc, th.hearts ? static_cast<ID2D1Geometry *>(gx.heart()) : gx.star(), b, m);
			if (k == 7 || th.hearts) {
				dc->DrawGeometry(th.hearts ? static_cast<ID2D1Geometry *>(gx.heart()) : gx.star(),
				                 gx.brush(th.design == Design::Paw ? rgb(0xffffff, a) : rgb(0x09090b, a * 0.6f)), 0.08f);
			}
		}
		dc->SetTransform(I);
	}
	if (fx && k >= 6 && eff == Effects::On) {
		// small lights rising
		const D2D1_COLOR_F col = k == 7 ? th.effect_light : st.top;
		for (int i = 0; i < 6; ++i) {
			const float p = frac(t / 2.2 + i * 0.37);
			const float x = rc.left + c * (0.14f + 0.144f * i) + std::sin(static_cast<float>(t) * 2 + i) * c * 0.02f;
			const float y = rc.bottom - c * 0.08f - p * c * 0.8f;
			const float r = c * 0.022f;
			const float a = std::sin(p * static_cast<float>(std::numbers::pi));
			dc->FillEllipse(D2D1::Ellipse({x, y}, r, r), gx.brush(D2D1_COLOR_F{col.r, col.g, col.b, a}));
			if (th.particle_edge.a > 0)
				dc->DrawEllipse(D2D1::Ellipse({x, y}, r, r), gx.brush(D2D1_COLOR_F{th.particle_edge.r, th.particle_edge.g,
				                                                                   th.particle_edge.b, a}), 1.0f);
		}
	}
	if (fx && k >= 7 && eff == Effects::On) {
		// lights going round the number
		for (int i = 0; i < 6; ++i) {
			const float a = static_cast<float>(t * 1.4) + i * kTau / 6;
			const D2D1_POINT_2F p{cx + std::cos(a) * c * 0.36f, cy + std::sin(a) * c * 0.3f};
			const float r = c * 0.026f;
			dc->FillEllipse(D2D1::Ellipse(p, r, r), gx.brush(th.effect_light));
			if (th.particle_edge.a > 0) dc->DrawEllipse(D2D1::Ellipse(p, r, r), gx.brush(rgb(0xffffff)), 1.0f);
		}
	}

	// stage 0: the small level (or "end" for a search to the end) at the lower right
	if (k == 0 && (v.level > 0 || v.exact)) {
		const std::wstring lv = v.exact ? lang::t("cell.exact") : std::to_wstring(v.level);
		const float sem = std::round(c * 0.2f * 2) / 2;
		const NumberShape &ls = gx.shape(lv, sem, v.exact ? Face::Ui : Face::Number);
		const float lw = ls.bounds.right - ls.bounds.left;
		const D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F::Translation(rc.right - c * 0.07f - lw - ls.bounds.left,
		                                                          rc.bottom - c * 0.07f - ls.bounds.bottom);
		dc->SetTransform(m);
		dc->DrawGeometry(ls.geom.Get(), gx.brush(rgb(0x09090b)), sem * 0.2f, gx.round_join());
		dc->FillGeometry(ls.geom.Get(), gx.brush(rgb(0xe4e4e7)));
		dc->SetTransform(I);
	}
}

void draw_pending(App &app, const Theme &th, const D2D1_RECT_F &rc, double t)
{
	Gfx &gx = app.gfx_;
	auto *dc = gx.dc();
	const float c = static_cast<float>(app.lay_.cell);
	const float cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
	const bool anim = app.effects_ == Effects::On;
	for (int i = 0; i < 3; ++i) {
		const float a = anim ? 0.25f + 0.55f * std::max(0.0f, wave(t, 1.2, -i * 0.18)) : 0.5f;
		if (th.design == Design::Paw) {
			// small footprints
			const float sz = c * 0.13f;
			const float x = cx + (i - 1) * c * 0.17f, y = cy + ((i & 1) ? -c * 0.05f : c * 0.05f);
			auto *b = gx.brush(th.grid);
			b->SetOpacity(a);
			fill_shape(dc, gx.paw(), b,
			           D2D1::Matrix3x2F::Scale(sz, sz) * D2D1::Matrix3x2F::Rotation(90) * D2D1::Matrix3x2F::Translation(x, y));
			dc->SetTransform(D2D1::Matrix3x2F::Identity());
		} else {
			const float r = c * 0.035f;
			dc->FillEllipse(D2D1::Ellipse({cx + (i - 1) * c * 0.14f, cy}, r, r), gx.brush(rgb(0xffffff, a)));
		}
	}
}

}  // namespace

void App::prepare_glows()
{
	if (!gfx_.dc() || effects_ == Effects::Off || !show_eval_) return;
	const Theme &th = theme(design_);
	u64 m = game_.now().moves();
	while (m) {
		const int sq = std::countr_zero(m);
		m &= m - 1;
		const CellView v = cell_view(sq);
		if (!v.has_value || v.stage < 3) continue;
		GlowSpec gs[2];
		const int n = glow_specs(th, v.stage, lay_.cell, gs);
		const std::wstring text = value_text(v.value);
		for (int i = 0; i < n; ++i) gfx_.prepare_glow(text, value_em(lay_, text.size()), gs[i].color, gs[i].radius);
	}
}

void draw_cell(App &app, int sq, double t)
{
	const Theme &th = theme(app.design_);
	const Layout &L = app.lay_;
	Gfx &gx = app.gfx_;
	auto *dc = gx.dc();
	const D2D1_RECT_F rc = L.cell_rect(sq);
	const CellView v = app.cell_view(sq);
	const float c = static_cast<float>(L.cell);

	dc->FillRectangle(rc, gx.brush(th.board));
	if (th.design == Design::Paw && !v.stone && !v.has_value && !v.pending &&
	    (sq == 0 || sq == 7 || sq == 56 || sq == 63)) {
		// faint paw prints in the corner squares (never under a stone or a number)
		const float sz = c * 0.72f;
		const float rot = sq == 0 ? -25.0f : sq == 7 ? 25.0f : sq == 56 ? -155.0f : 155.0f;
		auto *b = gx.brush(th.paw_print);
		b->SetOpacity(th.paw_print.a);
		b->SetColor(D2D1_COLOR_F{th.paw_print.r, th.paw_print.g, th.paw_print.b, 1});
		dc->SetTransform(D2D1::Matrix3x2F::Scale(sz, sz) * D2D1::Matrix3x2F::Rotation(rot) *
		                 D2D1::Matrix3x2F::Translation((rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2));
		dc->FillGeometry(gx.paw(), b);
		dc->SetTransform(D2D1::Matrix3x2F::Identity());
	}
	draw_star_points(app, th, &rc);

	const D2D1_POINT_2F center{(rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2};
	if (v.stone) {
		const float r = c * 0.43f;
		const bool black = v.stone == 1;
		dc->FillEllipse(D2D1::Ellipse(center, r, r), gx.brush(black ? th.black_stone : th.white_stone));
		const float rw = black ? 1.0f : th.white_rim_px;
		dc->DrawEllipse(D2D1::Ellipse(center, r - rw / 2, r - rw / 2), gx.brush(black ? th.black_rim : th.white_rim), rw);
		if (v.last) {
			const float d = std::max(2.0f, c * 0.075f);
			dc->FillEllipse(D2D1::Ellipse(center, d, d), gx.brush(black ? th.last_on_black : th.last_on_white));
		}
		return;
	}
	if (v.best || v.good || v.selected) {
		const float s = app.lay_.scale;
		if (v.best || v.good) {
			const float in = c * 0.06f, wd = v.best ? 2 * s : 1 * s;
			dc->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(rc.left + in, rc.top + in, rc.right - in, rc.bottom - in),
			                                           c * 0.12f, c * 0.12f),
			                         gx.brush(v.best ? th.best_frame : th.good_frame), wd);
		}
		if (v.selected) {
			const float in = c * 0.025f;
			dc->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(rc.left + in, rc.top + in, rc.right - in, rc.bottom - in),
			                                           c * 0.14f, c * 0.14f),
			                         gx.brush(th.select_frame), 2.5f * s);
		}
	}
	if (v.has_value) draw_value(app, th, rc, v, t);
	else if (v.pending) draw_pending(app, th, rc, t);
}

void draw_board(App &app, const D2D1_RECT_F *)
{
	const Theme &th = theme(app.design_);
	const Layout &L = app.lay_;
	Gfx &gx = app.gfx_;
	auto *dc = gx.dc();
	const float x0 = static_cast<float>(L.bx), y0 = static_cast<float>(L.by), S = static_cast<float>(L.board_px);

	// the frame
	if (th.frame_round > 0) {
		const float r = L.cell * th.frame_round;
		dc->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x0, y0, x0 + S, y0 + S), r, r), gx.brush(th.frame));
	} else {
		dc->FillRectangle(D2D1::RectF(x0, y0, x0 + S, y0 + S), gx.brush(th.frame));
	}
	// the grid: the lines are what the squares leave
	const float in = static_cast<float>(L.frame);
	dc->FillRectangle(D2D1::RectF(x0 + in, y0 + in, x0 + S - in, y0 + S - in), gx.brush(th.grid));
	if (th.design == Design::Paw) {
		// small paws on the four corners of the frame
		const float sz = L.frame * 0.85f;
		for (int i = 0; i < 4; ++i) {
			const float px = (i & 1) ? x0 + S - L.frame / 2.0f : x0 + L.frame / 2.0f;
			const float py = (i & 2) ? y0 + S - L.frame / 2.0f : y0 + L.frame / 2.0f;
			dc->SetTransform(D2D1::Matrix3x2F::Scale(sz, sz) * D2D1::Matrix3x2F::Translation(px, py));
			dc->FillGeometry(gx.paw(), gx.brush(th.grid));
		}
		dc->SetTransform(D2D1::Matrix3x2F::Identity());
	}

	// coordinates above and on the left
	IDWriteTextFormat *f = gx.text(app.lay_.font_s, false, DWRITE_TEXT_ALIGNMENT_CENTER);
	const float coord = std::round(app.lay_.font * 1.2f);
	for (int i = 0; i < 8; ++i) {
		const D2D1_RECT_F col = L.cell_rect(i), row = L.cell_rect(i * 8);
		const wchar_t a[2] = {static_cast<wchar_t>(L'a' + i), 0}, n[2] = {static_cast<wchar_t>(L'1' + i), 0};
		gx.draw_text(a, f, D2D1::RectF(col.left, y0 - coord, col.right, y0), th.text_dim);
		gx.draw_text(n, f, D2D1::RectF(x0 - coord, row.top, x0, row.bottom), th.text_dim);
	}

	const double t = app.now_s();
	for (int sq = 0; sq < 64; ++sq) draw_cell(app, sq, t);
}

}  // namespace ng
