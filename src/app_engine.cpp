// The engines: the main one reads the book (book analyses), the search one reads no book (hint).
// A request is sent once per position; answers are kept in the cache, so a position seen again is shown at once.
#include <windows.h>

#include <intrin.h>

#include <cstdio>

#include "app.h"
#include "lang.h"
#include "measure.h"
#include "paths.h"

namespace ng {

namespace {

// the names of the engines in the log, by their roles (eng_)
constexpr const char *kRoleNames[] = {"engine.main", "engine.search", "engine.tool", "engine.play", "engine.compare"};

bool dir_exists(const std::wstring &p)
{
	const DWORD a = GetFileAttributesW(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

// the size of a file, for the log
std::wstring size_text(u64 bytes)
{
	wchar_t s[32];
	if (bytes >= 1u << 30) swprintf_s(s, L"%.1f GB", static_cast<double>(bytes) / (1u << 30));
	else if (bytes >= 1u << 20) swprintf_s(s, L"%.1f MB", static_cast<double>(bytes) / (1u << 20));
	else swprintf_s(s, L"%llu B", bytes);
	return s;
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
		eng_[role].lost = true;  // (the end of its output comes after: on_engine)
		log(lang::f("log.engine_lost", {lang::t(kRoleNames[role])}));
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

bool App::engine_files_ok(EngineKind k)
{
	const std::wstring exe = engine_exe(k);
	if (exe.empty() || !file_exists(exe)) {
		log(lang::f("log.no_exe", {exe.empty() ? engine_dir(k) : exe}));
		return false;
	}
	// (an engine without its eval file ends at once, saying so without ending the line: nothing of it reaches the log)
	const std::wstring eval = engine_eval(k);
	if (!file_exists(eval)) {
		log(lang::f("log.no_eval", {eval}));
		return false;
	}
	return true;
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
	book_id_ = values_id_ = count_id_ = levels_id_ = best_id_ = graph_id_ = missing_id_ = findline_id_ = list_book_id_ = 0;
	missing_ = MissingList{};
	stages_held_ = false;
	cancel_reduce(nullptr);  // the count of book reduce goes with them (its answer may have come: it is for that book)
	book_peek_.stop();
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

int App::ask_main(std::string &out, const Position &p, Request::Kind kind, const char *command, const std::string &args,
                  const std::string &params)
{
	board_to(0, out, p);
	const int id = new_id();
	out += std::string(command) + " id=" + std::to_string(id) + (args.empty() ? "" : " " + args) + "\n";
	requests_[id] = {kind, key_of(p), params};
	return id;
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
	for (int *id : {&book_id_, &values_id_, &count_id_, &levels_id_, &best_id_, &graph_id_, &missing_id_}) stop_request(out, *id);
	return out;
}

void App::start_engines()
{
	const std::wstring dir = engine_dir();
	const std::wstring exe = engine_exe();
	const std::wstring book = book_file();

	if (!engine_files_ok(kind_)) return;
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
	// (not while the main engine runs: it may be saving the book, and a handle on the file makes its replacing fail)
	if (kind_ == EngineKind::Nikque18 && !eng_[0].proc.running()) {
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
		s.lost = false;
		s.alive = s.proc.start(exe, args, dir, hwnd_, WM_ENGINE, engine_tag(role, s.generation), &err);
		s.loaded = false;
		s.board.reset();
		s.open.clear();
		s.book_arg = role == 0 ? *book_arg : std::wstring();
		s.book_changed = false;
		if (role == 0) {
			WIN32_FILE_ATTRIBUTE_DATA attr{};
			book_bytes_.reset();
			if (GetFileAttributesExW(book.c_str(), GetFileExInfoStandard, &attr))
				book_bytes_ = u64{attr.nFileSizeHigh} << 32 | attr.nFileSizeLow;
		}
		engine_level_[role] = level_;
		if (!s.alive) {
			log(lang::f("log.start_failed", {exe, err}));
			continue;
		}
		s.old = false;
		s.exports = false;
		s.probe_id = 0;
		if (role == 0) {
			// first: an engine with the book analyses answers "@cancel id=<n> state=none" at once, before the version;
			// an engine without them reads a "stop" with nothing to drop yet
			s.probe_id = new_id();
			send(0, "stop id=" + std::to_string(s.probe_id) + "\n");
			book_peek_.start(book, hwnd_, WM_BOOK_PEEK, ++book_peek_generation_);
		}
		send(role, "mode 3\n");
		if (role == 0) {  // the settings of book learning (those changed while the last engine learned too)
			learn_options_sent_ = 0;
			send_learn_options();
			learn_options_waiting_ = false;
		}
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

// After a command that may change the book the main engine saves it as it ends ("quit"): it writes the whole book to
// another file, then puts it in the place of the book file (a book of hundreds of millions of positions is tens of GB:
// minutes on a slow drive). It is then not ended: its end is looked for by a timer, the window answering meanwhile (the
// log and the log row say so), and `then` is done after it (end_save). Else it is ended 2 seconds after "quit", as the
// other engines, and `then` is done at once. A learning in progress reads "quit" only after its end: it is not waited
// for. Closed while it saves (the window closed after another book was asked): `then` in the place of what was asked.
// Its requests and its learning go at once.
void App::close_main_engine(std::function<void()> then)
{
	EngineSlot &s = eng_[0];
	if (book_saving_) {
		after_save_ = std::move(then);
		return;
	}
	const bool save = s.alive && s.book_changed && !learning_;
	if (save) s.proc.quit();
	else s.proc.close();
	s.alive = s.lost = false;
	s.board.reset();
	forget_main_requests();
	learning_ = false;
	if (!save) {
		s.book_changed = false;
		if (then) then();
		return;
	}
	book_saving_ = true;
	save_t0_ = std::chrono::steady_clock::now();
	save_shown_s_ = -1;
	after_save_ = std::move(then);
	log(lang::t("log.engine_saving"));
	SetTimer(hwnd_, kTimerSave, 250, nullptr);
	update_menu();
	invalidate();
}

void App::watch_save()
{
	if (!book_saving_) {
		KillTimer(hwnd_, kTimerSave);
		return;
	}
	if (!eng_[0].proc.exited()) {
		const long long sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - save_t0_).count();
		if (sec != save_shown_s_) {  // (the time in the log row)
			save_shown_s_ = sec;
			invalidate();
		}
		return;
	}
	// a box or a small window open on the window (it is disabled then: the question of ask_cut_save too): after it
	if (hwnd_ && !IsWindowEnabled(hwnd_)) return;
	end_save(false);
}

void App::end_save(bool cut)
{
	KillTimer(hwnd_, kTimerSave);
	EngineSlot &s = eng_[0];
	// its last lines first (the save said by the engine, or why it could not save), which close() would drop: it has
	// read them all when it has ended (exited)
	if (!cut) on_engine(engine_tag(0, s.generation), false);
	s.proc.close(0);  // (it has ended, else it is ended now)
	s.book_changed = false;
	book_saving_ = false;
	const auto sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - save_t0_).count();
	log(cut ? lang::t("log.engine_save_cut") : lang::f("log.engine_saved", {std::to_wstring(sec / 60), std::to_wstring(sec % 60)}));
	update_menu();
	invalidate();
	schedule_script();  // (a script waits for the end of the save)
	std::function<void()> then = std::move(after_save_);
	after_save_ = nullptr;
	if (then) then();
}

void App::ask_cut_save()
{
	if (!book_saving_ || (hwnd_ && !IsWindowEnabled(hwnd_))) return;  // (the question, or another box, is open)
	const auto sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - save_t0_).count();
	const int r = message(lang::f("ask.cut_save", {std::to_wstring(sec / 60), std::to_wstring(sec % 60)}),
	                      MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
	if (!book_saving_) return;
	if (eng_[0].proc.exited()) end_save(false);  // (it has ended while asked: the book is saved)
	else if (r == IDYES) end_save(true);
}

bool App::refuse_while_saving()
{
	if (!book_saving_) return false;
	message(lang::t("save.busy"), MB_ICONINFORMATION);
	return true;
}

void App::stop_engines(std::function<void()> then)
{
	book_peek_.stop();  // (the main engine saves its book as it ends: the file is not open then)
	for (int role = 1; role < 5; ++role) {
		EngineSlot &s = eng_[role];
		s.proc.close();
		s.alive = s.lost = false;
		s.board.reset();
	}
	hint_busy_ = hint_accept_ = hint_background_ = false;
	tool_busy_ = false;
	com_thinking_ = com_pondering_ = com_waiting_book_ = com_accept_ = false;
	com_book_move_ = kNoMove;
	cmp_busy_ = cmp_accept_ = false;
	DeleteFileW((temp_dir_ + L"\\nobook.dat").c_str());
	DeleteFileW((temp_dir_ + L"\\nobook-tool.dat").c_str());
	close_main_engine(std::move(then));  // (last: what was asked is done after it)
}

bool App::refuse_while_book_busy()
{
	if (refuse_while_saving()) return true;
	if (!learning_) return false;
	message(lang::t("learn.busy"), MB_ICONINFORMATION);
	return true;
}

void App::restart_engines()
{
	// (a learning command takes no input to stop: the engine would be closed in the middle of it)
	if (refuse_while_book_busy()) return;
	stop_engines([this] {
		cache_.clear_book();
		cache_.clear_search();
		start_engines();
		if (compare_) set_compare(true);  // the other engine of the kind chosen now
		position_changed();
	});
}

// With the precision stages the engine writes the rows of the book moves when their stages are known, at the end of an
// answer that may take minutes (and the end of an answer with the umigame numbers may be as far). The values are asked
// by themselves, before all the rest: the analyses are done one after the other, and an answer of nothing else is
// there at once.
void App::ask_book_values(std::string &out, const Position &p)
{
	values_id_ = ask_main(out, p, Request::Values, "book moveinfo", "parts=");
}

std::optional<Position> App::next_values()
{
	if (tab_ == Tab::Tree && !panel_hidden_)
		if (const auto p = tree_next_wanted(true)) return p;
	for (int i = 0; i <= game_.size(); ++i) {
		const Position &p = game_.at(i);
		if (p.game_over() || p.must_pass()) continue;
		const PosInfo *info = cache_.peek(p);
		if (!info || !info->values_done) return p;
	}
	return std::nullopt;
}

// The engine reads its book, learns, or has a long job to do that moving the position does not stop (the analyses are
// done one after the other): nothing is asked behind it, and the book file is looked up meanwhile (not while a command
// that may replace the file runs: see start_learning_at).
bool App::book_held() const
{
	if (!eng_[0].loaded || learning_) return true;
	for (const auto &[id, r] : requests_) {
		if (r.kind == Request::FindLine || r.kind == Request::ListBook || r.kind == Request::ListBookAdd ||
		    r.kind == Request::BestExport || r.kind == Request::ReduceCount)
			return true;
	}
	return false;
}

// The book analyses of the other positions of the game, for the points of the graph: one at a time and only
// while the position shown has nothing waiting, so that it is never kept waiting by them. The values of all the
// positions (and of the open nodes of the tree) go before everything else.
void App::pump_graph()
{
	if (!eng_[0].alive || book_held() || !use_book_ || !show_eval_ || values_id_) return;
	if (!background_paused_ && values_apart()) {
		if (const auto p = next_values()) {
			std::string out = stop_main_analyses();
			stages_held_ = true;
			ask_book_values(out, *p);
			send(0, out);
			return;
		}
	}
	if (stages_held_) {
		stages_held_ = false;
		request_analysis();
		return;
	}
	if (graph_id_ || book_id_ || background_paused_) return;
	const std::string params = book_params();
	auto ask = [&](const Position &p) {
		std::string out;
		graph_id_ = ask_main(out, p, Request::Graph, "book moveinfo", params, params);
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

// The engine reads no command while it reads its book, and may take minutes: the book file is looked up meanwhile, for
// the position shown and then for the positions of the game (the points of the graph).
void App::request_book_peek()
{
	if (!book_peek_.running() || !use_book_ || !show_eval_) return;
	std::vector<Position> wanted;
	auto add = [&](const Position &p) {
		if (p.game_over()) return;
		const PosInfo *info = cache_.peek(p);
		if (info && (info->book_peeked || info->values_done)) return;
		if (std::find(wanted.begin(), wanted.end(), p) == wanted.end()) wanted.push_back(p);
	};
	add(game_.now());
	for (int i = 0; i <= game_.size(); ++i)
		if (!game_.at(i).must_pass()) add(game_.at(i));
	book_peek_.ask(std::move(wanted));
}

void App::book_peek_answers()
{
	std::vector<BookPeekAnswer> answers;
	book_peek_.take(answers);
	if (!book_held()) return;  // the engine answers now
	bool changed = false;
	for (const BookPeekAnswer &a : answers) {
		if (a.generation != book_peek_generation_) continue;
		PosInfo &info = cache_.get(a.pos);
		info.book_peeked = true;
		info.in_book = a.in_book;
		for (const BookPeekMove &e : a.moves) {
			MoveEval *m = info.find(e.sq);
			if (!m) continue;
			m->in_book = e.in_book;
			m->leaf = e.leaf;
			if (e.in_book) {
				m->book_value = e.value;
				m->book_level = e.level;
				m->lines = e.lines;
			} else if (e.leaf) {
				m->leaf_value = e.value;
				m->leaf_level = e.level;
			}
		}
		changed = true;
	}
	if (changed) invalidate();
}

void App::request_analysis()
{
	const Position &p = game_.now();
	const PosKey key = key_of(p);
	const bool want = show_eval_ && !p.game_over();

	// ---- main engine: the book analyses of the position
	if (eng_[0].alive && book_held()) {
		if (eng_[0].loaded) {
			if (!learning_) send(0, stop_main_analyses());
			// (lookups that found the file unusable are not started again: it is the same file until the engine answers)
			if (!book_peek_.started() && !(learning_ && learn_replaces_book_))
				book_peek_.start(book_file(), hwnd_, WM_BOOK_PEEK, ++book_peek_generation_);
		}
		request_book_peek();
	} else if (eng_[0].alive) {
		book_peek_.stop();
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
		cancel(values_id_, book_on && same(values_id_));
		const bool ask_values = info && values_apart() && !info->values_done && !values_id_;
		// the rest is asked when the values are there: of the position shown, of the game and of the tree
		const bool wait_values = info && values_apart() && (ask_values || values_id_ || (!background_paused_ && next_values()));
		info = book_on ? &cache_.get(p) : nullptr;  // (the lookup of the nodes of the tree may make room)
		// a point of the graph asked with other book settings is of no use any more; the position shown goes first:
		// a point of the graph for it becomes its request (unless it would hold the values back), one for another
		// position waits while it is not known (the analyses of the engine are done one after the other)
		const auto graph = requests_.find(graph_id_);
		const bool graph_ok = graph != requests_.end() && graph->second.params == params;
		if (graph_ok && info && !info->book_done && !book_id_ && graph->second.pos == key && !wait_values) {
			graph->second.kind = Request::Book;
			book_id_ = graph_id_;
			graph_id_ = 0;
		}
		cancel(graph_id_, graph_ok && !(info && !info->book_done));
		cancel(count_id_, book_on && count_ && same(count_id_));
		cancel(levels_id_, book_on && tab_ == Tab::Book && same(levels_id_));
		cancel(best_id_, book_on && tab_ == Tab::Book && same(best_id_));
		if (info) {
			if (missing_id_ && !same(missing_id_)) {  // (it may take long; asked again by the tab that shows it)
				missing_.asked = false;
				stop_request(out, missing_id_);
			}
			if (ask_values) ask_book_values(out, p);
			if (wait_values) {
				stages_held_ = true;
			} else {
				if (!info->book_done && !book_id_)
					book_id_ = ask_main(out, p, Request::Book, "book moveinfo", params, params);
				if (count_ && !info->count_done && !count_id_)
					count_id_ = ask_main(out, p, Request::Count, "book moveinfo", "parts=count");
				if (tab_ == Tab::Book && !info->levels_done && !levels_id_)
					levels_id_ = ask_main(out, p, Request::Levels, "book levels");
				if (tab_ == Tab::Book && !info->best_done && !best_id_)
					best_id_ = ask_main(out, p, Request::Best, "book bestlines");  // loss 0: the best moves only
				request_missing(out, p);
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

void App::eval_mismatch(int role)
{
	const std::wstring text = lang::f("log.eval_mismatch", {lang::t(kRoleNames[role])});
	log(text);
	set_status(text);
	if (eval_warned_) return;
	eval_warned_ = true;
	eval_warned_role_ = role;
	if (hwnd_) PostMessageW(hwnd_, WM_EVAL_WARN, 0, 0);  // (a message box is not shown while the lines of the engine are read)
}

void App::eval_mismatch_box()
{
	if (!script_.empty() || script_wait_ != ScriptWait::None) return;  // (a box would stop the script)
	message(lang::f("warn.eval_mismatch", {lang::t(kRoleNames[eval_warned_role_])}), MB_ICONWARNING);
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
	// newedax.13 (and later) with an eval file not made for it: it goes on, but its evaluation is wrong (said after
	// the lines are taken, so that it comes after the engine's own line in the log and stays in the status line)
	struct AtEnd {
		App *app = nullptr;
		int role = 0;
		~AtEnd()
		{
			if (app) app->eval_mismatch(role);
		}
	} eval_note;
	for (const std::string &line : lines)
		if (line.starts_with("WARNING") && line.find("has no weights for the extra groups") != std::string::npos) eval_note = {this, role};
	const bool held = role == 0 && book_held();
	bool changed = false, ask_missing = false, book_read = false;
	const PosKey here = key_of(game_.now());
	auto loaded_now = [&] {
		if (s.loaded) return;
		s.loaded = true;
		book_read = changed = true;
		measure::engine_answer(0);
	};

	if (role == 2) {
		for (const std::string &line : lines)
			if (!line.empty()) tool_line(line);
		if (ended && (s.alive || s.lost)) {
			s.alive = s.lost = false;
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
		if (ended && (s.alive || s.lost)) {
			s.alive = s.lost = false;
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
				if (f.num("export").value_or(0) != 0) s.exports = true;  // and book deviate ... export
				loaded_now();
				continue;
			}
			loaded_now();
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
					const long long written = f.num("written").value_or(0);
					if (!done || f.num("error").value_or(0))
						log(lang::f("best.export_failed", {best_file_}));
					else if (written == 0 && !file_exists(best_file_))  // (newedax makes no file without a line)
						log(lang::f("best.none", {best_file_}));
					else
						log(lang::f("best.written", {std::to_wstring(written), best_file_,
						                             std::to_wstring(f.num("lines").value_or(0)),
						                             std::to_wstring(f.num("incomplete_ends").value_or(0))}));
					requests_.erase(it);
				}
				continue;
			}
			PosInfo &info = cache_.get(position_of(req.pos));
			if constexpr (measure::kOn)
				measure::trace("BOOK " + position_of(req.pos).to_board_string() + " | " + line);
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
					info.values_done = info.values_done || done;
					if (book_id_ == id) book_id_ = 0;
					if (done && req.pos == here) ask_missing = true;  // the best book move is known now
				} else if (req.kind == Request::Values) {
					info.values_done = done;
					changed = true;  // the position shown, or a point of the graph
					if (values_id_ == id) values_id_ = 0;
				} else if (req.kind == Request::Graph) {
					info.book_done = done;
					info.values_done = info.values_done || done;
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
				loaded_now();
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
					PosInfo &info = cache_.get(position_of(hint_pos_));
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
					measure::trace("HINT " + position_of(hint_pos_).to_board_string() + " | " + line);
				PosInfo &info = cache_.get(position_of(hint_pos_));
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
	if (ended && (s.alive || s.lost)) {  // (lost: a command to it failed before its end came)
		s.alive = s.lost = false;
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
	if (book_read) {  // the position shown is asked now
		book_peek_.stop();
		if (book_bytes_) log(lang::f("log.book_loaded", {book_file(), size_text(*book_bytes_)}));
		else log(lang::f("log.book_missing", {book_file()}));
		if (s.alive) request_analysis();
	}
	if (held && !book_read && s.alive && !book_held()) request_analysis();  // the long job has ended
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
		if (info && info->values_done) {
			com_waiting_book_ = false;
			KillTimer(hwnd_, kTimerCom);
			com_think();
		}
	}
	if (changed) {
		update_animation_timer();
		invalidate();
	}
}

// Another book for the main engine (the menu, the recent books, a file dropped on the window).
void App::switch_book(const std::wstring &path)
{
	if (refuse_while_book_busy()) return;
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

// The main engine closed and started again (another book, the book read again): its requests go with it, and it starts
// again when it has ended (after the save of its book: close_main_engine).
void App::restart_main_engine()
{
	if (refuse_while_saving()) return;
	book_peek_.stop();  // (the engine saves its book as it ends: the file is not open then)
	close_main_engine([this] {
		cache_.clear_book();
		start_engines();
		position_changed();
	});
}

// The missing lines as games (the moves from the start), one a line, for a file.
// the list of missing lines is of the position shown (it stays when moves are played in another tab, where it is not
// asked again)
bool App::missing_here() const
{
	return !missing_.lines.empty() && missing_.pos == key_of(game_.now());
}

void App::export_missing()
{
	if (!missing_here()) {
		set_status(lang::t("status.missing_none"));
		return;
	}
	const std::wstring file = pick_save(file_filter("filter.text", L"*.txt").c_str(), L"txt", L"missing.txt");
	if (!file.empty()) write_missing(file);
}

bool App::write_missing(const std::wstring &file)
{
	const std::string head = game_.to_text(true);
	std::string out;
	for (const auto &ml : missing_.lines) {
		out += head;
		for (int mv : ml.moves)
			if (mv != kPass) out += square_name(mv);
		if (ml.leaf != kNoMove && ml.leaf != kPass) out += square_name(ml.leaf);
		out += "\r\n";
	}
	// (as the other exports: the log says it with the file, written or not, also when the file cannot be opened)
	FILE *f = nullptr;
	bool ok = _wfopen_s(&f, file.c_str(), L"wb") == 0 && f;
	if (ok) {
		const bool written = fwrite(out.data(), 1, out.size(), f) == out.size();
		ok = fclose(f) == 0 && written;  // (the last part is written as the file closes: a full drive fails there)
	}
	log(ok ? lang::f("status.missing_written", {std::to_wstring(missing_.lines.size()), file}) : lang::f("status.missing_failed", {file}));
	return ok;
}

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
	missing_id_ = ask_main(out, p, Request::Missing, "book missing", "sq=" + square_name(sq) + " " + params + " n=500");
}

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
