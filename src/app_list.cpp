// The game list (a file of games: read, add, save, sort, join the same games, cut, shuffle, one orientation,
// random orientations, delete, the move to open at, the lines of the book), the search of the position in the
// list, the kept lines, and the background jobs that do the heavy work.
#include <windows.h>

#include <cstdio>
#include <ctime>
#include <memory>

#include "app.h"
#include "form.h"
#include "lang.h"
#include "paths.h"

namespace ng {

namespace {

std::wstring file_name(const std::wstring &p)
{
	const size_t s = p.find_last_of(L"\\/");
	return s == std::wstring::npos ? p : p.substr(s + 1);
}

// the place in a game after n moves (passes not counted)
int index_after(const Game &g, int n)
{
	int k = 0;
	for (int i = 0; i < g.size(); ++i) {
		if (k == n) return i;
		if (g.moves()[i] != kPass) ++k;
	}
	return g.size();
}

std::wstring list_filter()
{
	std::wstring f = lang::t("filter.games") + L" (*.txt)";
	f += L'\0';
	f += L"*.txt";
	f += L'\0';
	f += lang::t("filter.all");
	f += L'\0';
	f += L"*.*";
	f += L'\0';
	return f;
}

std::optional<int> ask_number(HWND owner, const std::wstring &title, const std::wstring &label, int value, int lo, int hi,
                              const std::wstring &note = {})
{
	std::vector<FormField> f(1);
	f[0].kind = FormField::Number;
	f[0].label = label;
	f[0].text = std::to_wstring(value);
	f[0].lo = lo;
	f[0].hi = hi;
	f[0].note = note;
	if (!run_form(owner, title, f, lang::t("form.ok"), lang::t("form.cancel"), lang::t("form.bad_number"))) return std::nullopt;
	return std::stoi(f[0].text);
}

std::uint64_t seed_or_clock(int seed)
{
	return seed > 0 ? static_cast<std::uint64_t>(seed) : static_cast<std::uint64_t>(std::time(nullptr)) * 2654435761ULL;
}

}  // namespace

// ---- background jobs

void App::run_task(const std::wstring &label, std::function<std::function<void()>(std::stop_token)> work)
{
	if (task_busy_) {
		message(lang::t("task.busy"), MB_ICONINFORMATION);
		return;
	}
	if (task_.joinable()) task_.join();
	task_busy_ = true;
	task_label_ = label;
	task_t0_ = std::chrono::steady_clock::now();
	const HWND h = hwnd_;
	task_ = std::jthread([this, h, work = std::move(work)](std::stop_token st) {
		std::function<void()> done = work(st);
		{
			std::lock_guard lock(task_mutex_);
			task_done_ = std::move(done);
		}
		PostMessageW(h, WM_TASK, 0, 0);
	});
	update_menu();
	invalidate();
}

void App::task_ended()
{
	std::function<void()> done;
	{
		std::lock_guard lock(task_mutex_);
		done = std::move(task_done_);
	}
	if (task_.joinable()) task_.join();
	task_busy_ = false;
	if (done) done();
	update_menu();
	invalidate();
	pump_script();
}

void App::cancel_task()
{
	if (task_busy_) task_.request_stop();
}

// "Stop the background work": the job on the list, the book analyses of the main engine (the precision stages, the
// points of the graph, the counts) and the searches of the search engine stop; nothing starts again before the
// position changes (or the analysis is asked again).
void App::stop_background()
{
	cancel_task();
	background_paused_ = true;
	if (!learning_) send(0, stop_main_analyses());
	if (hint_busy_) stop_search(1);
	log(lang::t("task.stopped_all"));
	update_menu();
	invalidate();
}

// ---- the game list

void App::list_load(const std::wstring &file, bool add)
{
	if (task_busy_) return;
	auto nl = std::make_shared<GameList>();
	run_task(lang::f("list.reading", {file_name(file)}), [this, nl, file, add](std::stop_token st) -> std::function<void()> {
		const auto t0 = std::chrono::steady_clock::now();
		const ListReadStats r = read_list(file, *nl, st);
		const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		return [this, nl, file, add, r, s] {
			if (!r.opened) {
				log(lang::f("list.cannot_open", {file}));
				return;
			}
			if (r.cancelled) {
				log(lang::t("task.cancelled"));
				return;
			}
			if (add) {
				for (size_t i = 0; i < nl->size(); ++i) list_.add(nl->start(i), nl->moves(i));
			} else {
				list_ = std::move(*nl);
				list_index_ = -1;
				list_file_ = file;
			}
			hits_.clear();
			hits_shown_ = false;
			list_scroll_ = 0;
			tab_ = Tab::List;
			wchar_t sec[32];
			swprintf(sec, 32, L"%.2f", s);
			log(lang::f("task.took", {lang::f(add ? "list.added" : "list.read", {std::to_wstring(r.games), file_name(file),
			                                                                    std::to_wstring(list_.size()), std::to_wstring(r.cut)}),
			                          sec}));
			if (!add && !list_.empty()) list_select(0);
		};
	});
}

// the list moves to a job and comes back after it (the window shows it as busy meanwhile)
static void list_job(App &app, const std::wstring &label, std::function<std::wstring(GameList &, std::stop_token)> work)
{
	if (app.task_busy_) {  // the list stays where it is
		app.set_status(lang::t("task.busy"));
		return;
	}
	auto l = std::make_shared<GameList>(std::move(app.list_));
	app.list_.clear();
	app.list_index_ = -1;
	app.hits_.clear();
	app.hits_shown_ = false;
	app.run_task(label, [&app, l, work](std::stop_token st) -> std::function<void()> {
		const auto t0 = std::chrono::steady_clock::now();
		const std::wstring msg = work(*l, st);
		const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		return [&app, l, msg, s] {
			app.list_ = std::move(*l);
			app.list_scroll_ = 0;
			wchar_t sec[32];
			swprintf(sec, 32, L"%.2f", s);
			app.log(lang::f("task.took", {msg, sec}));
		};
	});
}

void App::list_job_script(const std::wstring &label, std::function<void(GameList &)> work)
{
	list_job(*this, label, [work](GameList &l, std::stop_token) {
		work(l);
		return lang::f("list.done", {std::to_wstring(l.size())});
	});
}

void App::list_select(int index, bool from_hit, int hit)
{
	if (index < 0 || index >= static_cast<int>(list_.size())) return;
	const auto mv = list_.moves(static_cast<size_t>(index));
	Game g;
	replay(g, list_.start(static_cast<size_t>(index)), std::vector<int>(mv.begin(), mv.end()));
	int orient = 0;
	if (from_hit && hit >= 0 && hit < static_cast<int>(hits_.size())) {
		// the game turned so that the position searched is shown as it was
		orient = symmetry_inverse(hits_[hit].sym);
		g.go_to(index_after(g, hits_[hit].ply));
		if (orient) g = transformed(g, orient);
	} else {
		g.go_to(list_start_ply_ < 0 ? g.size() : index_after(g, list_start_ply_));
	}
	set_game(g, false);
	orient_ = orient;
	list_index_ = index;
	update_menu();
	set_status(lang::f("list.at", {std::to_wstring(index + 1), std::to_wstring(list_.size())}));
}

void App::list_search()
{
	if (task_busy_) return;
	if (list_.empty()) {
		set_status(lang::t("list.empty"));
		return;
	}
	const Position target = game_.now();
	const bool sym = ini_.get_bool("list", "search_symmetries", true);
	auto out = std::make_shared<std::vector<ListHit>>();
	run_task(lang::t("list.searching"), [this, target, sym, out](std::stop_token st) -> std::function<void()> {
		const auto t0 = std::chrono::steady_clock::now();
		*out = search_list(list_, target, sym, st);
		const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
		const bool cancelled = st.stop_requested();
		return [this, out, ms, cancelled] {
			hits_ = std::move(*out);
			hits_shown_ = true;
			list_scroll_ = 0;
			tab_ = Tab::List;
			log(lang::f(cancelled ? "list.search_cancelled" : "list.found",
			            {std::to_wstring(hits_.size()), std::to_wstring(list_.size()), std::to_wstring(ms)}));
		};
	});
}

void App::list_command(int id)
{
	using lang::t;
	if (id == cmd::TaskCancel) {
		cancel_task();
		return;
	}
	if (task_busy_) {
		set_status(t("task.busy"));
		return;
	}
	switch (id) {
	case cmd::ListOpen:
	case cmd::ListAdd: {
		const std::wstring f = pick_open(list_filter().c_str(), t(id == cmd::ListOpen ? "menu.list.open" : "menu.list.add"));
		if (!f.empty()) list_load(f, id == cmd::ListAdd);
		break;
	}
	case cmd::ListSave: {
		if (list_.empty()) {
			set_status(t("list.empty"));
			break;
		}
		const std::wstring f = pick_save(list_filter().c_str(), L"txt", list_file_.empty() ? L"games.txt" : file_name(list_file_));
		if (f.empty()) break;
		if (write_list(f, list_)) {
			list_file_ = f;
			log(lang::f("list.saved", {std::to_wstring(list_.size()), f}));
		} else {
			log(t("status.write_failed"));
		}
		break;
	}
	case cmd::ListAddGame: {
		std::vector<std::uint8_t> mv;
		for (const int m : game_.moves())
			if (m != kPass) mv.push_back(static_cast<std::uint8_t>(m));
		list_.add(game_.start(), mv);
		list_index_ = static_cast<int>(list_.size()) - 1;
		log(lang::f("list.game_added", {std::to_wstring(list_.size())}));
		break;
	}
	case cmd::ListSort:
		list_job(*this, t("menu.list.sort"), [](GameList &l, std::stop_token) {
			sort_list(l);
			return lang::f("list.sorted", {std::to_wstring(l.size())});
		});
		break;
	case cmd::ListUnique:
		list_job(*this, t("menu.list.unique"), [](GameList &l, std::stop_token) {
			const size_t n = unique_list(l);
			return lang::f("list.uniqued", {std::to_wstring(n), std::to_wstring(l.size())});
		});
		break;
	case cmd::ListTruncate: {
		const auto n = ask_number(hwnd_, t("menu.list.truncate"), t("list.truncate.n"), ini_.get_int("list", "truncate", 20, 0, 60), 0, 60);
		if (!n) break;
		ini_.set_int("list", "truncate", *n);
		const int k = *n;
		list_job(*this, t("menu.list.truncate"), [k](GameList &l, std::stop_token) {
			truncate_list(l, k);
			return lang::f("list.truncated", {std::to_wstring(k)});
		});
		break;
	}
	case cmd::ListShuffle: {
		const auto n = ask_number(hwnd_, t("menu.list.shuffle"), t("list.seed"), 0, 0, 2000000000, t("list.seed.note"));
		if (!n) break;
		const std::uint64_t seed = seed_or_clock(*n);
		list_job(*this, t("menu.list.shuffle"), [seed](GameList &l, std::stop_token) {
			shuffle_list(l, seed);
			return lang::f("list.shuffled", {std::to_wstring(seed)});
		});
		break;
	}
	case cmd::ListNormalize:
		list_job(*this, t("menu.list.normalize"), [](GameList &l, std::stop_token st) {
			normalize_list(l, st);
			return st.stop_requested() ? lang::t("task.cancelled") : lang::t("list.normalized");
		});
		break;
	case cmd::ListScatter: {
		std::vector<FormField> f(2);
		f[0].kind = FormField::Choice;
		f[0].label = t("list.scatter.side");
		f[0].choices = {t("list.scatter.black"), t("list.scatter.white"), t("list.scatter.both")};
		f[0].choice = 2;
		f[1].kind = FormField::Number;
		f[1].label = t("list.seed");
		f[1].text = L"0";
		f[1].lo = 0;
		f[1].hi = 2000000000;
		f[1].note = t("list.seed.note");
		if (!run_form(hwnd_, t("menu.list.scatter"), f, t("form.ok"), t("form.cancel"), t("form.bad_number"))) break;
		const int sides = f[0].choice + 1;
		const std::uint64_t seed = seed_or_clock(std::stoi(f[1].text));
		list_job(*this, t("menu.list.scatter"), [sides, seed](GameList &l, std::stop_token st) {
			scatter_list(l, seed, sides, st);
			return st.stop_requested() ? lang::t("task.cancelled") : lang::f("list.scattered", {std::to_wstring(seed)});
		});
		break;
	}
	case cmd::ListDelete:
		if (list_index_ >= 0 && list_index_ < static_cast<int>(list_.size())) {
			list_.erase(static_cast<size_t>(list_index_));
			hits_.clear();
			hits_shown_ = false;
			log(lang::f("list.deleted", {std::to_wstring(list_index_ + 1), std::to_wstring(list_.size())}));
			if (list_index_ >= static_cast<int>(list_.size())) list_index_ = static_cast<int>(list_.size()) - 1;
			if (list_index_ >= 0) list_select(list_index_);
		}
		break;
	case cmd::ListClear:
		if (!list_.empty() && message(lang::f("list.ask_clear", {std::to_wstring(list_.size())}), MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
			list_.clear();
			list_index_ = -1;
			hits_.clear();
			hits_shown_ = false;
			list_file_.clear();
			log(t("list.cleared"));
		}
		break;
	case cmd::ListStartPly: {
		const auto n = ask_number(hwnd_, t("menu.list.start"), t("list.start.n"), list_start_ply_, -1, 60, t("list.start.note"));
		if (!n) break;
		list_start_ply_ = *n;
		ini_.set_int("list", "start_ply", list_start_ply_);
		break;
	}
	case cmd::ListFromBook: list_from_book(); break;
	case cmd::ListPrev:
	case cmd::ListNext:
		if (hits_shown_ && !hits_.empty()) {
			// through the games found
			int h = 0;
			for (int k = 0; k < static_cast<int>(hits_.size()); ++k)
				if (static_cast<int>(hits_[k].game) == list_index_) h = k + (id == cmd::ListNext ? 1 : -1);
			h = std::clamp(h, 0, static_cast<int>(hits_.size()) - 1);
			list_select(static_cast<int>(hits_[h].game), true, h);
		} else if (!list_.empty()) {
			const int i = list_index_ < 0 ? 0 : list_index_ + (id == cmd::ListNext ? 1 : -1);
			if (i >= 0 && i < static_cast<int>(list_.size())) list_select(i);
		}
		break;
	case cmd::ListReload: list_select(list_index_); break;
	case cmd::ListSearch: list_search(); break;
	case cmd::ListSearchClear:
		hits_.clear();
		hits_shown_ = false;
		break;
	default: break;
	}
	update_menu();
	invalidate();
}

// The lines of the book from the position shown into the list: the main engine writes the end positions of the
// lines of best moves (book bestlines, with the loss and the depth asked) to a file that is read in.
void App::list_from_book()
{
	using lang::t;
	if (!eng_[0].alive || learning_) {
		set_status(t("learn.no_engine"));
		return;
	}
	std::vector<FormField> f(3);
	f[0].kind = FormField::Number;
	f[0].label = t("best.loss");
	f[0].text = std::to_wstring(ini_.get_int("list", "book_loss", 0, 0, 129));
	f[0].lo = 0;
	f[0].hi = 129;
	f[0].note = t("best.loss.note");
	f[1].kind = FormField::Number;
	f[1].label = t("best.depth");
	f[1].text = std::to_wstring(ini_.get_int("list", "book_depth", 60, 1, 60));
	f[1].lo = 1;
	f[1].hi = 60;
	f[1].note = t("best.depth.note");
	f[2].kind = FormField::Number;
	f[2].label = t("list.book.n");
	f[2].text = std::to_wstring(ini_.get_int("list", "book_n", 10000, 1, 10000000));
	f[2].lo = 1;
	f[2].hi = 10000000;
	if (!run_form(hwnd_, t("menu.list.book"), f, t("form.ok"), t("form.cancel"), t("form.bad_number"))) return;
	ini_.set("list", "book_loss", narrow(f[0].text));
	ini_.set("list", "book_depth", narrow(f[1].text));
	ini_.set("list", "book_n", narrow(f[2].text));
	bool add = false;
	if (!list_.empty()) {
		const int a = message(lang::f("list.book.ask_add", {std::to_wstring(list_.size())}), MB_YESNOCANCEL | MB_ICONQUESTION);
		if (a == IDCANCEL) return;
		add = a == IDYES;
	}
	ask_book_lines(std::stoi(f[0].text), std::stoi(f[1].text), std::stoi(f[2].text), add);
}

bool App::ask_book_lines(int loss, int depth, int n, bool add)
{
	if (!eng_[0].alive || learning_) return false;
	const std::wstring file = temp_dir_ + L"\\book-lines.txt";
	DeleteFileW(file.c_str());
	const auto path = engine_path(file, false);  // the temporary folder
	if (!path) {
		message(lang::f("err.engine_path", {file}), MB_ICONWARNING);
		return false;
	}
	std::string out = line_commands(game_.start(), moves_to_cursor());
	eng_[0].board = key_of(game_.now());
	const int id = new_id();
	out += "book bestlines id=" + std::to_string(id) + " loss=" + std::to_string(std::clamp(loss, 0, 129)) +
	       " depth=" + std::to_string(std::clamp(depth, 1, 60)) + " solved=stop which=all file=\"" + *path + "\"\n";
	requests_[id] = {add ? Request::ListBookAdd : Request::ListBook, key_of(game_.now())};
	list_book_limit_ = std::max(1, n);
	list_book_start_ = game_.start();
	list_book_id_ = id;
	send(0, out);
	set_status(lang::t("list.book.asked"));
	return true;
}

void App::list_book_written(bool add, bool ok)
{
	list_book_id_ = 0;
	const std::wstring file = temp_dir_ + L"\\book-lines.txt";
	if (!ok) {
		log(lang::f("best.export_failed", {file}));
		return;
	}
	auto nl = std::make_shared<GameList>();
	const int limit = list_book_limit_;
	const Position start = list_book_start_;
	run_task(lang::t("menu.list.book"), [this, nl, file, add, limit, start](std::stop_token st) -> std::function<void()> {
		const bool cancelled = read_list(file, *nl, st, start).cancelled;
		DeleteFileW(file.c_str());
		return [this, nl, add, limit, cancelled] {
			if (cancelled) {  // (the parts read in on threads stop half way each: nothing is added)
				log(lang::t("task.cancelled"));
				return;
			}
			const size_t n = std::min<size_t>(nl->size(), static_cast<size_t>(limit));
			if (!add) {
				list_.clear();
				list_file_.clear();
				list_index_ = -1;
			}
			for (size_t i = 0; i < n; ++i) list_.add(nl->start(i), nl->moves(i));
			hits_.clear();
			hits_shown_ = false;
			tab_ = Tab::List;
			log(lang::f("list.book.read", {std::to_wstring(n), std::to_wstring(nl->size()), std::to_wstring(list_.size())}));
		};
	});
}

// ---- the kept lines

void App::keep_now(bool quiet)
{
	Kept k;
	k.start = game_.start();
	k.moves = game_.moves();
	k.cursor = game_.cursor();
	k.time = static_cast<long long>(std::time(nullptr));
	// the same line again: kept once (kept by itself: its place follows; a bookmark asked at another place: a new one)
	if (!kept_.empty() && kept_.back().start == k.start && kept_.back().moves == k.moves && (quiet || kept_.back().cursor == k.cursor)) {
		kept_.back().cursor = k.cursor;
	} else {
		kept_.push_back(std::move(k));
	}
	kept_sel_ = static_cast<int>(kept_.size()) - 1;
	kept_scroll_ = static_cast<int>(kept_.size());  // the row shows the newest (the drawing keeps it in its range)
	save_kept();
	if (!quiet) log(lang::f("kept.added", {std::to_wstring(kept_.size())}));
	invalidate();
}

void App::kept_command(int id)
{
	using lang::t;
	if (id == cmd::KeepNow) {
		keep_now(false);
		return;
	}
	if (id == cmd::KeptClear) {
		if (!kept_.empty() && message(lang::f("kept.ask_clear", {std::to_wstring(kept_.size())}), MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
			kept_.clear();
			kept_sel_ = -1;
			save_kept();
		}
		invalidate();
		return;
	}
	if (kept_sel_ < 0 || kept_sel_ >= static_cast<int>(kept_.size())) {
		set_status(t("kept.choose"));
		return;
	}
	const Kept &k = kept_[static_cast<size_t>(kept_sel_)];
	switch (id) {
	case cmd::KeptShow: {
		Game g;
		g.reset(k.start);
		for (const int m : k.moves)
			if (m != kPass) g.play(m);
		g.go_to(k.cursor);
		set_game(g, false);
		break;
	}
	case cmd::KeptCompare:
		// compared like a fixed record: a red frame on its losing moves, a mark where the game goes another way
		fixed_.clear();
		for (const int m : k.moves) fixed_.push_back(m);
		fixed_start_ = k.start;
		fixed_on_ = true;
		log(lang::f("kept.compared", {std::to_wstring(kept_sel_ + 1)}));
		break;
	case cmd::KeptDelete:
		kept_.erase(kept_.begin() + kept_sel_);
		if (kept_sel_ >= static_cast<int>(kept_.size())) kept_sel_ = static_cast<int>(kept_.size()) - 1;
		save_kept();
		break;
	default: break;
	}
	update_menu();
	invalidate();
}

// the menu of a bookmark: the right button on its picture
void App::bookmark_menu(int x, int y)
{
	HMENU m = CreatePopupMenu();
	AppendMenuW(m, MF_STRING, cmd::KeptShow, lang::t("menu.kept.show").c_str());
	AppendMenuW(m, MF_STRING, cmd::KeptCompare, lang::t("menu.kept.compare").c_str());
	AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(m, MF_STRING, cmd::KeptDelete, lang::t("menu.kept.delete").c_str());
	POINT pt{x, y};
	ClientToScreen(hwnd_, &pt);
	const int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
	DestroyMenu(m);
	if (id) on_command(id);
}

// kept.txt next to the exe: "seconds <TAB> start board (empty: the usual start) <TAB> moves <TAB> place"
void App::load_kept()
{
	FILE *f = nullptr;
	if (_wfopen_s(&f, (exe_dir_ + L"\\kept.txt").c_str(), L"rb") != 0 || !f) return;
	std::string all;
	char buf[1 << 14];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0) all.append(buf, n);
	fclose(f);
	size_t i = 0;
	while (i < all.size()) {
		size_t e = all.find('\n', i);
		if (e == std::string::npos) e = all.size();
		std::string line = all.substr(i, e - i);
		i = e + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();
		std::vector<std::string> p;
		size_t a = 0;
		for (;;) {
			const size_t t = line.find('\t', a);
			p.push_back(line.substr(a, t == std::string::npos ? std::string::npos : t - a));
			if (t == std::string::npos) break;
			a = t + 1;
		}
		if (p.size() < 4) continue;
		Kept k;
		k.time = std::atoll(p[0].c_str());
		if (!p[1].empty()) {
			auto s = parse_board_text(p[1]);
			if (!s) continue;
			k.start = *s;
		}
		Game g;
		g.reset(k.start);
		for (const int m : parse_moves(p[2]).moves)
			if (!g.play(m)) break;
		k.moves = g.moves();
		k.cursor = std::clamp(std::atoi(p[3].c_str()), 0, static_cast<int>(k.moves.size()));
		kept_.push_back(std::move(k));
	}
}

void App::save_kept() const
{
	std::string out;
	for (const Kept &k : kept_) {
		out += std::to_string(k.time) + "\t";
		if (!(k.start == Position{})) out += k.start.to_board_string();
		out += "\t";
		for (const int m : k.moves)
			if (m != kPass) out += square_name(m);
		out += "\t" + std::to_string(k.cursor) + "\r\n";
	}
	const std::wstring path = exe_dir_ + L"\\kept.txt";
	if (out.empty()) {
		DeleteFileW(path.c_str());
		return;
	}
	FILE *f = nullptr;
	if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) return;
	fwrite(out.data(), 1, out.size(), f);
	fclose(f);
}

}  // namespace ng
