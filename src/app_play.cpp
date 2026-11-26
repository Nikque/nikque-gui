// Games against the computer (a person against it, or the computer on both sides): the moves of the computer
// are searched by the play engine (a fourth engine, no book); its opening moves come from the book analyses of
// the main engine (the GUI has them), so that a large book is read once. The time of a game, taking back,
// what is done at the end of a game. And the compare mode: the other engine (search only) beside the table.
#include "platform.h"

#include <algorithm>
#include <optional>
#include <random>
#include <utility>

#include "app.h"
#include "form.h"
#include "lang.h"
#include "measure.h"

namespace ng {

namespace {

constexpr UINT kBookMoveDelayMs = 400;
constexpr UINT kBookWaitMs = 3000;      // the book analysis not there by then: the move is searched

std::mt19937_64 &rng()
{
	static std::mt19937_64 r{std::random_device{}()};
	return r;
}

// who plays, for the log and the bar
const std::wstring &play_mode_name(PlayMode m)
{
	const char *names[] = {"play.free", "play.human_black", "play.human_white", "play.engines"};
	return lang::t(names[static_cast<int>(m)]);
}

std::wstring clock_text(long long ms)
{
	if (ms < 0) ms = 0;
	const long long s = (ms + 999) / 1000;
	wchar_t b[32];
	swprintf(b, 32, L"%lld:%02lld", s / 60, s % 60);
	return b;
}

}  // namespace

bool App::start_play_engine()
{
	EngineSlot &s = eng_[3];
	if (s.alive && s.proc.running()) return true;
	if (!engine_files_ok(kind_)) return false;
	const std::wstring exe = engine_exe();
	const auto nb = nobook_args(L"nobook-play.dat");
	if (!nb) {
		log(lang::f("err.engine_name", {temp_dir_}));
		return false;
	}
	const auto common = engine_args();
	if (!common) return false;
	const std::wstring args = *common + *nb;
	++s.generation;
	s.lost = false;
	std::wstring err;
	s.alive = s.proc.start(exe, args, engine_dir(), hwnd_, WM_ENGINE, engine_tag(3, s.generation), &err);
	if (!s.alive) {
		log(lang::f("log.start_failed", {exe, err}));
		return false;
	}
	s.board.reset();
	send(3, "mode 3\n");
	// (sent only when on: an engine without the setting, nikque Edax for one, would warn about it)
	if (keep_hash_) send(3, "keep-hash on\n");
	log(lang::f("play.engine_started", {exe}));
	return true;
}

bool App::com_turn() const
{
	if (editing_) return false;
	const Position &p = game_.now();
	if (p.game_over()) return false;
	// one move asked for: from the position shown, also before the end of the game (the moves after it are cut,
	// as for a move of the person)
	if (com_once_ && key_of(p) == com_once_pos_) return true;
	if (game_.cursor() != game_.size()) return false;
	switch (play_mode_) {
	case PlayMode::HumanBlack: return p.side == kWhite;
	case PlayMode::HumanWhite: return p.side == kBlack;
	case PlayMode::Engines: return true;
	default: return false;
	}
}

void App::com_stop()
{
	if (com_thinking_ || com_pondering_) send(3, "stop\n");  // a move given by the stop is not played
	com_thinking_ = com_pondering_ = com_accept_ = false;
	com_book_move_ = kNoMove;
	com_waiting_book_ = false;
	kill_timer(kTimerCom);
}

void App::com_think()
{
	const Position &p = game_.now();
	const PosKey key = key_of(p);
	if (!com_turn()) {
		if (com_thinking_ || com_book_move_ != kNoMove || com_waiting_book_) com_stop();
		// the person's turn: the play engine searches this position meanwhile (its hash table helps the next move)
		if (ponder_ && play_mode_ != PlayMode::Free && !p.game_over() && game_.cursor() == game_.size() && !editing_ &&
		    !com_pondering_ && eng_[3].alive) {
			send(3, "setboard " + p.to_board_string() + "\nhint 60\n");
			eng_[3].board.reset();
			com_pondering_ = true;
		}
		return;
	}
	if ((com_thinking_ || com_book_move_ != kNoMove || com_waiting_book_) && com_pos_ == key) return;  // on it already
	com_stop();
	com_pos_ = key;
	// an opening move from the book analyses of the position
	const bool book_skipped = com_no_book_set_ && com_no_book_ == key;
	if (opening_ != Opening::Search && use_book_ && p.ply() < opening_until_ && eng_[0].alive && !learning_ && !book_skipped) {
		const PosInfo *info = cache_.peek(p);
		if (!info || (!info->values_done && !info->book_failed)) {
			com_waiting_book_ = true;  // com_think again when the answer has come (or the timer)
			set_timer(kTimerCom, kBookWaitMs);
			return;
		}
		// the moves of the book position: its links and its leaf, as Edax plays from its book
		const auto preview = requests_.find(info->book_preview_id);
		auto book_value = [&](size_t i) -> std::optional<int> {
			if (info->moves[i].sq == kPass) return std::nullopt;
			const BookMoveEval &m = preview == requests_.end() ? static_cast<const BookMoveEval &>(info->moves[i])
			                                                       : preview->second.previous_book_moves[i];
			if (m.in_book) return m.book_value;
			if (m.leaf) return m.leaf_value;
			return std::nullopt;
		};
		int best = -999;
		if (info->values_done)
			for (size_t i = 0; i < info->moves.size(); ++i)
				if (const auto v = book_value(i)) best = std::max(best, *v);
		if (best > -999) {
			const int width = opening_ == Opening::BookBest ? 0 : opening_width_;
			std::vector<int> choice;
			for (size_t i = 0; i < info->moves.size(); ++i)
				if (const auto v = book_value(i); v && *v >= best - width) choice.push_back(info->moves[i].sq);
			com_book_move_ = choice[rng()() % choice.size()];
			set_timer(kTimerCom, kBookMoveDelayMs);
			return;
		}
	}
	// a search by the play engine
	if (!start_play_engine()) {
		set_play_mode(PlayMode::Free);
		return;
	}
	std::string out = "setboard " + p.to_board_string() + "\n";
	if (game_clock_) {
		// the time of a move: what is left shared over the moves still to come of this side
		// (in tenths of a second). The engine's level stays a bound in this mode, so it is raised to 60 first:
		// "level" sets the play by level again, so "move-time" comes after it.
		const long long left = clock_left_ms_[p.side];
		const long long moves = std::max(1, (p.empties() + 1) / 2);
		const long long tenths = std::max<long long>(1, std::min<long long>(left / moves, left - 500) / 100);
		out += "level 60\nmove-time " + std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "\n";
	} else {
		out += "level " + std::to_string(com_level_) + "\n";
	}
	// A marker before the "go": the engine plays and tells the move of a search stopped by "stop" as well, and that
	// line comes before the answer of the marker (the lines of the engine's main loop keep their order).
	com_marker_id_ = new_id();
	out += "echo M" + std::to_string(com_marker_id_) + "\ngo\n";
	com_thinking_ = true;
	com_accept_ = false;
	eng_[3].board.reset();
	send(3, out);
	invalidate();
}

void App::com_timer()
{
	kill_timer(kTimerCom);
	if (com_waiting_book_) {
		// the book analysis has not come: this move is searched
		com_waiting_book_ = false;
		com_no_book_ = com_pos_;
		com_no_book_set_ = true;
		com_think();
		return;
	}
	if (com_book_move_ == kNoMove) return;
	const int sq = com_book_move_;
	com_book_move_ = kNoMove;
	if (key_of(game_.now()) == com_pos_ && com_turn()) com_played(sq);
}

void App::com_played(int sq)
{
	if (!game_.now().can_play(sq)) return;
	if (clock_ran_out()) return;  // (a move after the end of the time is not played)
	com_once_ = false;
	game_.play(sq);
	selected_sq_ = kNoMove;
	position_changed();  // the clock goes to the other side there
}

void App::play_line(const std::string &line)
{
	if (const int mark = echo_marker(line); mark >= 0) {
		if (com_thinking_ && mark == com_marker_id_) com_accept_ = true;
		return;
	}
	if (!line.starts_with("Edax plays ")) return;  // the lines of a search to ponder are not shown
	const int sq = parse_square(line.substr(11, 2));
	if (!com_thinking_ || !com_accept_) return;  // a move of a search stopped before
	com_thinking_ = com_accept_ = false;
	if (key_of(game_.now()) == com_pos_ && com_turn() && sq != kNoMove) com_played(sq);
}

bool App::take_back()
{
	if (play_mode_ != PlayMode::HumanBlack && play_mode_ != PlayMode::HumanWhite) return false;
	com_stop();
	const int person = play_mode_ == PlayMode::HumanBlack ? kBlack : kWhite;
	if (!game_.back()) return true;
	// back to the person's turn: two moves when the computer had answered
	while (game_.now().side != person && game_.back()) {}
	game_.truncate();
	game_end_done_ = false;
	clock_switch();
	return true;
}

void App::new_play_game(bool keep_old)
{
	clock_left_ms_[0] = clock_left_ms_[1] = static_cast<long long>(clock_minutes_) * 60000;
	game_clock_ = clock_minutes_ > 0;
	clock_side_ = -1;
	set_game(Game{}, keep_old);
	clock_switch();
}

void App::set_play_mode(PlayMode m, int games)
{
	com_stop();
	com_once_ = false;
	play_mode_ = m;
	engine_games_left_ = m == PlayMode::Engines ? std::max(0, games - 1) : 0;
	game_end_done_ = false;
	if (m != PlayMode::Free) {
		list_index_ = -1;
		new_play_game(true);  // (the game before is kept)
		log(lang::f("play.started", {play_mode_name(m), std::to_wstring(com_level_)}));
	} else {
		clock_side_ = -1;
		kill_timer(kTimerClock);
	}
	update_menu();
	invalidate();
	com_think();
}

void App::clock_switch()
{
	const auto now = std::chrono::steady_clock::now();
	const int was = clock_side_;
	if (was >= 0) clock_left_ms_[was] -= std::chrono::duration_cast<std::chrono::milliseconds>(now - clock_t0_).count();
	clock_t0_ = now;
	// the side whose clock ran has used its time (a move taken without clock_ran_out, after the last tick): that side
	// has lost on time, not the other one
	if (was >= 0 && clock_left_ms_[was] <= 0 && game_clock_ && play_mode_ != PlayMode::Free) {
		clock_out(was);
		return;
	}
	const Position &p = game_.now();
	if (!game_clock_ || play_mode_ == PlayMode::Free || p.game_over()) {
		clock_side_ = -1;
		kill_timer(kTimerClock);
		return;
	}
	clock_side_ = p.side;
	set_timer(kTimerClock, 1000);
}

void App::clock_tick()
{
	if (clock_side_ < 0) {
		kill_timer(kTimerClock);
		return;
	}
	if (clock_ran_out()) return;
	paint_bar();
}

bool App::clock_ran_out()
{
	if (clock_side_ < 0) return false;
	const long long left = clock_left_ms_[clock_side_] -
	                       std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - clock_t0_).count();
	if (left > 0) return false;
	clock_out(clock_side_);
	return true;
}

void App::clock_out(int side)
{
	if (clock_side_ == side) {  // (its time is counted up to now)
		const auto now = std::chrono::steady_clock::now();
		clock_left_ms_[side] -= std::chrono::duration_cast<std::chrono::milliseconds>(now - clock_t0_).count();
		clock_t0_ = now;
	}
	clock_side_ = -1;
	kill_timer(kTimerClock);
	log(lang::t(side == kBlack ? "play.time_out_black" : "play.time_out_white"));
	set_play_mode(PlayMode::Free);
}

void App::game_end_check()
{
	if (play_mode_ == PlayMode::Free || game_end_done_) return;
	const Position &p = game_.now();
	if (!p.game_over() || game_.cursor() != game_.size()) return;
	game_end_done_ = true;
	clock_switch();
	if constexpr (measure::kOn) {
		// the points of the graph at the end, with what each comes from
		const auto line = backward_line(game_, cache_, use_book_, backward_);
		for (int i = 0; i <= game_.size(); ++i) {
			const Position &q = game_.at(i);
			std::string s = "GRAPH " + std::to_string(i) + " " + q.to_board_string() + " known=" + std::to_string(line[i].known) +
			                " best=" + std::to_string(line[i].best);
			if (const PosInfo *info = cache_.peek(q))
				for (const MoveEval &m : info->moves)
					s += " [" + square_name(m.sq) + " s=" + std::to_string(m.searched) + ":" + std::to_string(m.score) + "@" +
					     std::to_string(m.depth) + " b=" + std::to_string(m.in_book) + ":" + std::to_string(m.book_value) + " l=" +
					     std::to_string(m.leaf) + ":" + std::to_string(m.leaf_value) + "]";
			measure::trace(s);
		}
	}
	const int b = std::popcount(p.black), w = std::popcount(p.white);
	log(lang::f(b > w ? "play.won_black" : b < w ? "play.won_white" : "play.draw", {std::to_wstring(b), std::to_wstring(w)}));
	if (end_keep_) keep_now(true);
	if (end_list_ && !task_busy_) list_add_game();
	// the count goes down when the next game starts, so a script waiting for the games waits for that one too
	if (play_mode_ == PlayMode::Engines && engine_games_left_ > 0) set_timer(kTimerNextGame, 1500);
}

void App::play_command(int id)
{
	using lang::t;
	auto number = [&](const char *title, const char *label, int value, int lo, int hi, const char *note) {
		return ask_number(hwnd_, t(title), t(label), value, lo, hi, t(note));
	};
	switch (id) {
	case cmd::PlayFree: set_play_mode(PlayMode::Free); break;
	case cmd::PlayHumanBlack: set_play_mode(PlayMode::HumanBlack); break;
	case cmd::PlayHumanWhite: set_play_mode(PlayMode::HumanWhite); break;
	case cmd::PlayEngines:
		if (auto n = number("menu.play.engines", "play.games", 1, 1, 10000, "play.games.note")) set_play_mode(PlayMode::Engines, *n);
		break;
	case cmd::PlayEngineMove:
		com_once_ = true;
		com_once_pos_ = key_of(game_.now());
		com_think();
		break;
	case cmd::PlayTakeBack:
		if (take_back()) position_changed();
		break;
	case cmd::PlayStop: set_play_mode(PlayMode::Free); break;
	case cmd::PlayLevel:
		if (auto n = number("menu.play.level", "play.level", com_level_, 1, 60, "play.level.note")) com_level_ = *n;
		break;
	case cmd::OpeningSearch: opening_ = Opening::Search; break;
	case cmd::OpeningBest: opening_ = Opening::BookBest; break;
	case cmd::OpeningWide: opening_ = Opening::BookWide; break;
	case cmd::OpeningSettings: {
		std::vector<FormField> f(2);
		f[0] = number_field(t("opening.width"), opening_width_, 0, 64, t("opening.width.note"));
		f[1] = number_field(t("opening.until"), opening_until_, 0, 60, t("opening.until.note"));
		if (!run_form(hwnd_, t("menu.opening.settings"), f)) break;
		opening_width_ = std::stoi(f[0].text);
		opening_until_ = std::stoi(f[1].text);
		break;
	}
	case cmd::PlayClock:
		if (auto n = number("menu.play.clock", "play.clock", clock_minutes_, 0, 600, "play.clock.note")) clock_minutes_ = *n;
		break;
	case cmd::PlayPonder:
		ponder_ = !ponder_;
		if (!ponder_ && com_pondering_) {
			send(3, "stop\n");
			com_pondering_ = false;
		}
		com_think();
		break;
	case cmd::PlayKeepHash:
		// the setboard of each move keeps the hash table when it continues the game: what the pondering found
		// helps the next move (newedax from v4.5.5-newedax.14; up to newedax.13 the table was cleaned at each one)
		keep_hash_ = !keep_hash_;
		if (eng_[3].alive) send(3, keep_hash_ ? "keep-hash on\n" : "keep-hash off\n");
		break;
	case cmd::PlayEndSettings: {
		std::vector<FormField> f(2);
		f[0].kind = FormField::Check;
		f[0].label = t("play.end.keep");
		f[0].checked = end_keep_;
		f[1].kind = FormField::Check;
		f[1].label = t("play.end.list");
		f[1].checked = end_list_;
		if (!run_form(hwnd_, t("menu.play.end"), f)) break;
		end_keep_ = f[0].checked;
		end_list_ = f[1].checked;
		break;
	}
	default: break;
	}
	update_menu();
	invalidate();
}

// ---- the compare mode

void App::set_compare(bool on)
{
	compare_ = on;
	EngineSlot &s = eng_[4];
	if (!on) {
		s.proc.close(0);
		s.alive = false;
		cmp_busy_ = cmp_accept_ = false;
	} else if (!s.alive || !s.proc.running()) {
		const EngineKind k = other_kind();
		const std::wstring exe = engine_exe(k);
		if (!engine_files_ok(k)) {
			compare_ = false;
		} else if (const auto nb = nobook_args(L"nobook-cmp.dat"); !nb) {
			log(lang::f("err.engine_name", {temp_dir_}));
			compare_ = false;
		} else if (const auto common = engine_args(k); !common) {
			compare_ = false;
		} else {
			const std::wstring args = *common + *nb;
			++s.generation;
			s.lost = false;
			std::wstring err;
			s.alive = s.proc.start(exe, args, engine_dir(k), hwnd_, WM_ENGINE, engine_tag(4, s.generation), &err);
			if (!s.alive) {
				log(lang::f("log.start_failed", {exe, err}));
				compare_ = false;
			} else {
				send(4, "mode 3\n");
				engine_level_[4] = level_;
				cmp_busy_ = cmp_accept_ = false;
				log(lang::f("compare.started", {lang::t(k == EngineKind::Newedax ? "engine.newedax" : "engine.nikque"), exe}));
			}
		}
	}
	update_menu();
	compare_request();
	invalidate();
}

void App::compare_request()
{
	if (!compare_ || !eng_[4].alive || !show_eval_ || editing_) return;
	const Position &p = game_.now();
	if (p.game_over()) return;
	const PosKey key = key_of(p);
	PosInfo &info = cache_.get(p);
	if (info.cmp_done && info.cmp_level == level_) {
		if (cmp_busy_ && !(cmp_pos_ == key)) stop_search(4);
		return;
	}
	if (cmp_busy_ && cmp_pos_ == key && cmp_level_ == level_) return;
	std::string out;
	if (cmp_busy_) {
		out += "stop\n";
		forget_engine_state(4);
	}
	if (engine_level_[4] != level_) {
		out += "level " + std::to_string(level_) + "\n";
		engine_level_[4] = level_;
	}
	if (info.cmp_level != level_) {
		for (auto &m : info.moves) m.cmp_searched = false;
		info.cmp_done = false;
	}
	out += "setboard " + p.to_board_string() + "\n";
	// markers around the hint, as for the search engine: lines before the first belong to a search stopped
	cmp_marker_id_ = new_id();
	out += "echo M" + std::to_string(cmp_marker_id_) + "\nhint 60\n";
	cmp_end_id_ = new_id();
	out += "echo M" + std::to_string(cmp_end_id_) + "\n";
	cmp_busy_ = true;
	cmp_accept_ = false;
	cmp_pos_ = key;
	cmp_level_ = level_;
	send(4, out);
}

void App::compare_line(const std::string &line)
{
	if (const int mark = echo_marker(line); mark >= 0) {
		if (mark == cmp_marker_id_) {
			cmp_accept_ = true;
		} else if (mark == cmp_end_id_ && cmp_busy_) {
			PosInfo &info = cache_.get(position_of(cmp_pos_));
			info.cmp_done = true;
			info.cmp_level = cmp_level_;
			cmp_busy_ = cmp_accept_ = false;
			invalidate();
		}
		return;
	}
	if (!cmp_accept_) return;
	if (auto h = parse_hint_line(line); h && !h->book && !h->pv.empty()) {
		PosInfo &info = cache_.get(position_of(cmp_pos_));
		if (MoveEval *m = info.find(h->pv[0])) {
			m->cmp_searched = true;
			m->cmp_score = h->score;
			m->cmp_depth = h->depth;
			m->cmp_selectivity = h->selectivity;
			m->cmp_bound = h->bound;
		}
		if (cmp_pos_ == key_of(game_.now())) invalidate();
	}
}

// the timers of this file (from the window's message loop)
bool play_timer(App &app, UINT_PTR id)
{
	switch (id) {
	case kTimerCom: app.com_timer(); return true;
	case kTimerClock: app.clock_tick(); return true;
	case kTimerLoadBox:  // (one time: a box is not shown while the lines of the engine are read)
		app.kill_timer(kTimerLoadBox);
		if (!app.load_box_.empty()) app.message(std::exchange(app.load_box_, std::wstring()), MB_ICONWARNING);
		return true;
	case kTimerNextGame:
		app.kill_timer(kTimerNextGame);
		if (app.play_mode_ == PlayMode::Engines && app.engine_games_left_ > 0) {
			--app.engine_games_left_;
			app.game_end_done_ = false;
			app.new_play_game(false);
			app.com_think();
		}
		return true;
	default: return false;
	}
}

// the clocks of a game for the bar ("Black 4:59  White 5:00"), empty when there is no game or no time
std::wstring play_bar_text(const App &app)
{
	if (app.play_mode_ == PlayMode::Free) return {};
	std::wstring s = play_mode_name(app.play_mode_);
	if (app.com_thinking_ || app.com_book_move_ != kNoMove) s += L" " + lang::t("play.thinking");
	if (app.game_clock_) {
		long long left[2] = {app.clock_left_ms_[0], app.clock_left_ms_[1]};
		if (app.clock_side_ >= 0)
			left[app.clock_side_] -= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - app.clock_t0_).count();
		s += L"  " + lang::f("play.clocks", {clock_text(left[0]), clock_text(left[1])});
	}
	return s;
}

}  // namespace ng
