// The engines: the main one reads the book (book analyses), the search one reads no book (hint).
// A request is sent once per position; answers are kept in the cache, so a position seen again is shown at once.
#include <windows.h>

#include <commdlg.h>
#include <intrin.h>

#include <cstdio>

#include "app.h"
#include "lang.h"
#include "measure.h"
#include "paths.h"

namespace ng {

namespace {

bool file_exists(const std::wstring &p)
{
	const DWORD a = GetFileAttributesW(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool dir_exists(const std::wstring &p)
{
	const DWORD a = GetFileAttributesW(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
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

std::wstring App::engine_dir(EngineKind k) const
{
	const char *sec = engine_section(k);
	const std::string d = ini_.get(sec, "dir");
	if (!d.empty()) return absolute(exe_dir_, d);
	const std::wstring dir = exe_dir_ + L"\\engines\\" + widen(sec);
	// (the folder of the C version in a distribution of 0.2.0 or before)
	if (k == EngineKind::Nikque18 && !dir_exists(dir) && dir_exists(exe_dir_ + L"\\engines\\nikque18")) return exe_dir_ + L"\\engines\\nikque18";
	return dir;
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
	else if (umi_ == UmiMode::Range) s += " umi=range lo=" + std::to_string(umi_lo_) + " hi=" + std::to_string(umi_hi_);
	else s += " umi=off";
	if (umi_depth_ != 60 && umi_ != UmiMode::Off) s += " depth=" + std::to_string(umi_depth_);
	// the parts of book moveinfo: the precision stages can be left out (the values of the book come anyway)
	std::string parts;
	if (precision_) parts = "stage";
	if (umi_ != UmiMode::Off) parts += parts.empty() ? "umi" : ",umi";
	return s + " parts=" + parts;
}

void App::send(int role, const std::string &text)
{
	if (text.empty() || !eng_[role].alive) return;
	if (!eng_[role].proc.send(text)) {
		eng_[role].alive = false;
		log(lang::f("log.engine_lost", {lang::t(role ? "engine.search" : "engine.main")}));
	}
}

std::wstring App::engine_exe(EngineKind k) const
{
	const char *sec = engine_section(k);
	const std::wstring dir = engine_dir(k);
	const std::string e = ini_.get(sec, "exe", "auto");
	if (!e.empty() && !iequals(e, "auto")) return absolute(dir, e);
	const int lv = cpu_level();
	std::vector<std::wstring> names;
	if (lv >= 4) names.push_back(L"wEdax-x86-64-v4.exe");
	if (lv >= 3) names.push_back(L"wEdax-x86-64-v3.exe");
	names.push_back(L"wEdax-x86-64.exe");
	for (const auto &n : names)
		if (file_exists(dir + L"\\" + n)) return dir + L"\\" + n;
	return {};
}

std::wstring App::engine_eval(EngineKind k) const
{
	const char *sec = engine_section(k);
	const std::string ev = ini_.get(sec, "eval");
	return ev.empty() ? engine_dir(k) + L"\\data\\eval.dat" : absolute(engine_dir(k), ev);
}

std::optional<std::wstring> App::engine_args(EngineKind k)
{
	const std::wstring eval = engine_eval(k);
	const auto e = engine_arg_path(eval, EngineName::File);
	if (!e) {
		log(lang::f("err.engine_name", {eval}));
		return std::nullopt;
	}
	int n = threads_;
	if (n <= 0) n = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
	std::wstring a = L"-q -eval-file \"" + *e + L"\" -n-tasks " + std::to_wstring(n) + L" -level " + std::to_wstring(level_);
	if (hash_bits_ > 0) a += L" -hash-table-size " + std::to_wstring(hash_bits_);
	return a;
}

std::optional<std::wstring> App::nobook_args(const wchar_t *name) const
{
	// a name the engine cannot keep would be ignored, and the engine would read its own book (data\book.dat)
	const auto f = engine_arg_path(temp_dir_ + L"\\" + name, EngineName::Book);
	if (!f) return std::nullopt;
	return L" -book-file \"" + *f + L"\" -book-usage off";
}

int App::new_id()
{
	next_id_ = next_request_id(next_id_, [this](int id) {
		if (requests_.count(id) || stopped_.count(id)) return true;
		for (const int x : {marker_id_, hint_end_id_, cmp_marker_id_, cmp_end_id_, com_marker_id_, learn_end_id_, tool_end_id_,
		                    eng_[0].ping_id, eng_[1].ping_id, eng_[0].probe_id})
			if (x == id) return true;
		return false;
	});
	return next_id_;
}

void App::forget_main_requests()
{
	requests_.clear();
	stopped_.clear();
	book_id_ = count_id_ = levels_id_ = best_id_ = graph_id_ = missing_id_ = findline_id_ = list_book_id_ = 0;
	missing_ = MissingList{};
	cancel_reduce(nullptr);  // the count of book reduce goes with them (its answer may have come: it is for that book)
}

void App::cancel_reduce(std::string *out)
{
	++reduce_epoch_;
	if (reduce_.state == ReduceJob::None) return;
	if (reduce_.id) {  // still counted
		if (out) stop_request(*out, reduce_.id);
		reduce_.id = 0;
		if (hwnd_) PostMessageW(hwnd_, WM_REDUCE, 0, 0);  // (a script waits no longer)
	}
	reduce_.state = ReduceJob::Stopped;  // (an answer that has come is not taken: WM_REDUCE is on its way)
}

std::string line_commands(const Position &start, const std::string &moves)
{
	std::string out = "setboard " + start.to_board_string() + "\n";
	if (!moves.empty()) out += "play " + moves + "\n";
	return out;
}

std::string App::moves_to_cursor() const
{
	std::string s;
	for (int i = 0; i < game_.cursor(); ++i) s += square_name(game_.moves()[i]);
	return s;
}

void App::board_to(int role, std::string &out, const Position &p)
{
	const PosKey key = key_of(p);
	if (eng_[role].board == key) return;
	out += "setboard " + p.to_board_string() + "\n";
	eng_[role].board = key;
}

void App::stop_request(std::string &out, int &id)
{
	if (!id) return;
	out += "stop id=" + std::to_string(id) + "\n";
	requests_.erase(id);
	stopped_.insert(id);  // until its "@end" or "@cancel"
	id = 0;
}

std::string App::stop_main_analyses()
{
	std::string out;
	if (missing_id_) missing_.asked = false;  // asked again
	for (int *id : {&book_id_, &count_id_, &levels_id_, &best_id_, &graph_id_, &missing_id_}) stop_request(out, *id);
	return out;
}

void App::start_engines()
{
	const std::wstring dir = engine_dir();
	const std::wstring exe = engine_exe();
	const std::wstring eval = engine_eval();
	const std::wstring book = book_file();

	if (exe.empty() || !file_exists(exe)) {
		log(lang::f("log.no_exe", {exe.empty() ? dir : exe}));
		return;
	}
	if (!file_exists(eval)) {
		log(lang::f("log.no_eval", {eval}));
		return;
	}
	const auto common = engine_args();
	if (!common) return;
	bool main_ok = true;
	// a book name the engine does not keep is ignored by it, and its own book (data\book.dat) is read instead
	const auto book_arg = engine_arg_path(book, EngineName::Book);
	eng_[0].error.clear();
	if (!book_arg && !eng_[0].proc.running()) {
		eng_[0].error = lang::f("err.book_name", {book});
		log(eng_[0].error);
		main_ok = false;
	}
	if (kind_ == EngineKind::Nikque18) {
		// nikque.18 cannot read the book form 2 of newedax ("NEDXBOOK")
		FILE *f = nullptr;
		if (_wfopen_s(&f, book.c_str(), L"rb") == 0 && f) {
			char head[8] = {};
			const size_t got = fread(head, 1, 8, f);
			fclose(f);
			if (got == 8 && std::string_view(head, 8) == "NEDXBOOK") {
				message(lang::t("err.book_form2"), MB_ICONWARNING);
				main_ok = false;
			}
		}
	}
	for (int role = 0; role < 2; ++role) {
		EngineSlot &s = eng_[role];
		if (s.proc.running()) continue;
		if (role == 0 && !main_ok) continue;
		if (role == 1 && !search_engine_) continue;
		std::wstring args = *common;
		if (role == 0) {
			args += L" -book-file \"" + *book_arg + L"\"";
		} else if (const auto nb = nobook_args(L"nobook.dat")) {
			args += *nb;
		} else {
			log(lang::f("err.engine_name", {temp_dir_}));
			continue;
		}
		std::wstring err;
		++s.generation;
		s.alive = s.proc.start(exe, args, dir, hwnd_, WM_ENGINE, engine_tag(role, s.generation), &err);
		s.loaded = false;
		s.board.reset();
		s.open.clear();
		s.book_arg = role == 0 ? *book_arg : std::wstring();
		engine_level_[role] = level_;
		if (!s.alive) {
			log(lang::f("log.start_failed", {exe, err}));
			continue;
		}
		s.old = false;
		s.probe_id = 0;
		if (role == 0) {
			// first: an engine with the book analyses answers "@cancel id=<n> state=none" at once, before the version;
			// an engine without them reads a "stop" with nothing to drop yet
			s.probe_id = new_id();
			send(0, "stop id=" + std::to_string(s.probe_id) + "\n");
		}
		send(role, "mode 3\n");
		if (role == 0) send(0, learn_options_commands());  // the settings of book learning
		s.version.clear();
		s.ping_id = 0;
		s.ping_sent = std::chrono::steady_clock::now();
		if (role == 0) send(0, "version\n");
		log(lang::f("log.engine_started", {lang::t(role ? "engine.search" : "engine.main"), exe,
		                                   role == 0 ? book : lang::t("log.no_book")}));
	}
	if (!eng_[0].alive) forget_main_requests();
	if (!eng_[1].alive) hint_busy_ = false;
}

void App::stop_engines()
{
	for (auto &s : eng_) {
		s.proc.close();
		s.alive = false;
		s.board.reset();
	}
	forget_main_requests();
	learning_ = false;
	hint_busy_ = hint_accept_ = hint_background_ = false;
	tool_busy_ = false;
	com_thinking_ = com_pondering_ = com_waiting_book_ = com_accept_ = false;
	com_book_move_ = kNoMove;
	cmp_busy_ = cmp_accept_ = false;
	DeleteFileW((temp_dir_ + L"\\nobook.dat").c_str());
	DeleteFileW((temp_dir_ + L"\\nobook-tool.dat").c_str());
}

bool App::refuse_while_learning()
{
	if (!learning_) return false;
	message(lang::t("learn.busy"), MB_ICONINFORMATION);
	return true;
}

void App::restart_engines()
{
	// (a learning command takes no input to stop: the engine would be closed in the middle of it)
	if (refuse_while_learning()) return;
	stop_engines();
	cache_.clear_book();
	cache_.clear_search();
	start_engines();
	if (compare_) set_compare(true);  // the other engine of the kind chosen now
	position_changed();
}

// The book analyses of the other positions of the game, for the points of the graph: one at a time and only
// while the position shown has nothing waiting, so that it is never kept waiting by them.
void App::pump_graph()
{
	if (!eng_[0].alive || learning_ || !use_book_ || !show_eval_ || graph_id_ || book_id_ || background_paused_) return;
	const std::string params = book_params();
	auto ask = [&](const Position &p) {
		const PosKey key = key_of(p);
		std::string out;
		board_to(0, out, p);
		graph_id_ = new_id();
		out += "book moveinfo id=" + std::to_string(graph_id_) + " " + params + "\n";
		requests_[graph_id_] = {Request::Graph, key, params};
		send(0, out);
	};
	// the nodes of the tree first while it is shown
	if (tab_ == Tab::Tree && !panel_hidden_) {
		if (const auto p = tree_next_wanted()) {
			ask(*p);
			return;
		}
	}
	for (int i = 0; i <= game_.size(); ++i) {
		const Position &p = game_.at(i);
		if (p.game_over() || p.must_pass()) continue;
		PosInfo &info = cache_.get(p);
		info.set_book_params(params);
		if (info.book_done) continue;
		ask(p);
		return;
	}
}

void App::request_analysis()
{
	const Position &p = game_.now();
	const PosKey key = key_of(p);
	const bool want = show_eval_ && !p.game_over();

	// ---- main engine: the book analyses of the position
	if (eng_[0].alive && !learning_) {  // no analysis while the book is learning
		std::string out;
		auto cancel = [&](int &id, bool keep) {
			if (!keep) stop_request(out, id);
		};
		const bool book_on = want && use_book_;
		auto same = [&](int id) { return requests_.count(id) && requests_[id].pos == key; };
		PosInfo *info = book_on ? &cache_.get(p) : nullptr;
		const std::string params = book_params();
		const bool params_changed = info && info->set_book_params(params);
		cancel(book_id_, book_on && same(book_id_) && !params_changed);
		// a point of the graph asked with other book settings is of no use any more; the position shown goes first:
		// a point of the graph for it becomes its request, one for another position waits while it is not known (the
		// analyses of the engine are done one after the other)
		const auto graph = requests_.find(graph_id_);
		const bool graph_ok = graph != requests_.end() && graph->second.params == params;
		if (graph_ok && info && !info->book_done && !book_id_ && graph->second.pos == key) {
			graph->second.kind = Request::Book;
			book_id_ = graph_id_;
			graph_id_ = 0;
		}
		cancel(graph_id_, graph_ok && !(info && !info->book_done));
		cancel(count_id_, book_on && count_ && same(count_id_));
		cancel(levels_id_, book_on && tab_ == Tab::Book && same(levels_id_));
		cancel(best_id_, book_on && tab_ == Tab::Book && same(best_id_));
		if (info) {
			auto board = [&] { board_to(0, out, p); };
			if (!info->book_done && !book_id_) {
				board();
				book_id_ = new_id();
				out += "book moveinfo id=" + std::to_string(book_id_) + " " + params + "\n";
				requests_[book_id_] = {Request::Book, key, params};
			}
			if (count_ && !info->count_done && !count_id_) {
				board();
				count_id_ = new_id();
				out += "book moveinfo id=" + std::to_string(count_id_) + " parts=count\n";
				requests_[count_id_] = {Request::Count, key};
			}
			if (tab_ == Tab::Book && !info->levels_done && !levels_id_) {
				board();
				levels_id_ = new_id();
				out += "book levels id=" + std::to_string(levels_id_) + "\n";
				requests_[levels_id_] = {Request::Levels, key};
			}
			if (tab_ == Tab::Book && !info->best_done && !best_id_) {
				board();
				best_id_ = new_id();
				out += "book bestlines id=" + std::to_string(best_id_) + "\n";  // loss 0: the best moves only
				requests_[best_id_] = {Request::Best, key};
			}
			request_missing(out, p);
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
			const bool keep = hint_background_ && (backward_ || forward_) && show_eval_ && !background_paused_;
			if (hint_busy_ && !keep) {
				if constexpr (measure::kOn) measure::trace("STOP shown=" + p.to_board_string());
				stop_search(1);
			}
			pump_search();
		} else if (!(hint_busy_ && hint_pos_ == key && hint_level_ == level_)) {
			send_hint(p, false);
		} else {
			hint_background_ = false;  // the background search reached the position shown
		}
	}
	compare_request();
}

// Edax drops the commands it has not read yet when it reads "stop" (not "stop id=", a book analysis): the
// position and the level sent before it may never have been set, so they are sent again after it.
void App::forget_engine_state(int role)
{
	eng_[role].board.reset();
	engine_level_[role] = -1;
}

void App::stop_search(int role)
{
	send(role, "stop\n");
	forget_engine_state(role);
	if (role == 1) hint_busy_ = hint_accept_ = false;
	else cmp_busy_ = cmp_accept_ = false;
}

void App::send_hint(const Position &p, bool background)
{
	const PosKey key = key_of(p);
	std::string out;
	if (hint_busy_) {
		out += "stop\n";  // the commands sent after it are kept
		forget_engine_state(1);
	}
	if (engine_level_[1] != level_) {
		out += "level " + std::to_string(level_) + "\n";
		engine_level_[1] = level_;
	}
	PosInfo &info = cache_.get(p);
	if (info.search_level != level_) info.reset_search();
	board_to(1, out, p);
	// Markers around the hint. "echo M<n>" is answered by the engine's main loop (a warning line naming
	// "M<n>"), in order with the lines of the hint: the first comes after every line of the search stopped
	// above, the second when the hint has ended. (An answer of the book analyses comes from another thread
	// and may come before or after the lines of the hint.)
	marker_id_ = new_id();
	out += "echo M" + std::to_string(marker_id_) + "\n";
	out += "hint 60\n";
	hint_end_id_ = new_id();
	out += "echo M" + std::to_string(hint_end_id_) + "\n";
	hint_busy_ = true;
	hint_accept_ = false;
	hint_background_ = background;
	hint_pos_ = key;
	hint_level_ = level_;
	if constexpr (measure::kOn)
		measure::trace("SEND " + p.to_board_string() + " bg=" + std::to_string(background) + " M" + std::to_string(marker_id_) + " end=M" + std::to_string(hint_end_id_));
	send(1, out);
}

// The positions of the game are searched while the search engine has nothing else to do, from the end backward
// (their values go back to the moves before them: backward analysis) or from the start forward; they fill the graph.
void App::pump_search()
{
	if (!eng_[1].alive || hint_busy_ || !(backward_ || forward_) || !show_eval_ || background_paused_) return;
	const int n = game_.size();
	for (int k = 0; k <= n; ++k) {
		const Position &p = game_.at(forward_ ? k : n - k);
		if (p.game_over() || p.must_pass()) continue;
		const PosInfo *info = cache_.peek(p);
		if (info && info->search_done && info->search_level == level_) continue;
		send_hint(p, true);
		return;
	}
}

void App::on_engine(WPARAM tag, bool ended)
{
	const int role = static_cast<int>(tag & 7);
	if (role > 4) return;
	EngineSlot &s = eng_[role];
	if (static_cast<unsigned>(tag >> 3) != s.generation) return;  // from a process closed before
	std::vector<std::string> lines;
	s.proc.take_lines(lines);
	if (role == 0) unmix_lines(lines, s.open);
	bool changed = false, ask_missing = false;
	const PosKey here = key_of(game_.now());

	if (role == 2) {
		for (const std::string &line : lines)
			if (!line.empty()) tool_line(line);
		if (ended && s.alive) {
			s.alive = false;
			if (tool_busy_) tool_ended(true);
		}
		return;
	}
	if (role == 3 || role == 4) {  // the play engine, the compare engine
		for (const std::string &line : lines) {
			if (line.empty()) continue;
			if (role == 3) play_line(line);
			else compare_line(line);
		}
		if (ended && s.alive) {
			s.alive = false;
			log(lang::f("log.engine_ended", {lang::t(role == 3 ? "engine.play" : "engine.compare")}));
			if (role == 3) com_thinking_ = com_pondering_ = com_accept_ = false;
			else cmp_busy_ = cmp_accept_ = false;
		}
		return;
	}
	for (const std::string &line : lines) {
		if (line.empty()) continue;
		if (line[0] == '@') {
			const Fields f(line);
			const int id = static_cast<int>(f.num("id").value_or(-1));
			if (role == 1) continue;  // the search engine is asked no book analysis
			if (s.probe_id && id == s.probe_id) {
				s.probe_id = 0;  // the book analyses are known
				continue;
			}
			if (!s.loaded) {
				s.loaded = true;
				changed = true;
				measure::engine_answer(0);
			}
			if (f.kind() == "error") {
				if (id > 0 && id == reduce_.id && f.num("damaged").value_or(0)) reduce_.damaged = true;
				log(lang::t("engine.main") + L": " + engine_text(line));
				continue;
			}
			const bool last = f.kind() == "end" || f.kind() == "cancel";  // the last line of an answer
			auto it = requests_.find(id);
			if (it == requests_.end()) {  // stopped before
				if (last) stopped_.erase(id);
				continue;
			}
			const Request req = it->second;
			const bool done = f.kind() == "end";  // (the last line of an answer that has come to its end)
			if (req.kind == Request::FindLine) {
				if (last) requests_.erase(it);  // first: the answer may change the requests
				findline_answer(f, last);
				continue;
			}
			if (req.kind == Request::ReduceCount) {
				if (last) {
					requests_.erase(it);
					reduce_answer(f, done);
				}
				continue;
			}
			if (req.kind == Request::Missing) {
				if (f.kind() == "line" && id == missing_id_) {
					if (auto ml = parse_missing_line(f)) missing_.lines.push_back(std::move(*ml));
				} else if (last) {
					if (id == missing_id_) {
						missing_id_ = 0;
						if (done) {
							missing_.done = true;
							missing_.count = static_cast<int>(f.num("count").value_or(0));
							missing_.stage = static_cast<int>(f.num("stage").value_or(-1));
						} else {
							missing_.asked = false;  // asked again
						}
					}
					requests_.erase(it);
				}
				changed = true;
				continue;
			}
			if (req.kind == Request::ListBook || req.kind == Request::ListBookAdd) {
				if (last) {
					requests_.erase(it);
					list_book_written(req.kind == Request::ListBookAdd, done && !f.num("error").value_or(0));
				}
				continue;
			}
			if (req.kind == Request::BestExport) {
				if (last) {
					if (!done || f.num("error").value_or(0))
						log(lang::f("best.export_failed", {best_file_}));
					else
						log(lang::f("best.written", {std::to_wstring(f.num("written").value_or(0)), best_file_,
						                             std::to_wstring(f.num("lines").value_or(0)),
						                             std::to_wstring(f.num("incomplete_ends").value_or(0))}));
					requests_.erase(it);
				}
				continue;
			}
			PosInfo &info = cache_.get(Position{req.pos.black, req.pos.white, req.pos.side});
			if constexpr (measure::kOn)
				measure::trace("BOOK " + Position{req.pos.black, req.pos.white, req.pos.side}.to_board_string() + " | " + line);
			if ((req.kind == Request::Book || req.kind == Request::Graph) && req.params != info.book_params) {
				// asked with other book settings (changed since): nothing of it is kept
				if (last) {
					if (book_id_ == id) book_id_ = 0;
					if (graph_id_ == id) graph_id_ = 0;
					requests_.erase(it);
				}
				continue;
			}
			if (f.kind() == "begin") {
				info.book_begun = true;
				info.in_book = f.num("book").value_or(0) == 1;
			} else if (f.kind() == "move") {
				apply_move_line(info, f);
			} else if (f.kind() == "level") {
				info.levels.emplace_back(static_cast<int>(f.num("level").value_or(0)), f.num("count").value_or(0));
			} else if (last) {
				if (done && f.num("error").value_or(0)) log(lang::f("log.analysis_error", {engine_text(line)}));
				if (req.kind == Request::Book) {
					info.book_done = done;
					if (book_id_ == id) book_id_ = 0;
					if (done && req.pos == here) ask_missing = true;  // the best book move is known now
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
				} else if (req.kind == Request::Best) {
					info.best_done = done;
					info.best_error = done && f.num("error").value_or(0);
					info.best_lines = f.num("lines").value_or(-1);
					info.best_ends = f.num("ends").value_or(-1);
					info.best_incomplete = f.num("incomplete").value_or(-1);
					info.best_incomplete_ends = f.num("incomplete_ends").value_or(-1);
					if (best_id_ == id) best_id_ = 0;
				}
				requests_.erase(it);
			}
			if (req.pos == here) changed = true;
			continue;
		}
		if (const int mark = echo_marker(line); mark >= 0 && mark == s.ping_id) {
			s.ping_id = 0;  // the engine answers (watch)
			continue;
		}
		if (line.starts_with("Edax version")) {
			// "Edax version 4.5.5 Oct 19 2026 20:05:32 for Windows": the version and the day it was built
			std::string v = line.substr(13);
			if (const size_t f = v.find(" for "); f != std::string::npos) v.resize(f);
			s.version = widen(v);
			if (role == 0 && s.probe_id) {
				s.probe_id = 0;
				s.old = true;
				log(lang::f("log.old_engine", {engine_exe()}));
			}
			changed = true;
			continue;
		}
		if (line.starts_with("copyright")) continue;
		if (role == 1) {
			if (const int mark = echo_marker(line); mark >= 0) {
				if constexpr (measure::kOn) measure::trace("MARK M" + std::to_string(mark) + " start=M" + std::to_string(marker_id_) + " end=M" + std::to_string(hint_end_id_) + " busy=" + std::to_string(hint_busy_));
				if (mark == marker_id_) {
					hint_accept_ = true;
					s.loaded = true;
					measure::engine_answer(1);
				} else if (mark == hint_end_id_ && hint_busy_) {
					// the hint has ended: a move without a line (an engine that gave fewer) is not asked again
					PosInfo &info = cache_.get(Position{hint_pos_.black, hint_pos_.white, hint_pos_.side});
					info.search_done = true;
					info.search_level = hint_level_;
					hint_busy_ = hint_accept_ = false;
					changed = true;
				}
				continue;
			}
			if (!hint_accept_) {
				if constexpr (measure::kOn) measure::trace("SKIP " + line);
				continue;  // lines of a search stopped before
			}
			if (auto h = parse_hint_line(line); h && !h->book) {
				if constexpr (measure::kOn)
					measure::trace("HINT " + Position{hint_pos_.black, hint_pos_.white, hint_pos_.side}.to_board_string() + " | " + line);
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
		if (role == 0) {
			// the end of a learning command ("echo M<n>" after it), and its progress
			if (const int mark = echo_marker(line); mark >= 0) {
				if (learning_ && mark == learn_end_id_) learning_ended();
				continue;
			}
			if (learning_) {
				learning_line(line);
				continue;
			}
		}
		// an engine without "book findline": the line is looked for without the book
		if (role == 0 && findline_id_ && line.starts_with("WARNING") && line.find("findline") != std::string::npos) {
			const std::string end = "@end id=" + std::to_string(findline_id_) + " count=0 unknown=1";
			requests_.erase(findline_id_);
			findline_answer(Fields(end), true);
			continue;
		}
		// an engine without "book reducecount" (and without book reduce): nothing is done
		if (role == 0 && reduce_.id && line.starts_with("WARNING") && line.find("reducecount") != std::string::npos) {
			requests_.erase(reduce_.id);
			reduce_.id = 0;
			reduce_.state = ReduceJob::Unknown;
			PostMessageW(hwnd_, WM_REDUCE, 0, 0);
			continue;
		}
		// other lines of the engines go to the log (not the prompts and the search lines)
		if (line[0] == '<' || parse_hint_line(line)) continue;
		log(lang::t(role ? "engine.search" : "engine.main") + L": " + engine_text(line));
	}
	if (ended && s.alive) {
		s.alive = false;
		log(lang::f("log.engine_ended", {lang::t(role ? "engine.search" : "engine.main")}));
		if (role == 0) {
			forget_main_requests();
			learning_ = false;
			queue_running_ = false;
		} else {
			hint_busy_ = hint_accept_ = false;
		}
		changed = true;
	}
	if (ask_missing && s.alive && !learning_) {
		std::string out;
		request_missing(out, game_.now());
		send(0, out);
	}
	if (role == 0) pump_graph();
	else if (!hint_busy_) pump_search();
	// the computer waits for the book analysis of the position (its opening moves)
	if (role == 0 && com_waiting_book_) {
		const PosInfo *info = cache_.peek(game_.now());
		if (info && info->book_done) {
			com_waiting_book_ = false;
			KillTimer(hwnd_, 4);
			com_think();
		}
	}
	if (changed) {
		update_animation_timer();
		invalidate();
	}
}

}  // namespace ng

namespace ng {

// Another book for the main engine (the menu, the recent books, a file dropped on the window).
void App::switch_book(const std::wstring &path)
{
	if (refuse_while_learning()) return;
	if (message(lang::f("ask.open_book", {path}), MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
		return;
	const char *sec = engine_section(kind_);
	const std::string p = narrow(path);
	ini_.set(sec, "book", p);
	// the recent books, newest first, up to 8
	std::vector<std::string> recent{p};
	for (int i = 1; i <= 8; ++i) {
		const std::string r = ini_.get("books", "recent" + std::to_string(i));
		if (!r.empty() && !iequals(r, p)) recent.push_back(r);
	}
	for (int i = 1; i <= 8; ++i)
		ini_.set("books", "recent" + std::to_string(i), i <= static_cast<int>(recent.size()) ? recent[i - 1] : "");
	save_config();
	build_menu();

	restart_main_engine();
}

// The main engine closed and started again (another book, the book read again): its requests go with it.
void App::restart_main_engine()
{
	eng_[0].proc.close();
	eng_[0].alive = false;
	eng_[0].board.reset();
	forget_main_requests();
	cache_.clear_book();
	start_engines();
	position_changed();
}

// The missing lines as games (the moves from the start), one a line, for a file.
void App::export_missing()
{
	if (missing_.lines.empty()) {
		set_status(lang::t("status.missing_none"));
		return;
	}
	wchar_t file[MAX_PATH] = L"missing.txt";
	OPENFILENAMEW of{sizeof of};
	of.hwndOwner = hwnd_;
	of.lpstrFilter = L"*.txt\0*.txt\0*.*\0*.*\0";
	of.lpstrFile = file;
	of.nMaxFile = MAX_PATH;
	of.lpstrDefExt = L"txt";
	of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	if (!GetSaveFileNameW(&of)) return;
	const std::string head = game_.to_text(true);
	std::string out;
	for (const auto &ml : missing_.lines) {
		out += head;
		for (int mv : ml.moves)
			if (mv != kPass) out += square_name(mv);
		if (ml.leaf != kNoMove && ml.leaf != kPass) out += square_name(ml.leaf);
		out += "\r\n";
	}
	FILE *f = nullptr;
	if (_wfopen_s(&f, file, L"wb") != 0 || !f) {
		set_status(lang::t("status.write_failed"));
		return;
	}
	const bool ok = fwrite(out.data(), 1, out.size(), f) == out.size();
	fclose(f);
	log(ok ? lang::f("status.missing_written", {std::to_wstring(missing_.lines.size()), file}) : lang::t("status.write_failed"));
}

}  // namespace ng

namespace ng {

// the move whose missing leaves are listed: the move chosen, else the best move of the book (only book moves
// have missing leaves; the search values would change the move again and again while they come)
int App::missing_target() const
{
	const Position &p = game_.now();
	if (selected_sq_ != kNoMove && p.can_play(selected_sq_)) return selected_sq_;
	const PosInfo *info = cache_.peek(p);
	if (!info) return kNoMove;
	int best = -999, sq = kNoMove;
	for (const auto &m : info->moves) {
		if (m.sq != kPass && m.in_book && m.book_value > best) {
			best = m.book_value;
			sq = m.sq;
		}
	}
	return sq;
}

void App::request_missing(std::string &out, const Position &p)
{
	if (tab_ != Tab::Book) return;
	const int sq = missing_target();
	if (sq == kNoMove) {
		// no book move here (yet): the list of another position goes
		stop_request(out, missing_id_);
		missing_ = MissingList{};
		return;
	}
	const PosKey key = key_of(p);
	const std::string params = "base=" + base_ + " stages=" + stages_ + " x=" + std::to_string(loss_x_) +
	                           " y=" + std::to_string(loss_y_);
	if (missing_.asked && missing_.pos == key && missing_.sq == sq && missing_.params == params) return;
	stop_request(out, missing_id_);
	missing_ = MissingList{};
	missing_.pos = key;
	missing_.sq = sq;
	missing_.params = params;
	missing_.asked = true;
	missing_scroll_ = 0;
	board_to(0, out, p);
	missing_id_ = new_id();
	out += "book missing id=" + std::to_string(missing_id_) + " sq=" + square_name(sq) + " " + params + " n=500\n";
	requests_[missing_id_] = {Request::Missing, key};
}

}  // namespace ng

namespace ng {

// Every few seconds: an engine with nothing to do is asked an echo marker; one that has not answered it after
// 20 seconds does not answer and is closed and started again. (Busy engines are not asked: a learning command
// or a hint keeps the engine's main loop, and the answer would only come after it.)
void App::watch_engines()
{
	const auto now = std::chrono::steady_clock::now();
	for (int role = 0; role < 2; ++role) {
		EngineSlot &s = eng_[role];
		if (!s.alive || !s.loaded) continue;
		const bool busy = role == 0 ? learning_ : hint_busy_;
		if (s.ping_id) {
			if (busy) {
				s.ping_id = 0;  // busy since: the answer comes later
				continue;
			}
			if (now - s.ping_sent < std::chrono::seconds(20)) continue;
			log(lang::f("log.engine_hung", {lang::t(role ? "engine.search" : "engine.main")}));
			s.ping_id = 0;
			s.proc.close(0);
			s.alive = false;
			s.board.reset();
			if (role == 0) {
				forget_main_requests();
			} else {
				hint_busy_ = hint_accept_ = hint_background_ = false;
			}
			start_engines();  // starts the one that is not running
			position_changed();
			continue;
		}
		if (busy || now - s.ping_sent < std::chrono::seconds(15)) continue;
		s.ping_id = new_id();
		s.ping_sent = now;
		send(role, "echo M" + std::to_string(s.ping_id) + "\n");
	}
}

}  // namespace ng
