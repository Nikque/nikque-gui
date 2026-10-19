// The information bar, the right panel (tabs, range row, the table of moves, the book and record tabs, the
// graph of values) and the log row.
#include <d2d1_1helper.h>

#include <algorithm>
#include <cmath>
#include <optional>

#include "app.h"
#include "lang.h"

namespace ng {

namespace {

// "x/y" of stage k (3..), from the ranges asked
std::wstring stage_range(const App &app, int k)
{
	std::vector<std::string> parts;
	size_t i = 0;
	while (i <= app.stages_.size()) {
		size_t e = app.stages_.find(',', i);
		if (e == std::string::npos) e = app.stages_.size();
		parts.push_back(app.stages_.substr(i, e - i));
		i = e + 1;
	}
	const size_t idx = static_cast<size_t>(k - 3);
	return idx < parts.size() ? widen(parts[idx]) : L"?";
}

std::wstring stage_meaning(const App &app, int k)
{
	if (k <= 2) return lang::t("stage." + std::to_string(k));
	std::wstring r = stage_range(app, k);
	for (auto &c : r)
		if (c == L'/') c = L' ';
	return lang::f("stage.n", {std::to_wstring(k), widen(app.base_), r});
}

// a readable color of a stage on the panel
D2D1_COLOR_F stage_ink(const Theme &th, int k)
{
	if (k <= 0) return th.text;
	if (k == 7) return th.design == Design::Paw ? rgb(0x18181b) : rgb(0xffffff);
	return th.stage[k].mid;
}

void stage_dot(Gfx &gx, const Theme &th, int k, D2D1_POINT_2F p, float r)
{
	auto *dc = gx.dc();
	const StageStyle &st = th.stage[std::clamp(k, 0, 7)];
	if (k >= 3) {
		const D2D1_GRADIENT_STOP stops[3] = {{0, st.top}, {0.5f, st.mid}, {1, st.bottom}};
		auto *g = gx.gradient(300 + k, stops, 3);
		g->SetStartPoint({p.x, p.y - r});
		g->SetEndPoint({p.x, p.y + r});
		dc->FillEllipse(D2D1::Ellipse(p, r, r), g);
	} else {
		dc->FillEllipse(D2D1::Ellipse(p, r, r), gx.brush(st.mid));
	}
	dc->DrawEllipse(D2D1::Ellipse(p, r, r), gx.brush(k == 7 ? rgb(0xffffff) : rgb(0x09090b, 0.8f)), 1.0f);
}

std::wstring group(long long n)
{
	std::wstring s = std::to_wstring(n);
	for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), L",");
	return s;
}

// the best value of the position after i moves, from black
std::optional<int> black_value(const App &app, int i)
{
	const Position &p = app.game_.at(i);
	if (p.game_over()) {
		const int b = std::popcount(p.black), w = std::popcount(p.white);
		const int e = 64 - b - w;
		return b > w ? b - w + e : b < w ? b - w - e : 0;
	}
	const PosInfo *info = app.cache_.peek(p);
	if (!info) return std::nullopt;
	int best = -999;
	for (const auto &m : info->moves) {
		int v;
		if (shown_value(m, app.use_book_, &v)) best = std::max(best, v);
	}
	if (best == -999) return std::nullopt;
	return p.side == kBlack ? best : -best;
}

// the stage of the i-th move (the move from the position after i moves), -1 unknown
int move_stage(const App &app, int i)
{
	if (i < 0 || i >= app.game_.size()) return -1;
	const PosInfo *info = app.cache_.peek(app.game_.at(i));
	if (!info) return -1;
	const MoveEval *m = info->find(app.game_.moves()[i]);
	if (!m || !m->book_row) return -1;
	return m->stage;
}

void rounded(Gfx &gx, const D2D1_RECT_F &r, float rad, const D2D1_COLOR_F &fill)
{
	gx.dc()->FillRoundedRectangle(D2D1::RoundedRect(r, rad, rad), gx.brush(fill));
}

void draw_tabs(App &app, const Theme &th)
{
	Gfx &gx = app.gfx_;
	const Layout &L = app.lay_;
	IDWriteTextFormat *f = gx.text(L.font, false, DWRITE_TEXT_ALIGNMENT_CENTER);
	const char *keys[] = {"tab.moves", "tab.book", "tab.record"};
	app.tab_rects_.clear();
	float x = L.tabs.left;
	const float h = L.tabs.bottom - L.tabs.top;
	for (int i = 0; i < 3; ++i) {
		const std::wstring &s = lang::t(keys[i]);
		const float w = gx.text_width(s, f) + L.font * 1.6f;
		const D2D1_RECT_F r{x, L.tabs.top, x + w, L.tabs.bottom};
		const bool on = static_cast<int>(app.tab_) == i;
		rounded(gx, r, h * th.tab_round, on ? th.accent : th.panel2);
		gx.draw_text(s, f, r, on ? th.accent_text : th.text);
		app.tab_rects_.emplace_back(i, r);
		x += w + 4 * L.scale;
	}
}

void draw_field(App &app, const Theme &th, int which, const Layout::Field &fd, int value, int hi)
{
	Gfx &gx = app.gfx_;
	auto *dc = gx.dc();
	const Layout &L = app.lay_;
	const float h = fd.box.bottom - fd.box.top;
	const bool editing = app.edit_field_ == which;
	rounded(gx, fd.box, h * th.tab_round * 0.6f, th.panel2);
	dc->DrawRoundedRectangle(D2D1::RoundedRect(fd.box, h * th.tab_round * 0.6f, h * th.tab_round * 0.6f),
	                         gx.brush(editing ? th.accent : th.line), editing ? 2.0f * L.scale : 1.0f);
	IDWriteTextFormat *f = gx.text(L.font, false, DWRITE_TEXT_ALIGNMENT_CENTER, Face::Number);
	gx.draw_text(editing ? app.edit_text_ + L"|" : std::to_wstring(value), f, fd.box, th.text);
	if (fd.track.right - fd.track.left < 4) return;
	const float cy = (fd.track.top + fd.track.bottom) / 2;
	const float th_h = std::max(3.0f, 4 * L.scale);
	rounded(gx, D2D1::RectF(fd.track.left, cy - th_h / 2, fd.track.right, cy + th_h / 2), th_h / 2, th.line);
	const float x = fd.track.left + (fd.track.right - fd.track.left) * value / static_cast<float>(hi);
	rounded(gx, D2D1::RectF(fd.track.left, cy - th_h / 2, x, cy + th_h / 2), th_h / 2, th.accent);
	const float r = std::round(7 * L.scale);
	dc->FillEllipse(D2D1::Ellipse({x, cy}, r, r), gx.brush(th.accent));
	dc->DrawEllipse(D2D1::Ellipse({x, cy}, r, r), gx.brush(th.panel), 2.0f * L.scale);
}

void draw_range(App &app, const Theme &th)
{
	Gfx &gx = app.gfx_;
	const Layout &L = app.lay_;
	IDWriteTextFormat *f = gx.text(L.font, false);
	gx.draw_text(lang::t("range.title"), f, D2D1::RectF(L.range.left, L.range.top, L.fx.box.left, L.range.bottom), th.text_dim);
	const float gap = std::round(8 * L.scale);
	const float lx = gx.text_width(lang::t("range.x"), f), ly = gx.text_width(lang::t("range.y"), f);
	gx.draw_text(lang::t("range.x"), f, D2D1::RectF(L.fx.box.left - lx - gap * 0.5f, L.range.top, L.fx.box.left, L.range.bottom), th.text);
	gx.draw_text(lang::t("range.y"), f, D2D1::RectF(L.fy.box.left - ly - gap * 0.5f, L.range.top, L.fy.box.left, L.range.bottom), th.text);
	draw_field(app, th, 1, L.fx, app.loss_x_, 64);
	draw_field(app, th, 2, L.fy, app.loss_y_, 128);
}

void draw_moves_table(App &app, const Theme &th)
{
	Gfx &gx = app.gfx_;
	auto *dc = gx.dc();
	const Layout &L = app.lay_;
	const D2D1_RECT_F T = L.table;
	const float em = L.font;
	struct Col {
		Column c;
		const char *key;
		float w;
	};
	std::vector<Col> cols = {{Column::Move, "col.move", 2.4f * em},       {Column::Value, "col.value", 3.6f * em},
	                         {Column::Stage, "col.stage", 6.4f * em},     {Column::Umi, "col.umi", 5.4f * em},
	                         {Column::Missing, "col.missing", 4.4f * em}, {Column::Positions, "col.positions", 6.6f * em},
	                         {Column::Level, "col.level", 2.6f * em}};
	if (app.umi_ == UmiMode::Off) std::erase_if(cols, [](const Col &c) { return c.c == Column::Umi; });
	const float W = T.right - T.left;
	auto total = [&] {
		float s = 0;
		for (auto &c : cols) s += c.w;
		return s;
	};
	// narrow: leave out the columns of less use first
	for (Column drop : {Column::Level, Column::Positions, Column::Missing, Column::Umi}) {
		if (total() <= W) break;
		std::erase_if(cols, [&](const Col &c) { return c.c == drop; });
	}
	const float extra = std::max(0.0f, (W - total()) / cols.size());
	app.col_rects_.clear();
	float x = T.left;
	const float rh = std::round(L.row * 0.92f);
	rounded(gx, D2D1::RectF(T.left, T.top, T.right, T.top + rh), 4 * L.scale, th.panel2);
	IDWriteTextFormat *hf = gx.text(L.font_s, true, DWRITE_TEXT_ALIGNMENT_CENTER);
	for (auto &c : cols) {
		const D2D1_RECT_F r{x, T.top, x + c.w + extra, T.top + rh};
		std::wstring s = lang::t(c.key);
		if (app.sort_col_ == c.c) s += app.sort_desc_ ? L" ▼" : L" ▲";
		gx.draw_text(s, hf, r, th.text_dim);
		app.col_rects_.emplace_back(c.c, r);
		x = r.right;
	}

	app.row_rects_.clear();
	const Position &p = app.game_.now();
	const PosInfo *info = app.cache_.peek(p);
	if (!info || info->moves.empty()) {
		IDWriteTextFormat *f = gx.text(L.font, false, DWRITE_TEXT_ALIGNMENT_CENTER);
		gx.draw_text(p.game_over() ? lang::t("table.game_over") : lang::t("table.wait"), f,
		             D2D1::RectF(T.left, T.top + rh, T.right, T.top + rh * 3), th.text_dim);
		return;
	}
	const std::vector<const MoveEval *> rows = app.sorted_moves(*info);
	const int fit = std::max(1, static_cast<int>((T.bottom - T.top - rh) / rh));
	app.table_scroll_ = std::clamp(app.table_scroll_, 0, std::max(0, static_cast<int>(rows.size()) - fit));
	IDWriteTextFormat *f = gx.text(L.font, false, DWRITE_TEXT_ALIGNMENT_CENTER, Face::Number);
	IDWriteTextFormat *fs = gx.text(L.font_s, false, DWRITE_TEXT_ALIGNMENT_CENTER);
	float y = T.top + rh;
	dc->PushAxisAlignedClip(D2D1::RectF(T.left, T.top + rh, T.right, T.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
	for (int i = app.table_scroll_; i < static_cast<int>(rows.size()) && y < T.bottom; ++i, y += rh) {
		const MoveEval &m = *rows[i];
		const D2D1_RECT_F row{T.left, y, T.right, y + rh};
		app.row_rects_.emplace_back(m.sq, row);
		if (m.sq == app.selected_sq_) rounded(gx, row, 4 * L.scale, D2D1_COLOR_F{th.accent.r, th.accent.g, th.accent.b, 0.25f});
		else if (m.sq == app.hover_sq_) rounded(gx, row, 4 * L.scale, D2D1_COLOR_F{th.text.r, th.text.g, th.text.b, 0.07f});
		int v = 0;
		const bool hv = shown_value(m, app.use_book_, &v);
		const int stage = (app.use_book_ && m.book_row) ? m.stage : -1;
		for (auto &[c, hr] : app.col_rects_) {
			const D2D1_RECT_F r{hr.left, y, hr.right, y + rh};
			switch (c) {
			case Column::Move:
				gx.draw_text(widen(square_name(m.sq)), f, r, th.text);
				break;
			case Column::Value:
				if (hv) {
					std::wstring s = (v > 0 ? L"+" : L"") + std::to_wstring(v);
					if (!(app.use_book_ && m.in_book) && m.searched && m.bound) s.insert(s.begin(), static_cast<wchar_t>(m.bound));
					gx.draw_text(s, f, r, th.text);
				} else {
					gx.draw_text(L"…", f, r, th.text_dim);
				}
				break;
			case Column::Stage:
				if (stage >= 0) {
					const float rr = std::round(L.font_s * 0.42f);
					stage_dot(gx, th, stage, {r.left + rr + 6 * L.scale, (r.top + r.bottom) / 2}, rr);
					std::wstring s = std::to_wstring(stage);
					if (stage >= 3) s += L" " + lang::f("table.done", {stage_range(app, stage)});
					if (m.has_link) s += L" ※";
					gx.draw_text(s, fs, D2D1::RectF(r.left + 2 * rr + 8 * L.scale, r.top, r.right, r.bottom), th.text);
				} else if (app.use_book_ && app.eng_[0].alive && !info->book_done) {
					gx.draw_text(L"…", f, r, th.text_dim);
				}
				break;
			case Column::Missing:
				if (stage >= 1 && m.missing >= 0) gx.draw_text(group(m.missing), f, r, th.text);
				break;
			case Column::Positions:
				if (m.positions >= 0) gx.draw_text(group(m.positions), f, r, th.text);
				else if (m.lines >= 0) gx.draw_text(L"≈" + group(m.lines), f, r, th.text_dim);
				break;
			case Column::Level: {
				const int lv = (app.use_book_ && m.in_book) ? m.book_level : shown_level(m, app.use_book_);
				if (lv > 0) gx.draw_text(std::to_wstring(lv), f, r, th.text);
				break;
			}
			case Column::Umi:
				if (m.umi_known) {
					std::wstring s = m.umi_none ? lang::t("table.umi_none")
					                            : std::to_wstring(m.umi_black) + L":" + std::to_wstring(m.umi_white);
					if (!m.umi_exact) s = L"≦" + s;
					gx.draw_text(s, f, r, th.text);
				}
				break;
			default:
				break;
			}
		}
	}
	dc->PopAxisAlignedClip();
}

void draw_book_tab(App &app, const Theme &th)
{
	Gfx &gx = app.gfx_;
	const Layout &L = app.lay_;
	const D2D1_RECT_F T = L.table;
	IDWriteTextFormat *f = gx.text(L.font, false);
	IDWriteTextFormat *fn = gx.text(L.font, false, DWRITE_TEXT_ALIGNMENT_TRAILING, Face::Number);
	const PosInfo *info = app.cache_.peek(app.game_.now());
	float y = T.top;
	const float rh = L.row;
	auto line = [&](const std::wstring &s, const D2D1_COLOR_F &c) {
		gx.draw_text(s, f, D2D1::RectF(T.left + 4, y, T.right, y + rh), c);
		y += rh;
	};
	line(lang::f("book.engine", {app.engine_name()}), th.text_dim);
	if (!app.use_book_) {
		line(lang::t("book.off"), th.text);
		return;
	}
	if (!info || !info->book_begun) {
		line(lang::t("book.wait"), th.text_dim);
		return;
	}
	line(lang::t(info->in_book ? "book.in" : "book.out"), th.text);
	line(lang::t("book.levels"), th.text_dim);
	if (!info->levels_done) {
		line(lang::t("book.counting"), th.text_dim);
		return;
	}
	const float mid = T.left + (T.right - T.left) * 0.45f;
	for (const auto &[lv, n] : info->levels) {
		if (y + rh > T.bottom) break;
		gx.draw_text(L"Lv " + std::to_wstring(lv), f, D2D1::RectF(T.left + 16 * L.scale, y, mid, y + rh), th.text);
		gx.draw_text(group(n), fn, D2D1::RectF(mid, y, T.right - 16 * L.scale, y + rh), th.text);
		y += rh;
	}
	if (y + rh <= T.bottom) {
		gx.draw_text(lang::t("book.total"), f, D2D1::RectF(T.left + 16 * L.scale, y, mid, y + rh), th.text_dim);
		gx.draw_text(group(info->levels_positions), fn, D2D1::RectF(mid, y, T.right - 16 * L.scale, y + rh), th.text);
	}
}

void draw_record_tab(App &app, const Theme &th)
{
	Gfx &gx = app.gfx_;
	const Layout &L = app.lay_;
	const D2D1_RECT_F T = L.table;
	IDWriteTextFormat *fn = gx.text(L.font_s, false, DWRITE_TEXT_ALIGNMENT_TRAILING, Face::Number);
	IDWriteTextFormat *fm = gx.text(L.font, false, DWRITE_TEXT_ALIGNMENT_LEADING, Face::Number);
	app.record_rects_.clear();
	const float cw = std::round(L.font * 4.2f), rh = L.row;
	const int per_row = std::max(1, static_cast<int>((T.right - T.left) / cw));
	int shown = 0;
	for (int i = 0; i < app.game_.size(); ++i) {
		const int mv = app.game_.moves()[i];
		const int col = shown % per_row, row = shown / per_row;
		const D2D1_RECT_F r{T.left + col * cw, T.top + row * rh, T.left + (col + 1) * cw, T.top + (row + 1) * rh};
		if (r.bottom > T.bottom) break;
		++shown;
		const bool here = i + 1 == app.game_.cursor() || (mv == kPass && i + 2 == app.game_.cursor());
		if (here) rounded(gx, r, 4 * L.scale, D2D1_COLOR_F{th.accent.r, th.accent.g, th.accent.b, 0.3f});
		const bool after = i >= app.game_.cursor();
		gx.draw_text(std::to_wstring(i + 1), fn, D2D1::RectF(r.left, r.top, r.left + cw * 0.38f, r.bottom), th.text_dim);
		gx.draw_text(mv == kPass ? lang::t("record.pass") : widen(square_name(mv)), fm,
		             D2D1::RectF(r.left + cw * 0.45f, r.top, r.right, r.bottom), after ? th.text_dim : th.text);
		app.record_rects_.emplace_back(i + 1, r);
	}
	if (app.game_.size() == 0)
		gx.draw_text(lang::t("record.empty"), gx.text(L.font, false), D2D1::RectF(T.left + 4, T.top, T.right, T.top + rh), th.text_dim);
}

void draw_graph(App &app, const Theme &th)
{
	Gfx &gx = app.gfx_;
	auto *dc = gx.dc();
	const Layout &L = app.lay_;
	const D2D1_RECT_F G = L.graph;
	rounded(gx, G, 6 * L.scale, th.panel2);
	const float lm = std::round(L.font_s * 2.6f), bm = std::round(L.font_s * 1.6f), m = 6 * L.scale;
	const D2D1_RECT_F P{G.left + lm, G.top + m, G.right - m, G.bottom - bm};
	app.graph_plot_ = P;
	if (P.right - P.left < 20 || P.bottom - P.top < 20) return;

	const int n = app.game_.size();
	std::vector<std::optional<int>> vals(n + 1);
	int range = 8;
	for (int i = 0; i <= n; ++i) {
		vals[i] = black_value(app, i);
		if (vals[i]) range = std::max(range, std::abs(*vals[i]));
	}
	range = std::min(64, (range + 7) / 8 * 8);
	auto X = [&](int i) { return P.left + (P.right - P.left) * app.game_.at(i).ply() / 60.0f; };
	auto Y = [&](int v) { return (P.top + P.bottom) / 2 - (P.bottom - P.top) / 2 * v / static_cast<float>(range); };

	IDWriteTextFormat *fl = gx.text(L.font_s, false, DWRITE_TEXT_ALIGNMENT_TRAILING, Face::Number);
	IDWriteTextFormat *fc = gx.text(L.font_s, false, DWRITE_TEXT_ALIGNMENT_CENTER, Face::Number);
	for (int v : {-range, -range / 2, 0, range / 2, range}) {
		dc->DrawLine({P.left, Y(v)}, {P.right, Y(v)}, gx.brush(v == 0 ? th.text_dim : th.line), v == 0 ? 1.5f : 1.0f);
		gx.draw_text((v > 0 ? L"+" : L"") + std::to_wstring(v), fl, D2D1::RectF(G.left, Y(v) - L.font_s, P.left - 4, Y(v) + L.font_s),
		             th.text_dim, false);
	}
	for (int k = 0; k <= 60; k += 10) {
		const float x = P.left + (P.right - P.left) * k / 60.0f;
		dc->DrawLine({x, P.top}, {x, P.bottom}, gx.brush(th.line), 1.0f);
		gx.draw_text(std::to_wstring(k), fc, D2D1::RectF(x - 20, P.bottom, x + 20, G.bottom), th.text_dim, false);
	}
	// the place shown
	const float cx = X(app.game_.cursor());
	dc->DrawLine({cx, P.top}, {cx, P.bottom}, gx.brush(th.accent), 2.0f * L.scale);
	// the line and the points (colored by the stage of the move that led there)
	int prev = -1;
	for (int i = 0; i <= n; ++i) {
		if (!vals[i]) continue;
		if (prev >= 0) dc->DrawLine({X(prev), Y(*vals[prev])}, {X(i), Y(*vals[i])}, gx.brush(th.text_dim), 1.5f * L.scale);
		prev = i;
	}
	const float r = std::max(2.5f, 3.2f * L.scale);
	for (int i = 0; i <= n; ++i) {
		if (!vals[i]) continue;
		const int k = move_stage(app, i - 1);
		if (k < 0) {
			dc->FillEllipse(D2D1::Ellipse({X(i), Y(*vals[i])}, r * 0.8f, r * 0.8f), gx.brush(th.text_dim));
		} else {
			stage_dot(gx, th, k, {X(i), Y(*vals[i])}, r);
		}
	}
}

}  // namespace

void draw_panel(App &app)
{
	if (app.panel_hidden_) return;
	const Theme &th = theme(app.design_);
	Gfx &gx = app.gfx_;
	const Layout &L = app.lay_;
	gx.dc()->FillRectangle(L.panel, gx.brush(th.panel));
	draw_tabs(app, th);
	draw_range(app, th);
	switch (app.tab_) {
	case Tab::Moves: draw_moves_table(app, th); break;
	case Tab::Book: draw_book_tab(app, th); break;
	default: draw_record_tab(app, th); break;
	}
	const float cy = (L.splitter.top + L.splitter.bottom) / 2;
	gx.dc()->DrawLine({L.splitter.left + 40 * L.scale, cy}, {L.splitter.right - 40 * L.scale, cy}, gx.brush(th.line),
	                  2.0f * L.scale);
	draw_graph(app, th);
}

void draw_bar(App &app)
{
	const Theme &th = theme(app.design_);
	Gfx &gx = app.gfx_;
	auto *dc = gx.dc();
	const Layout &L = app.lay_;
	dc->FillRectangle(L.bar, gx.brush(th.panel));
	dc->DrawLine({L.bar.left, L.bar.bottom - 0.5f}, {L.bar.right, L.bar.bottom - 0.5f}, gx.brush(th.line), 1.0f);
	IDWriteTextFormat *f = gx.text(L.font, false);
	const Position &p = app.game_.now();
	const int b = std::popcount(p.black), w = std::popcount(p.white);
	std::vector<std::wstring> parts;
	if (p.game_over()) parts.push_back(lang::t("bar.game_over") + L"  " + lang::f("bar.discs", {std::to_wstring(b), std::to_wstring(w)}));
	else
		parts.push_back(lang::f("bar.ply", {std::to_wstring(p.ply() + 1)}) + L" " +
		                lang::t(p.side == kBlack ? "bar.black" : "bar.white") + L"  " +
		                lang::f("bar.discs", {std::to_wstring(b), std::to_wstring(w)}));
	std::wstring eng = app.engine_name() + L" Lv" + std::to_wstring(app.level_);
	if (!app.eng_[0].alive && !app.eng_[1].alive) eng += L"  " + lang::t("bar.no_engine");
	else if (app.use_book_ && app.eng_[0].alive && !app.eng_[0].loaded) eng += L"  " + lang::t("bar.loading");
	if (!app.use_book_) eng += L"  " + lang::t("bar.book_off");
	parts.push_back(eng);
	float x = L.bar.left + 10 * L.scale;
	for (size_t i = 0; i < parts.size(); ++i) {
		const float tw = gx.text_width(parts[i], f);
		gx.draw_text(parts[i], f, D2D1::RectF(x, L.bar.top, x + tw + 2, L.bar.bottom), th.text, false);
		x += tw + 12 * L.scale;
		dc->DrawLine({x, L.bar.top + L.row * 0.25f}, {x, L.bar.bottom - L.row * 0.25f}, gx.brush(th.line), 1.0f);
		x += 12 * L.scale;
	}
	// the legend of the book's accuracy: weak -> strong
	const std::wstring &title = lang::t("bar.accuracy"), &weak = lang::t("bar.weak"), &strong = lang::t("bar.strong");
	const float r = std::round(L.font * 0.36f), step = r * 2.6f;
	const float need = gx.text_width(title, f) + gx.text_width(weak, f) + gx.text_width(strong, f) + 8 * step + 40 * L.scale;
	if (x + need <= L.bar.right) {
		float lx = L.bar.right - need;
		const float cy = (L.bar.top + L.bar.bottom) / 2;
		const float tw = gx.text_width(title, f);
		gx.draw_text(title, f, D2D1::RectF(lx, L.bar.top, lx + tw + 2, L.bar.bottom), th.text_dim, false);
		lx += tw + 10 * L.scale;
		const float ww = gx.text_width(weak, f);
		gx.draw_text(weak, f, D2D1::RectF(lx, L.bar.top, lx + ww + 2, L.bar.bottom), th.text_dim, false);
		lx += ww + 6 * L.scale;
		for (int k = 0; k < 8; ++k) {
			const D2D1_POINT_2F c{lx + r + k * step, cy};
			stage_dot(gx, th, k, c, r);
			app.legend_rects_[k] = {c.x - step / 2, L.bar.top, c.x + step / 2, L.bar.bottom};
		}
		lx += 8 * step + 2 * L.scale;
		gx.draw_text(strong, f, D2D1::RectF(lx, L.bar.top, lx + gx.text_width(strong, f) + 2, L.bar.bottom), th.text_dim, false);
	} else {
		for (auto &lr : app.legend_rects_) lr = {};
	}
	// the meaning of a stage under the mouse
	if (app.hover_stage_ >= 0) {
		const std::wstring s = stage_meaning(app, app.hover_stage_);
		const float tw = gx.text_width(s, f) + 20 * L.scale;
		const D2D1_RECT_F &lr = app.legend_rects_[app.hover_stage_];
		float left = std::min((lr.left + lr.right) / 2 - tw / 2, L.bar.right - tw - 4);
		left = std::max(4.0f, left);
		const D2D1_RECT_F box{left, L.bar.bottom + 4 * L.scale, left + tw, L.bar.bottom + 4 * L.scale + L.row};
		rounded(gx, box, 6 * L.scale, th.panel2);
		dc->DrawRoundedRectangle(D2D1::RoundedRect(box, 6 * L.scale, 6 * L.scale), gx.brush(th.line), 1.0f);
		gx.draw_text(s, f, D2D1::RectF(box.left + 10 * L.scale, box.top, box.right, box.bottom), th.text, false);
	}
}

void draw_log(App &app)
{
	const Theme &th = theme(app.design_);
	Gfx &gx = app.gfx_;
	auto *dc = gx.dc();
	const Layout &L = app.lay_;
	dc->FillRectangle(L.log, gx.brush(th.panel));
	dc->DrawLine({L.log.left, L.log.top + 0.5f}, {L.log.right, L.log.top + 0.5f}, gx.brush(th.line), 1.0f);
	IDWriteTextFormat *f = gx.text(L.font_s, false);
	const float rh = std::round(L.font_s * 1.6f);
	if (!app.log_open_) {
		gx.draw_text(app.status_, f, D2D1::RectF(L.log.left + 10 * L.scale, L.log.top, L.log.right - 10 * L.scale, L.log.bottom), th.text_dim);
		return;
	}
	float y = L.log.bottom - rh - 4 * L.scale;
	for (auto it = app.log_.rbegin(); it != app.log_.rend() && y >= L.log.top; ++it, y -= rh)
		gx.draw_text(*it, f, D2D1::RectF(L.log.left + 10 * L.scale, y, L.log.right - 10 * L.scale, y + rh), th.text_dim);
}

}  // namespace ng
