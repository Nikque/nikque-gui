// Keys, mouse and menu commands.
#include <windows.h>

#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>

#include "app.h"
#include "lang.h"

namespace ng {

namespace {

bool inside(const D2D1_RECT_F &r, float x, float y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }

}  // namespace

std::vector<const MoveEval *> App::sorted_moves(const PosInfo &info) const
{
	std::vector<const MoveEval *> v;
	v.reserve(info.moves.size());
	for (const auto &m : info.moves) v.push_back(&m);
	// unknown values go last whatever the order
	auto key = [&](const MoveEval &m, bool *known) -> long long {
		int x = 0;
		*known = true;
		switch (sort_col_) {
		case Column::Move: return m.sq;
		case Column::Value: *known = shown_value(m, use_book_, &x); return x;
		case Column::Stage: *known = m.book_row; return m.stage * 1000 + (m.missing >= 0 ? 999 - std::min<long long>(m.missing, 999) : 0);
		case Column::Missing: *known = m.missing >= 0; return m.missing;
		case Column::Positions: *known = m.positions >= 0 || m.lines >= 0; return m.positions >= 0 ? m.positions : m.lines;
		case Column::Level: x = m.in_book ? m.book_level : shown_level(m, use_book_); *known = x > 0; return x;
		case Column::Umi: *known = m.umi_known && !m.umi_none; return -(static_cast<long long>(m.umi_black) + m.umi_white);
		default: return 0;
		}
	};
	std::stable_sort(v.begin(), v.end(), [&](const MoveEval *a, const MoveEval *b) {
		bool ka, kb;
		const long long x = key(*a, &ka), y = key(*b, &kb);
		if (ka != kb) return ka;
		if (!ka) return a->sq < b->sq;
		if (x != y) return sort_desc_ ? x > y : x < y;
		int va = -999, vb = -999;  // then by value
		shown_value(*a, use_book_, &va);
		shown_value(*b, use_book_, &vb);
		if (va != vb) return va > vb;
		return a->sq < b->sq;
	});
	return v;
}

void App::position_changed()
{
	const Position &p = game_.now();
	if (selected_sq_ != kNoMove && !p.can_play(selected_sq_)) selected_sq_ = kNoMove;
	table_scroll_ = 0;
	request_analysis();
	update_animation_timer();
	invalidate();
}

void App::do_action(Action a)
{
	switch (a) {
	case Action::Back:
		if (game_.back()) position_changed();
		break;
	case Action::Forward:
		if (game_.forward()) position_changed();
		break;
	case Action::Back10:
		for (int i = 0; i < 10 && game_.back(); ++i) {}
		position_changed();
		break;
	case Action::Forward10:
		for (int i = 0; i < 10 && game_.forward(); ++i) {}
		position_changed();
		break;
	case Action::Start:
		game_.go_to(0);
		position_changed();
		break;
	case Action::End:
		game_.go_to(game_.size());
		position_changed();
		break;
	case Action::CandUp:
	case Action::CandDown: {
		const PosInfo *info = cache_.peek(game_.now());
		if (!info || info->moves.empty() || info->moves[0].sq == kPass) break;
		const auto rows = sorted_moves(*info);
		int i = -1;
		for (size_t k = 0; k < rows.size(); ++k)
			if (rows[k]->sq == selected_sq_) i = static_cast<int>(k);
		if (i < 0) i = a == Action::CandDown ? 0 : static_cast<int>(rows.size()) - 1;
		else i = std::clamp(i + (a == Action::CandDown ? 1 : -1), 0, static_cast<int>(rows.size()) - 1);
		selected_sq_ = rows[i]->sq;
		if (tab_ == Tab::Book) request_analysis();  // the missing lines of the move chosen
		invalidate();
		break;
	}
	case Action::PlayCand:
		if (selected_sq_ != kNoMove && game_.play(selected_sq_)) {
			selected_sq_ = kNoMove;
			position_changed();
		}
		break;
	case Action::ToggleBook: on_command(cmd::UseBook); break;
	case Action::ToggleUmi: on_command(umi_ == UmiMode::Off ? cmd::UmiBest : cmd::UmiOff); break;
	case Action::HideEval: on_command(cmd::ShowEval); break;
	case Action::CopyMoves: copy_moves(); break;
	case Action::PasteMoves: {
		const auto now = std::chrono::steady_clock::now();
		if (now - paste_armed_ < std::chrono::seconds(3)) {
			paste_armed_ = {};
			paste_moves();
		} else {
			paste_armed_ = now;
			set_status(lang::f("status.paste_again", {keys_.describe(Action::PasteMoves)}));
		}
		break;
	}
	case Action::TogglePanel: on_command(cmd::TogglePanel); break;
	case Action::ToggleLog: on_command(cmd::ToggleLog); break;
	default: break;
	}
}

void App::on_key(WPARAM vk)
{
	if (edit_field_) {
		if (vk == VK_RETURN) commit_edit();
		else if (vk == VK_ESCAPE) {
			edit_field_ = 0;
			invalidate();
		} else if (vk == VK_BACK) {
			if (!edit_text_.empty()) edit_text_.pop_back();
			invalidate();
		} else if (vk == VK_TAB) {
			const int next = edit_field_ == 1 ? 2 : 1;
			commit_edit();
			edit_field_ = next;
			edit_text_.clear();
			invalidate();
		}
		return;  // digits come with WM_CHAR
	}
	KeyChord k;
	k.vk = static_cast<UINT>(vk);
	k.ctrl = GetKeyState(VK_CONTROL) < 0;
	k.shift = GetKeyState(VK_SHIFT) < 0;
	k.alt = GetKeyState(VK_MENU) < 0;
	if (auto a = keys_.find(k)) do_action(*a);
}

void App::on_char(wchar_t c)
{
	if (!edit_field_) return;
	if (c >= L'0' && c <= L'9' && edit_text_.size() < 3) {
		edit_text_ += c;
		invalidate();
	}
}

void App::commit_edit()
{
	if (!edit_field_) return;
	if (!edit_text_.empty()) {
		const int v = std::stoi(edit_text_);
		if (edit_field_ == 1) set_loss(v, loss_y_);
		else set_loss(loss_x_, v);
	}
	edit_field_ = 0;
	edit_text_.clear();
	invalidate();
}

void App::set_loss(int x, int y)
{
	x = std::clamp(x, 0, 64);
	y = std::clamp(y, 0, 128);
	if (x == loss_x_ && y == loss_y_) return;
	loss_x_ = x;
	loss_y_ = y;
	// the X and Y count only for the "loss" condition of the umigame numbers here
	if (umi_ == UmiMode::Loss) request_analysis();
	invalidate();
}

void App::on_mouse_down(int x, int y, bool right, bool dbl)
{
	const float fx = static_cast<float>(x), fy = static_cast<float>(y);
	if (edit_field_ && !inside(lay_.fx.box, fx, fy) && !inside(lay_.fy.box, fx, fy)) commit_edit();
	const int sq = lay_.square_at(fx, fy);
	if (right) {
		// a right click goes one move back (on the board, also anywhere but the panel)
		if (sq != kNoMove || !inside(lay_.panel, fx, fy)) do_action(Action::Back);
		return;
	}
	if (sq != kNoMove) {
		if (game_.play(sq)) {
			selected_sq_ = kNoMove;
			position_changed();
		}
		return;
	}
	if (inside(opening_rect_, fx, fy)) {
		show_opening_ = !show_opening_;
		invalidate();
		return;
	}
	if (inside(lay_.log, fx, fy)) {
		on_command(cmd::ToggleLog);
		return;
	}
	if (panel_hidden_) return;
	if (!lay_.panel_below && inside(lay_.panel_split, fx, fy)) {
		drag_ = Drag::Panel;
		return;
	}
	if (inside(lay_.splitter, fx, fy)) {
		drag_ = Drag::Graph;
		return;
	}
	for (auto &[i, r] : tab_rects_)
		if (inside(r, fx, fy)) {
			on_command(cmd::TabMoves + i);
			return;
		}
	if (inside(lay_.fx.box, fx, fy) || inside(lay_.fy.box, fx, fy)) {
		edit_field_ = inside(lay_.fx.box, fx, fy) ? 1 : 2;
		edit_text_.clear();
		invalidate();
		return;
	}
	auto slider = [&](const Layout::Field &f, Drag d) {
		D2D1_RECT_F r = f.track;
		r.left -= 8 * lay_.scale;
		r.right += 8 * lay_.scale;
		if (f.track.right - f.track.left < 4 || !inside(r, fx, fy)) return false;
		drag_ = d;
		on_mouse_move(x, y);
		return true;
	};
	if (slider(lay_.fx, Drag::SliderX) || slider(lay_.fy, Drag::SliderY)) return;
	if (tab_ == Tab::Moves) {
		for (auto &[c, r] : col_rects_)
			if (inside(r, fx, fy)) {
				if (sort_col_ == c) sort_desc_ = !sort_desc_;
				else {
					sort_col_ = c;
					sort_desc_ = c != Column::Move;
				}
				invalidate();
				return;
			}
		for (auto &[s, r] : row_rects_)
			if (inside(r, fx, fy)) {
				if (dbl && game_.play(s)) {
					selected_sq_ = kNoMove;
					position_changed();
				} else {
					selected_sq_ = s;
					invalidate();
				}
				return;
			}
	}
	if (tab_ == Tab::Book) {
		for (auto &[k, r] : missing_rects_)
			if (inside(r, fx, fy) && k < static_cast<int>(missing_.lines.size())) {
				// to the position where the leaf is to be added
				for (const int mv : missing_.lines[k].moves)
					if (mv != kPass) game_.play(mv);
				selected_sq_ = kNoMove;
				position_changed();
				return;
			}
	}
	if (tab_ == Tab::Record) {
		for (auto &[n, r] : record_rects_)
			if (inside(r, fx, fy)) {
				game_.go_to(n);
				position_changed();
				return;
			}
	}
	if (inside(lay_.graph, fx, fy) && graph_plot_.right > graph_plot_.left) {
		// to the position nearest to the place clicked
		const int ply = static_cast<int>(std::lround((fx - graph_plot_.left) / (graph_plot_.right - graph_plot_.left) * 60));
		int best = 0;
		for (int i = 0; i <= game_.size(); ++i)
			if (std::abs(game_.at(i).ply() - ply) < std::abs(game_.at(best).ply() - ply)) best = i;
		game_.go_to(best);
		position_changed();
	}
}

void App::on_mouse_move(int x, int y)
{
	const float fx = static_cast<float>(x), fy = static_cast<float>(y);
	if (x >= 0 && (mouse_.x < 0 || true)) {
		TRACKMOUSEEVENT tme{sizeof tme, TME_LEAVE, hwnd_, 0};
		TrackMouseEvent(&tme);
	}
	mouse_ = {x, y};
	switch (drag_) {
	case Drag::Panel:
		panel_w_ = std::max(200.0f, (gfx_.width() - fx) / lay_.scale);
		layout();
		invalidate();
		return;
	case Drag::Graph: {
		const float top = lay_.range.bottom, bottom = lay_.graph.bottom;
		graph_ratio_ = std::clamp((bottom - fy) / std::max(1.0f, bottom - top), 0.1f, 0.9f);
		layout();
		invalidate();
		return;
	}
	case Drag::SliderX:
	case Drag::SliderY: {
		const Layout::Field &f = drag_ == Drag::SliderX ? lay_.fx : lay_.fy;
		const int hi = drag_ == Drag::SliderX ? 64 : 128;
		const float t = std::clamp((fx - f.track.left) / std::max(1.0f, f.track.right - f.track.left), 0.0f, 1.0f);
		const int v = static_cast<int>(std::lround(t * hi));
		if (drag_ == Drag::SliderX) set_loss(v, loss_y_);
		else set_loss(loss_x_, v);
		return;
	}
	default: break;
	}

	// hover: the expected line of a move, the meaning of a stage
	int hs = -1;
	for (int k = 0; k < 8; ++k)
		if (inside(legend_rects_[k], fx, fy)) hs = k;
	int sq = lay_.square_at(fx, fy);
	if (sq == kNoMove && tab_ == Tab::Moves && !panel_hidden_)
		for (auto &[s, r] : row_rects_)
			if (inside(r, fx, fy)) sq = s;
	bool redraw = hs != hover_stage_;
	hover_stage_ = hs;
	if (sq != hover_sq_) {
		hover_sq_ = sq;
		redraw = true;
		std::wstring s;
		if (const PosInfo *info = cache_.peek(game_.now()))
			if (const MoveEval *m = info->find(sq); m && !m->pv.empty()) {
				std::wstring line;
				for (int mv : m->pv) line += widen(square_name(mv)) + L" ";
				s = lang::f("status.pv", {widen(square_name(sq)), line});
			}
		if (!s.empty()) status_ = s;
		else if (!log_.empty()) status_ = log_.back();
	}
	if (redraw) invalidate();
}

void App::on_mouse_up(int, int)
{
	if (drag_ != Drag::None) {
		drag_ = Drag::None;
		invalidate();
	}
}

void App::on_wheel(int x, int y, int delta)
{
	const float fx = static_cast<float>(x), fy = static_cast<float>(y);
	const int step = delta > 0 ? 1 : -1;
	if (inside(lay_.fx.box, fx, fy) || inside(lay_.fx.track, fx, fy)) set_loss(loss_x_ + step, loss_y_);
	else if (inside(lay_.fy.box, fx, fy) || inside(lay_.fy.track, fx, fy)) set_loss(loss_x_, loss_y_ + step);
	else if (inside(lay_.table, fx, fy) && tab_ == Tab::Book) {
		missing_scroll_ = std::max(0, missing_scroll_ - step * 3);
		invalidate();
	}
	else if (inside(lay_.table, fx, fy)) {
		table_scroll_ = std::max(0, table_scroll_ - step * 3);
		invalidate();
	}
}

void App::copy_moves()
{
	const std::wstring text = widen(game_.to_text(true));
	if (!OpenClipboard(hwnd_)) return;
	EmptyClipboard();
	if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t))) {
		std::copy_n(text.c_str(), text.size() + 1, static_cast<wchar_t *>(GlobalLock(h)));
		GlobalUnlock(h);
		SetClipboardData(CF_UNICODETEXT, h);
	}
	CloseClipboard();
	set_status(lang::f("status.copied", {std::to_wstring(text.size() / 2)}));
}

void App::paste_moves()
{
	std::wstring text;
	if (OpenClipboard(hwnd_)) {
		if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
			if (const wchar_t *p = static_cast<const wchar_t *>(GlobalLock(h))) text = p;
			GlobalUnlock(h);
		}
		CloseClipboard();
	}
	const std::string u = narrow(text);
	// a board (64 squares and the side): the game starts there
	if (auto b = parse_board_text(u)) {
		game_.reset(*b);
		log(lang::t("status.pasted_board"));
		position_changed();
		return;
	}
	// a blog board (the numbers of the moves by square), else coordinates in any text
	ParsedMoves pm;
	if (auto pl = parse_param_lines(u)) pm.moves = *pl;
	else pm = parse_moves(u);
	if (pm.moves.empty()) {
		set_status(lang::t("status.paste_none"));
		return;
	}
	Game g;
	const int n = replay(g, Position{}, pm.moves);
	if (n == 0) {
		set_status(lang::t("status.paste_bad"));
		return;
	}
	game_ = g;
	game_.go_to(game_.size());
	log(n == static_cast<int>(pm.moves.size()) ? lang::f("status.pasted", {std::to_wstring(n)})
	                                           : lang::f("status.pasted_part", {std::to_wstring(n), std::to_wstring(pm.moves.size())}));
	position_changed();
}

void App::on_command(int id)
{
	if (id >= cmd::LearnDeviate && id <= cmd::LearnFix) {
		learn_command(id);
		return;
	}
	if (id == cmd::LearnStop) {
		stop_learning();
		return;
	}
	if (id == cmd::QueueMissing) {
		queue_missing();
		update_menu();
		return;
	}
	if (id == cmd::QueueStart) {
		if (learn_queue_.empty() || learning_) return;
		if (MessageBoxW(hwnd_, lang::f("queue.ask", {std::to_wstring(learn_queue_.size()), book_file()}).c_str(), lang::t("app.title").c_str(), MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
		queue_running_ = true;
		next_in_queue();
		return;
	}
	if (id == cmd::QueueClear) {
		learn_queue_.clear();
		log(lang::t("queue.cleared"));
		update_menu();
		return;
	}
	if (id == cmd::LearnSettings) {
		open_learn_settings();
		return;
	}
	if (id == cmd::RangeSettings) {
		open_range_settings();
		return;
	}
	if (id == cmd::LearnContinue) {
		continue_learning();
		return;
	}
	if (id >= cmd::RecentBook0 && id < cmd::RecentBook0 + 8) {
		const std::string r = ini_.get("books", "recent" + std::to_string(id - cmd::RecentBook0 + 1));
		if (!r.empty()) switch_book(widen(r));
		return;
	}
	if (id >= cmd::Level0 && id < cmd::Level0 + static_cast<int>(std::size(kLevels))) {
		level_ = kLevels[id - cmd::Level0];
		update_menu();
		request_analysis();
		invalidate();
		return;
	}
	switch (id) {
	case cmd::NewGame:
		game_.reset(Position{});
		position_changed();
		break;
	case cmd::Undo: do_action(Action::Back); break;
	case cmd::Redo: do_action(Action::Forward); break;
	case cmd::ToStart: do_action(Action::Start); break;
	case cmd::ToEnd: do_action(Action::End); break;
	case cmd::ShowEval:
		show_eval_ = !show_eval_;
		update_menu();
		position_changed();
		break;
	case cmd::Backward:
		backward_ = !backward_;
		update_menu();
		request_analysis();
		invalidate();
		break;
	case cmd::Recompute:
		cache_.clear_search();
		cache_.clear_book();
		request_analysis();
		invalidate();
		break;
	case cmd::UmiBest:
	case cmd::UmiLoss:
	case cmd::UmiOff:
		umi_ = id == cmd::UmiBest ? UmiMode::Best : id == cmd::UmiLoss ? UmiMode::Loss : UmiMode::Off;
		update_menu();
		request_analysis();
		invalidate();
		break;
	case cmd::CountOn:
		count_ = !count_;
		update_menu();
		request_analysis();
		break;
	case cmd::UseBook:
		use_book_ = !use_book_;
		update_menu();
		position_changed();
		break;
	case cmd::ReloadBook:
		cache_.clear_book();
		eng_[0].proc.close();
		start_engines();
		position_changed();
		break;
	case cmd::BaseDeviate:
	case cmd::BaseDeviate2:
	case cmd::BaseDeviate3:
		base_ = id == cmd::BaseDeviate ? "deviate" : id == cmd::BaseDeviate2 ? "deviate2" : "deviate3";
		update_menu();
		request_analysis();
		invalidate();
		break;
	case cmd::BookInfo: on_command(cmd::TabBook); break;
	case cmd::OpenBook: {
		wchar_t file[MAX_PATH * 4] = L"";
		OPENFILENAMEW of{sizeof of};
		of.hwndOwner = hwnd_;
		of.lpstrFilter = L"book (*.dat;*.dev*;*.store)\0*.dat;*.dev*;*.store\0*.*\0*.*\0";
		of.lpstrFile = file;
		of.nMaxFile = static_cast<DWORD>(std::size(file));
		of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		if (GetOpenFileNameW(&of)) switch_book(file);
		break;
	}
	case cmd::ExportMissing: export_missing(); break;
	case cmd::FixRecord:
		fixed_on_ = !fixed_on_;
		if (fixed_on_) {
			fixed_ = game_.moves();
			fixed_start_ = game_.start();
			set_status(lang::f("status.fixed", {std::to_wstring(game_.size())}));
		}
		update_menu();
		invalidate();
		break;
	case cmd::EngineNikque:
	case cmd::EngineNewedax: {
		const EngineKind k = id == cmd::EngineNewedax ? EngineKind::Newedax : EngineKind::Nikque18;
		if (k == kind_) break;
		if (MessageBoxW(hwnd_, lang::t("ask.switch_engine").c_str(), lang::t("app.title").c_str(),
		                MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
			break;
		kind_ = k;
		update_menu();
		restart_engines();
		break;
	}
	case cmd::RestartEngines: restart_engines(); break;
	case cmd::EngineVersion:
		send(0, "version\n");
		log_open_ = true;
		update_menu();
		layout();
		invalidate();
		break;
	case cmd::CopyMoves: copy_moves(); break;
	case cmd::PasteMoves: paste_moves(); break;
	case cmd::DesignStandard:
	case cmd::DesignPaw:
		design_ = id == cmd::DesignPaw ? Design::Paw : Design::Standard;
		gfx_.clear_shapes();
		set_window_icon();
		update_menu();
		layout();
		invalidate();
		break;
	case cmd::EffectsOn:
	case cmd::EffectsSubdued:
	case cmd::EffectsOff:
		effects_ = id == cmd::EffectsOn ? Effects::On : id == cmd::EffectsSubdued ? Effects::Subdued : Effects::Off;
		update_menu();
		update_animation_timer();
		invalidate();
		break;
	case cmd::TopStrong:
	case cmd::TopNormal:
	case cmd::TopWeak:
		top_effect_ = static_cast<TopEffect>(id - cmd::TopStrong);
		update_menu();
		invalidate();
		break;
	case cmd::TogglePanel:
		panel_hidden_ = !panel_hidden_;
		update_menu();
		layout();
		invalidate();
		break;
	case cmd::ToggleLog:
		log_open_ = !log_open_;
		update_menu();
		layout();
		invalidate();
		break;
	case cmd::TabMoves:
	case cmd::TabBook:
	case cmd::TabRecord:
		tab_ = static_cast<Tab>(id - cmd::TabMoves);
		update_menu();
		if (tab_ == Tab::Book) request_levels();
		invalidate();
		break;
	case cmd::OpenConfig:
		save_config();
		ShellExecuteW(hwnd_, L"open", L"notepad.exe", (L"\"" + ini_path_ + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
		break;
	case cmd::LangJa: break;
	case cmd::HelpKeys: {
		std::wstring s;
		for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
			const Action a = static_cast<Action>(i);
			s += lang::t(std::string("action.") + KeyMap::name(a)) + L"\t" + keys_.describe(a) + L"\n";
		}
		s += lang::t("action.right_click") + L"\n\n" + lang::t("help.keys_change");
		MessageBoxW(hwnd_, s.c_str(), lang::t("menu.help.keys").c_str(), MB_OK);
		break;
	}
	case cmd::HelpAbout:
		MessageBoxW(hwnd_, lang::f("help.about", {widen(NG_VERSION)}).c_str(), lang::t("app.title").c_str(), MB_OK);
		break;
	default: break;
	}
}

}  // namespace ng
