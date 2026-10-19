// The engines: the main one reads the book (book analyses), the search one reads no book (hint).
// A request is sent once per position; answers are kept in the cache, so a position seen again is shown at once.
#include <intrin.h>

#include <cstdio>

#include "app.h"
#include "lang.h"
#include "measure.h"

namespace ng {

namespace {

bool file_exists(const std::wstring &p)
{
	const DWORD a = GetFileAttributesW(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring absolute(const std::wstring &base, const std::string &p)
{
	const std::wstring w = widen(p);
	if (w.size() >= 2 && (w[1] == L':' || (w[0] == L'\\' && w[1] == L'\\'))) return w;
	return base + L"\\" + w;
}

// 4: AVX-512 (x86-64-v4), 3: AVX2 (x86-64-v3), 1: other
int cpu_level()
{
	int r[4];
	__cpuid(r, 0);
	const int max_leaf = r[0];
	__cpuid(r, 1);
	const bool osxsave = (r[2] >> 27) & 1, avx = (r[2] >> 28) & 1, fma = (r[2] >> 12) & 1, movbe = (r[2] >> 22) & 1;
	const bool popcnt = (r[2] >> 23) & 1, f16c = (r[2] >> 29) & 1;
	if (!osxsave || !avx || max_leaf < 7) return 1;
	const unsigned long long xcr0 = _xgetbv(0);
	if ((xcr0 & 6) != 6) return 1;
	__cpuidex(r, 7, 0);
	const unsigned b = static_cast<unsigned>(r[1]);
	const bool avx2 = (b >> 5) & 1, bmi1 = (b >> 3) & 1, bmi2 = (b >> 8) & 1;
	int e[4];
	__cpuid(e, static_cast<int>(0x80000001));
	const bool lzcnt = (e[2] >> 5) & 1;
	if (!(avx2 && bmi1 && bmi2 && fma && movbe && popcnt && f16c && lzcnt)) return 1;
	const bool avx512 = ((b >> 16) & 1) && ((b >> 17) & 1) && ((b >> 28) & 1) && ((b >> 30) & 1) && ((b >> 31) & 1);
	if (avx512 && (xcr0 & 0xe6) == 0xe6) return 4;
	return 3;
}

}  // namespace

std::wstring App::engine_dir() const
{
	const char *sec = kind_ == EngineKind::Newedax ? "newedax" : "nikque18";
	const std::string d = ini_.get(sec, "dir");
	if (d.empty()) return exe_dir_ + L"\\engines\\" + widen(sec);
	return absolute(exe_dir_, d);
}

std::wstring App::engine_name() const
{
	return lang::t(kind_ == EngineKind::Newedax ? "engine.newedax" : "engine.nikque");
}

std::string App::book_params() const
{
	std::string s = "base=" + base_ + " stages=" + stages_;
	if (umi_ == UmiMode::Best) s += " umi=best";
	else if (umi_ == UmiMode::Loss) s += " umi=loss x=" + std::to_string(loss_x_) + " y=" + std::to_string(loss_y_);
	return s;
}

void App::send(int role, const std::string &text)
{
	if (text.empty() || !eng_[role].alive) return;
	if (!eng_[role].proc.send(text)) {
		eng_[role].alive = false;
		log(lang::f("log.engine_lost", {lang::t(role ? "engine.search" : "engine.main")}));
	}
}

void App::start_engines()
{
	const char *sec = kind_ == EngineKind::Newedax ? "newedax" : "nikque18";
	const std::wstring dir = engine_dir();
	std::wstring exe;
	const std::string e = ini_.get(sec, "exe", "auto");
	if (e.empty() || iequals(e, "auto")) {
		const int lv = cpu_level();
		std::vector<std::wstring> names;
		if (lv >= 4) names.push_back(L"wEdax-x86-64-v4.exe");
		if (lv >= 3) names.push_back(L"wEdax-x86-64-v3.exe");
		names.push_back(L"wEdax-x86-64.exe");
		for (const auto &n : names)
			if (file_exists(dir + L"\\" + n)) {
				exe = dir + L"\\" + n;
				break;
			}
	} else {
		exe = absolute(dir, e);
	}
	const std::string ev = ini_.get(sec, "eval");
	const std::wstring eval = ev.empty() ? dir + L"\\data\\eval.dat" : absolute(dir, ev);
	const std::string bk = ini_.get(sec, "book");
	const std::wstring book = bk.empty() ? dir + L"\\data\\book.dat" : absolute(dir, bk);
	int n = threads_;
	if (n <= 0) n = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));

	if (exe.empty() || !file_exists(exe)) {
		log(lang::f("log.no_exe", {exe.empty() ? dir : exe}));
		return;
	}
	if (!file_exists(eval)) {
		log(lang::f("log.no_eval", {eval}));
		return;
	}
	bool main_ok = true;
	if (kind_ == EngineKind::Nikque18) {
		// nikque.18 cannot read the book form 2 of newedax ("NEDXBOOK")
		FILE *f = nullptr;
		if (_wfopen_s(&f, book.c_str(), L"rb") == 0 && f) {
			char head[8] = {};
			const size_t got = fread(head, 1, 8, f);
			fclose(f);
			if (got == 8 && std::string_view(head, 8) == "NEDXBOOK") {
				MessageBoxW(hwnd_, lang::t("err.book_form2").c_str(), lang::t("app.title").c_str(), MB_ICONWARNING);
				main_ok = false;
			}
		}
	}
	const std::wstring common = L"-q -eval-file \"" + eval + L"\" -n-tasks " + std::to_wstring(n) + L" -level " +
	                            std::to_wstring(level_);
	for (int role = 0; role < 2; ++role) {
		EngineSlot &s = eng_[role];
		if (s.proc.running()) continue;
		if (role == 0 && !main_ok) continue;
		if (role == 1 && !search_engine_) continue;
		const std::wstring args =
			role == 0 ? common + L" -book-file \"" + book + L"\""
			          : common + L" -book-file \"" + temp_dir_ + L"\\nobook.dat\" -book-usage off";
		std::wstring err;
		++s.generation;
		s.alive = s.proc.start(exe, args, dir, hwnd_, WM_ENGINE, static_cast<WPARAM>(role) | (static_cast<WPARAM>(s.generation) << 1), &err);
		s.loaded = false;
		s.board.reset();
		engine_level_[role] = level_;
		if (!s.alive) {
			log(lang::f("log.start_failed", {exe, err}));
			continue;
		}
		send(role, "mode 3\n");
		log(lang::f("log.engine_started", {lang::t(role ? "engine.search" : "engine.main"), exe,
		                                   role == 0 ? book : lang::t("log.no_book")}));
	}
	if (!eng_[0].alive) book_id_ = count_id_ = levels_id_ = 0;
	if (!eng_[1].alive) hint_busy_ = false;
}

void App::stop_engines()
{
	for (auto &s : eng_) {
		s.proc.close();
		s.alive = false;
		s.board.reset();
	}
	requests_.clear();
	book_id_ = count_id_ = levels_id_ = 0;
	graph_id_ = 0;
	hint_busy_ = hint_accept_ = hint_background_ = false;
	DeleteFileW((temp_dir_ + L"\\nobook.dat").c_str());
}

void App::restart_engines()
{
	stop_engines();
	cache_.clear_book();
	cache_.clear_search();
	start_engines();
	position_changed();
}

void App::request_levels()
{
	request_analysis();
}

// The book analyses of the other positions of the game, for the points of the graph: one at a time and only
// while the position shown has nothing waiting, so that it is never kept waiting by them.
void App::pump_graph()
{
	if (!eng_[0].alive || !use_book_ || !show_eval_ || graph_id_ || book_id_) return;
	const std::string params = book_params();
	for (int i = 0; i <= game_.size(); ++i) {
		const Position &p = game_.at(i);
		if (p.game_over() || p.must_pass()) continue;
		PosInfo &info = cache_.get(p);
		if (info.book_params != params) {
			if (info.book_begun || info.book_done || info.count_done || info.levels_done) info.reset_book();
			info.book_params = params;
		}
		if (info.book_done) continue;
		const PosKey key = key_of(p);
		std::string out;
		if (eng_[0].board != key) {
			out += "setboard " + p.to_board_string() + "\n";
			eng_[0].board = key;
		}
		graph_id_ = next_id_++;
		out += "book moveinfo id=" + std::to_string(graph_id_) + " " + params +
		       (umi_ == UmiMode::Off ? " parts=stage\n" : " parts=stage,umi\n");
		requests_[graph_id_] = {Request::Graph, key, 0};
		send(0, out);
		return;
	}
}

void App::request_analysis()
{
	const Position &p = game_.now();
	const PosKey key = key_of(p);
	const bool want = show_eval_ && !p.game_over();

	// ---- main engine: the book analyses of the position
	if (eng_[0].alive) {
		std::string out;
		auto cancel = [&](int &id, bool keep) {
			if (!id || keep) return;
			out += "stop id=" + std::to_string(id) + "\n";
			requests_.erase(id);
			id = 0;
		};
		const bool book_on = want && use_book_;
		auto same = [&](int id) { return requests_.count(id) && requests_[id].pos == key; };
		PosInfo *info = book_on ? &cache_.get(p) : nullptr;
		const std::string params = book_params();
		bool params_changed = false;
		if (info && info->book_params != params) {
			if (info->book_begun || info->book_done || info->count_done || info->levels_done) info->reset_book();
			info->book_params = params;
			params_changed = true;
		}
		cancel(book_id_, book_on && same(book_id_) && !params_changed);
		cancel(count_id_, book_on && count_ && same(count_id_));
		cancel(levels_id_, book_on && tab_ == Tab::Book && same(levels_id_));
		if (info) {
			auto board = [&] {
				if (eng_[0].board != key) {
					out += "setboard " + p.to_board_string() + "\n";
					eng_[0].board = key;
				}
			};
			if (!info->book_done && !book_id_) {
				board();
				book_id_ = next_id_++;
				out += "book moveinfo id=" + std::to_string(book_id_) + " " + params +
				       (umi_ == UmiMode::Off ? " parts=stage\n" : " parts=stage,umi\n");
				requests_[book_id_] = {Request::Book, key, 0};
			}
			if (count_ && !info->count_done && !count_id_) {
				board();
				count_id_ = next_id_++;
				out += "book moveinfo id=" + std::to_string(count_id_) + " parts=count\n";
				requests_[count_id_] = {Request::Count, key, 0};
			}
			if (tab_ == Tab::Book && !info->levels_done && !levels_id_) {
				board();
				levels_id_ = next_id_++;
				out += "book levels id=" + std::to_string(levels_id_) + "\n";
				requests_[levels_id_] = {Request::Levels, key, 0};
			}
		}
		send(0, out);
		pump_graph();
	}

	// ---- search engine: the values of all the moves (hint)
	if (eng_[1].alive) {
		PosInfo *info = want ? &cache_.get(p) : nullptr;
		const bool need = info && !(info->search_done && info->search_level == level_);
		if (!need) {
			// a search of another position is stopped, unless it is the background work on the game
			const bool keep = hint_background_ && backward_ && show_eval_;
			if (hint_busy_ && !keep) {
				send(1, "stop\n");
				hint_busy_ = hint_accept_ = false;
			}
			pump_search();
		} else if (!(hint_busy_ && hint_pos_ == key && hint_level_ == level_)) {
			send_hint(p, false);
		} else {
			hint_background_ = false;  // the background search reached the position shown
		}
	}
}

void App::send_hint(const Position &p, bool background)
{
	const PosKey key = key_of(p);
	std::string out;
	if (hint_busy_) out += "stop\n";  // the commands sent after it are kept
	if (engine_level_[1] != level_) {
		out += "level " + std::to_string(level_) + "\n";
		engine_level_[1] = level_;
	}
	PosInfo &info = cache_.get(p);
	if (info.search_level != level_) info.reset_search();
	if (eng_[1].board != key) {
		out += "setboard " + p.to_board_string() + "\n";
		eng_[1].board = key;
	}
	// the answer of this marker comes after every line of the search stopped above
	marker_id_ = next_id_++;
	out += "book moveinfo id=" + std::to_string(marker_id_) + " parts=\n";
	out += "hint 60\n";
	// and the answer of this one comes when the hint has ended
	hint_end_id_ = next_id_++;
	out += "book moveinfo id=" + std::to_string(hint_end_id_) + " parts=\n";
	hint_busy_ = true;
	hint_accept_ = false;
	hint_background_ = background;
	hint_pos_ = key;
	hint_level_ = level_;
	send(1, out);
}

// The positions of the game, from the end backward, are searched while the search engine has nothing else to
// do: their values go back to the moves before them (backward analysis) and fill the graph.
void App::pump_search()
{
	if (!eng_[1].alive || hint_busy_ || !backward_ || !show_eval_) return;
	for (int i = game_.size(); i >= 0; --i) {
		const Position &p = game_.at(i);
		if (p.game_over() || p.must_pass()) continue;
		const PosInfo *info = cache_.peek(p);
		if (info && info->search_done && info->search_level == level_) continue;
		send_hint(p, true);
		return;
	}
}

void App::on_engine(WPARAM tag, bool ended)
{
	const int role = static_cast<int>(tag & 1);
	EngineSlot &s = eng_[role];
	if (static_cast<unsigned>(tag >> 1) != s.generation) return;  // from a process closed before
	std::vector<std::string> lines;
	s.proc.take_lines(lines);
	bool changed = false;
	const PosKey here = key_of(game_.now());

	for (const std::string &line : lines) {
		if (line.empty()) continue;
		if (line[0] == '@') {
			const Fields f(line);
			const int id = static_cast<int>(f.num("id").value_or(-1));
			if (role == 1) {
				if (f.kind() == "begin" && id == marker_id_) {
					hint_accept_ = true;
					s.loaded = true;
					measure::engine_answer(1);
				}
				if (f.kind() == "begin" && id == hint_end_id_ && hint_busy_) {
					// the hint has ended: a move without a line (an engine that gave fewer) is not asked again
					PosInfo &info = cache_.get(Position{hint_pos_.black, hint_pos_.white, hint_pos_.side});
					info.search_done = true;
					info.search_level = hint_level_;
					hint_busy_ = hint_accept_ = false;
					changed = true;
				}
				continue;
			}
			if (!s.loaded) {
				s.loaded = true;
				changed = true;
				measure::engine_answer(0);
			}
			if (f.kind() == "error") {
				log(lang::t("engine.main") + L": " + widen(line));
				continue;
			}
			auto it = requests_.find(id);
			if (it == requests_.end()) continue;  // stopped before
			const Request req = it->second;
			PosInfo &info = cache_.get(Position{req.pos.black, req.pos.white, req.pos.side});
			if (f.kind() == "begin") {
				info.book_begun = true;
				info.in_book = f.num("book").value_or(0) == 1;
			} else if (f.kind() == "move") {
				apply_move_line(info, f);
			} else if (f.kind() == "level") {
				info.levels.emplace_back(static_cast<int>(f.num("level").value_or(0)), f.num("count").value_or(0));
			} else if (f.kind() == "end" || f.kind() == "cancel") {
				const bool done = f.kind() == "end";
				if (done && f.num("error").value_or(0)) log(lang::f("log.analysis_error", {widen(line)}));
				if (req.kind == Request::Book) {
					info.book_done = done;
					if (book_id_ == id) book_id_ = 0;
				} else if (req.kind == Request::Graph) {
					info.book_done = done;
					changed = true;  // a point of the graph
					if (graph_id_ == id) graph_id_ = 0;
				} else if (req.kind == Request::Count) {
					info.count_done = done;
					if (count_id_ == id) count_id_ = 0;
				} else if (req.kind == Request::Levels) {
					info.levels_done = done;
					if (done) info.levels_positions = f.num("positions").value_or(-1);
					else info.levels.clear();
					if (levels_id_ == id) levels_id_ = 0;
				}
				requests_.erase(it);
			}
			if (req.pos == here) changed = true;
			continue;
		}
		if (role == 1) {
			if (!hint_accept_) continue;  // lines of a search stopped before
			if (auto h = parse_hint_line(line); h && !h->book) {
				PosInfo &info = cache_.get(Position{hint_pos_.black, hint_pos_.white, hint_pos_.side});
				if (MoveEval *m = info.find(h->pv[0])) {
					m->searched = true;
					m->score = h->score;
					m->depth = h->depth;
					m->selectivity = h->selectivity;
					m->bound = h->bound;
					m->pv = std::move(h->pv);
				}
				int done = 0;
				for (const auto &m : info.moves) done += m.searched;
				if (done == static_cast<int>(info.moves.size())) {
					info.search_done = true;
					info.search_level = hint_level_;
					hint_busy_ = hint_accept_ = false;
				}
				changed = true;  // the position shown, or a point of the graph
				continue;
			}
		}
		// other lines of the engines go to the log (not the prompts and the search lines)
		if (line[0] == '<' || parse_hint_line(line)) continue;
		log(lang::t(role ? "engine.search" : "engine.main") + L": " + widen(line));
	}
	if (ended && s.alive) {
		s.alive = false;
		log(lang::f("log.engine_ended", {lang::t(role ? "engine.search" : "engine.main")}));
		if (role == 0) book_id_ = count_id_ = levels_id_ = 0;
		else hint_busy_ = hint_accept_ = false;
		changed = true;
	}
	if (role == 0) pump_graph();
	else if (!hint_busy_) pump_search();
	if (changed) {
		update_animation_timer();
		invalidate();
	}
}

}  // namespace ng
