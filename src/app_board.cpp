// The board: its orientation (the game turned by a symmetry), setting up a position, the line of moves of a
// position (the book first, else a search without the book), positions as text and bitboards, images of the
// window and of the board, the numbers of the moves.
#include <windows.h>

#include <ctime>

#include "app.h"
#include "form.h"
#include "lang.h"
#include "linefind.h"

namespace ng {

namespace {

constexpr int kEstimateMaxPly = 20;           // positions up to the 20th move
constexpr long long kSearchNodes = 50'000'000;  // the search without the book gives up after this

std::wstring stamp()
{
	const std::time_t t = std::time(nullptr);
	std::tm tm{};
	localtime_s(&tm, &t);
	wchar_t b[32];
	wcsftime(b, 32, L"%Y%m%d-%H%M%S", &tm);
	return b;
}

}  // namespace

// ---- orientation

void App::orient_to(int s)
{
	if (editing_ || s < 0 || s > 7) return;
	const int d = symmetry_compose(symmetry_inverse(orient_), s);  // undo the orientation shown, then s
	orient_ = s;
	if (d != 0) {
		// what is known of the positions goes with them (the book is the same for symmetric positions)
		for (int i = 0; i <= game_.size(); ++i) cache_.copy_symmetric(game_.at(i), d);
		game_ = transformed(game_, d);
		fixed_start_ = symmetric(fixed_start_, d);
		for (auto &m : fixed_)
			if (m != kPass) m = symmetric_square(m, d);
		selected_sq_ = hover_sq_ = kNoMove;
	}
	set_status(lang::f("status.orient", {lang::t("orient." + std::to_string(s))}));
	update_menu();
	position_changed();
}

// ---- a new game on the board

void App::set_game(const Game &g, bool keep_old)
{
	if (keep_old && game_.size() > 0) keep_now(true);
	game_ = g;
	orient_ = 0;
	selected_sq_ = hover_sq_ = kNoMove;
	update_menu();
	position_changed();
}

// ---- setting up a position

void App::begin_edit()
{
	if (editing_) return;
	editing_ = true;
	edit_pos_ = game_.now();
	set_status(lang::t("edit.help"));
	update_menu();
	update_animation_timer();
	invalidate();
}

void App::end_edit(bool keep)
{
	if (!editing_) return;
	editing_ = false;
	if (keep) {
		Game g;
		g.reset(edit_pos_);
		set_game(g, true);
		log(lang::f("edit.done", {widen(edit_pos_.to_board_string())}));
	} else {
		set_status(lang::t("edit.cancelled"));
	}
	update_menu();
	invalidate();
}

void App::edit_click(int sq, bool right)
{
	const u64 b = 1ULL << sq;
	if (right) {
		edit_pos_.black &= ~b;
		edit_pos_.white &= ~b;
	} else if (edit_pos_.black & b) {  // black -> white -> empty
		edit_pos_.black &= ~b;
		edit_pos_.white |= b;
	} else if (edit_pos_.white & b) {
		edit_pos_.white &= ~b;
	} else {
		edit_pos_.black |= b;
	}
	invalidate();
}

bool App::edit_key(WPARAM vk)
{
	switch (vk) {
	case VK_RETURN: end_edit(true); return true;
	case VK_ESCAPE: end_edit(false); return true;
	case 'T': on_command(cmd::EditSide); return true;
	case VK_DELETE: on_command(cmd::EditClear); return true;
	case 'I': on_command(cmd::EditInitial); return true;
	default: return false;
	}
}

// ---- the line of moves of a position: the book first (book findline), else a search without the book

void App::estimate_line()
{
	const Position target = game_.now();
	if (target.ply() > kEstimateMaxPly) {
		MessageBoxW(hwnd_, lang::f("estimate.too_late", {std::to_wstring(kEstimateMaxPly)}).c_str(), lang::t("app.title").c_str(), MB_ICONINFORMATION);
		return;
	}
	if (target == Position{}) return;
	if (findline_id_ || task_busy_) {
		set_status(lang::t("estimate.busy"));
		return;
	}
	findline_target_ = target;
	findline_found_ = false;
	if (eng_[0].alive && !learning_ && use_book_) {
		findline_id_ = next_id_++;
		const std::string t = target.to_board_string();  // "<64> X": the side right after the squares
		std::string out = "setboard " + Position{}.to_board_string() + "\n";
		eng_[0].board = key_of(Position{});
		out += "book findline id=" + std::to_string(findline_id_) + " target=" + t.substr(0, 64) + t.substr(65, 1) + "\n";
		requests_[findline_id_] = {Request::FindLine, key_of(target), 0};
		send(0, out);
		set_status(lang::t("estimate.book"));
		return;
	}
	estimate_without_book(target);
}

void App::findline_answer(const Fields &f, bool end)
{
	if (f.kind() == "line" && !findline_found_) {
		const ParsedMoves pm = parse_moves(f.get("moves").value_or(""));
		Game g;
		replay(g, Position{}, pm.moves);
		if (g.now() == findline_target_) {
			findline_found_ = true;
			set_game(g, true);
			log(lang::f("estimate.found_book", {std::to_wstring(g.size())}));
		}
	}
	if (!end) return;
	findline_id_ = 0;
	if (!findline_found_) {
		log(lang::t(f.num("unknown").value_or(0) ? "estimate.no_command" : "estimate.not_in_book"));
		estimate_without_book(findline_target_);
	}
	pump_script();
}

void App::estimate_without_book(const Position &target)
{
	run_task(lang::t("estimate.searching"), [this, target](std::stop_token st) -> std::function<void()> {
		const auto t0 = std::chrono::steady_clock::now();
		const LineSearch r = find_line(Position{}, target, kSearchNodes, st);
		const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
		return [this, r, target, ms] {
			if (r.line) {
				Game g;
				for (const int m : *r.line)
					if (m != kPass) g.play(m);
				if (g.now() == target) {
					set_game(g, true);
					log(lang::f("estimate.found_search", {std::to_wstring(g.size()), std::to_wstring(r.nodes), std::to_wstring(ms)}));
					return;
				}
			}
			log(lang::f(r.gave_up ? "estimate.gave_up" : "estimate.none", {std::to_wstring(r.nodes), std::to_wstring(ms)}));
		};
	});
}

// ---- positions as text

void App::copy_text(const std::wstring &text)
{
	if (!OpenClipboard(hwnd_)) return;
	EmptyClipboard();
	if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t))) {
		std::copy_n(text.c_str(), text.size() + 1, static_cast<wchar_t *>(GlobalLock(h)));
		GlobalUnlock(h);
		SetClipboardData(CF_UNICODETEXT, h);
	}
	CloseClipboard();
}

std::wstring App::clipboard_text()
{
	std::wstring text;
	if (OpenClipboard(hwnd_)) {
		if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
			if (const wchar_t *p = static_cast<const wchar_t *>(GlobalLock(h))) text = p;
			GlobalUnlock(h);
		}
		CloseClipboard();
	}
	return text;
}

void App::copy_position(int id)
{
	const Position &p = editing_ ? edit_pos_ : game_.now();
	std::string s;
	switch (id) {
	case cmd::CopyBoardText: s = p.to_board_string(); break;
	case cmd::CopyObf: s = obf_text(p); break;
	case cmd::CopyBitDecimal: s = bitboard_text(p, BitForm::Decimal); break;
	case cmd::CopyBitHex: s = bitboard_text(p, BitForm::Hex); break;
	case cmd::CopyBitNtest: s = bitboard_text(p, BitForm::Ntest); break;
	default: return;
	}
	copy_text(widen(s));
	log(lang::f("status.position_copied", {widen(s)}));
}

bool App::take_position_text(const std::string &text)
{
	std::optional<Position> p = parse_board_text(text);  // the board string, or a line of a problem file
	if (!p) p = parse_bitboards(text, false);
	if (!p) return false;
	Game g;
	g.reset(*p);
	set_game(g, true);
	log(lang::t("status.pasted_board"));
	return true;
}

void App::import_position()
{
	using lang::t;
	std::vector<FormField> f(2);
	f[0].kind = FormField::Text;
	f[0].label = t("import.text");
	f[0].text = clipboard_text();
	if (f[0].text.size() > 400) f[0].text.clear();
	f[0].note = t("import.text.note");
	f[1].kind = FormField::Choice;
	f[1].label = t("import.form");
	f[1].choices = {t("import.form.auto"), t("import.form.ntest")};
	if (!run_form(hwnd_, t("menu.io.import"), f, t("form.ok"), t("form.cancel"), t("form.bad_number"))) return;
	const std::string s = narrow(f[0].text);
	if (f[1].choice == 1) {
		if (auto p = parse_bitboards(s, true)) {
			Game g;
			g.reset(*p);
			set_game(g, true);
			log(lang::t("status.pasted_board"));
			return;
		}
	} else if (take_position_text(s)) {
		return;
	}
	MessageBoxW(hwnd_, t("import.bad").c_str(), t("app.title").c_str(), MB_ICONWARNING);
}

// ---- images

void App::screenshot(const std::wstring &file)
{
	std::wstring path = file;
	if (path.empty()) {
		const std::string d = ini_.get("view", "screenshot_dir");
		std::wstring dir = d.empty() ? exe_dir_ + L"\\screenshots"
		                             : (d.size() > 1 && d[1] == ':' ? widen(d) : exe_dir_ + L"\\" + widen(d));
		CreateDirectoryW(dir.c_str(), nullptr);
		path = dir + L"\\" + stamp() + L".png";
		for (int k = 2; GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; ++k)
			path = dir + L"\\" + stamp() + L"-" + std::to_wstring(k) + L".png";
	}
	compute_line();
	prepare_glows();
	const Theme &th = theme(design_);
	const bool ok = gfx_.save_png(path, D2D1::RectU(0, 0, gfx_.width(), gfx_.height()), [&] {
		gfx_.dc()->Clear(th.bg);
		draw_board(*this);
		draw_panel(*this);
		draw_log(*this);
		draw_bar(*this);
	});
	log(ok ? lang::f("status.screenshot", {path}) : lang::f("status.screenshot_failed", {path}));
}

void App::save_board_image()
{
	const std::wstring flt = lang::t("filter.png") + std::wstring(L" (*.png)\0*.png\0", 16);
	const std::wstring f = pick_save(flt.c_str(), L"png", L"board-" + stamp() + L".png");
	if (f.empty()) return;
	compute_line();
	prepare_glows();
	const Theme &th = theme(design_);
	const int coord = static_cast<int>(std::round(lay_.font * 1.2f));
	const int pad = static_cast<int>(std::round(4 * lay_.scale));
	const UINT left = static_cast<UINT>(std::max(0, lay_.bx - coord - pad)), top = static_cast<UINT>(std::max(0, lay_.by - coord - pad));
	const UINT right = std::min<UINT>(gfx_.width(), static_cast<UINT>(lay_.bx + lay_.board_px + pad));
	const UINT bottom = std::min<UINT>(gfx_.height(), static_cast<UINT>(lay_.by + lay_.board_px + pad));
	const bool ok = gfx_.save_png(f, D2D1::RectU(left, top, right, bottom), [&] {
		gfx_.dc()->Clear(th.bg);
		draw_board(*this);
	});
	log(ok ? lang::f("status.screenshot", {f}) : lang::f("status.screenshot_failed", {f}));
}

// ---- the numbers of the moves

int App::move_number_at(int sq) const
{
	int n = 0, at = 0;
	for (int i = 0; i < game_.cursor(); ++i) {
		const int m = game_.moves()[i];
		if (m == kPass) continue;
		++n;
		if (m == sq) at = n;
	}
	return at;
}

}  // namespace ng
