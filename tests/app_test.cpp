// The app with the main engine: the lines the engine writes and what becomes of the requests. The engine is this
// program started again with "--engine": it writes back the lines it reads that begin with '!' (without the '!').
#include <windows.h>

#include <wrl/client.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "app.h"
#include "lang.h"
#include "paths.h"

using namespace ng;

static int failures = 0;
#define CHECK(c)                                                              \
	do {                                                                      \
		if (!(c)) {                                                           \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);          \
			++failures;                                                       \
		}                                                                     \
	} while (0)

static std::wstring self;

static std::unique_ptr<App> make_app()
{
	auto a = std::make_unique<App>();
	a->hwnd_ = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
	wchar_t tmp[MAX_PATH];
	GetTempPathW(MAX_PATH, tmp);
	a->exe_dir_ = a->temp_dir_ = tmp;
	a->ini_path_ = a->temp_dir_ + L"\\ng-app-test.ini";
	a->show_eval_ = false;
	a->backward_ = false;
	return a;
}

// the main engine: this program, writing back what it is given
static void start_engine(App &a)
{
	EngineSlot &s = a.eng_[0];
	s.generation = 1;
	s.alive = s.proc.start(self, L"--engine", a.temp_dir_, a.hwnd_, WM_ENGINE, engine_tag(0, 1), nullptr);
	s.loaded = true;
	CHECK(s.alive);
}

// the engine writes `lines`, then a line that ends them (the answer to a probe); the app reads them all
static void feed(App &a, const std::string &lines)
{
	a.eng_[0].probe_id = kMaxRequestId - 1;
	a.eng_[0].proc.send(lines + "!@cancel id=" + std::to_string(kMaxRequestId - 1) + " state=none\n");
	const ULONGLONG end = GetTickCount64() + 10000;
	while (a.eng_[0].probe_id && GetTickCount64() < end) {
		MSG m{};
		while (PeekMessageW(&m, a.hwnd_, WM_ENGINE, WM_ENGINE, PM_REMOVE)) a.on_engine(m.wParam, m.lParam != 0);
		Sleep(1);
	}
	CHECK(a.eng_[0].probe_id == 0);
}

static bool logged(const App &a, const std::wstring &text)
{
	for (const auto &l : a.log_)
		if (l.find(text) != std::wstring::npos) return true;
	return false;
}

// the messages the app posts to itself (after the lines of an engine are read), as the window would take them
static void pump_app(App &a)
{
	MSG m{};
	while (PeekMessageW(&m, a.hwnd_, WM_APP + 2, WM_APP + 63, PM_REMOVE)) a.proc(a.hwnd_, m.message, m.wParam, m.lParam);
}

// the search engine (role 1): this program too
static void start_search_engine(App &a)
{
	EngineSlot &s = a.eng_[1];
	s.generation = 1;
	s.alive = s.proc.start(self, L"--engine", a.temp_dir_, a.hwnd_, WM_ENGINE, engine_tag(1, 1), nullptr);
	s.loaded = true;
	CHECK(s.alive);
}

// the only request asked of the main engine (0 if none or more)
static int only_request(const App &a)
{
	return a.requests_.size() == 1 ? a.requests_.begin()->first : 0;
}

// "book reduce X Y" of a script: the positions kept and removed are counted first (book reducecount, from the initial
// position), then the book command runs from the initial position; its result line gives the numbers in the log.
static void test_reduce_script()
{
	auto a = make_app();
	start_engine(*a);
	a->game_.play(parse_square("f5"));
	std::wstring err;
	CHECK(a->script_command("book reduce 2 4", 1, &err));
	const int id = only_request(*a);
	CHECK(id != 0);
	feed(*a, "!@end id=" + std::to_string(id) + " kept=6 removed=1541 positions=1547 ms=0\n");
	pump_app(*a);
	CHECK(a->learning_ && a->learn_cmd_ == "book reduce 2 4");
	CHECK(a->ini_.get("learn", "last_start") == Position{}.to_board_string() && a->ini_.get("learn", "last_moves").empty());
	feed(*a, "!Book reduce 2 4: walk...\rBook reduce 2 4: 6 positions kept, 1541 removed\n!echo: \"M" +
	             std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
	CHECK(!a->learning_);
	CHECK(logged(*a, lang::f("reduce.done", {L"6", L"1,541", L"2", L"4"})));
	CHECK(a->game_.cursor() == 1);  // the game shown stays

	// from the position shown ("here")
	CHECK(a->script_command("book reduce 1 3 here", 1, &err));
	const int here = only_request(*a);
	CHECK(here != 0);
	feed(*a, "!@end id=" + std::to_string(here) + " kept=2 removed=4 positions=6 ms=0\n");
	pump_app(*a);
	CHECK(a->learning_ && a->learn_cmd_ == "book reduce 1 3" && a->ini_.get("learn", "last_moves") == "f5");
	a->learning_ = false;
}

// An engine without book reduce (v4.5.5-nikque.19, newedax.4) answers the count with a warning: nothing is done, and
// the log says which engines have it. A position out of the book is not reduced either.
static void test_reduce_unknown()
{
	auto a = make_app();
	start_engine(*a);
	std::wstring err;
	CHECK(a->script_command("book reduce 2 4", 1, &err));
	const int id = only_request(*a);
	CHECK(id != 0);
	feed(*a, "!\n!WARNING: Unknown book command: \"book reducecount id=" + std::to_string(id) + " x=2 y=4\"\n");
	pump_app(*a);
	CHECK(!a->learning_);
	CHECK(a->requests_.empty());
	CHECK(logged(*a, L"nikque.20"));
	CHECK(!a->script_waiting());

	CHECK(a->script_command("book reduce 2 4 here", 1, &err));
	const int out = only_request(*a);
	feed(*a, "!@end id=" + std::to_string(out) + " kept=6 removed=0 positions=6 notinbook=1 ms=0\n");
	pump_app(*a);
	CHECK(!a->learning_ && a->requests_.empty());
	CHECK(!a->script_command("book reduce 2", 1, &err));
	CHECK(!a->script_command("book reduce 2 255", 1, &err));
}

// A line of the book analyses is written whole by another thread of the engine, while a line of the main thread
// (a warning, the result of a command) may be written in parts: the analysis line then comes after the first part.
static void test_mixed_lines()
{
	auto a = make_app();
	start_engine(*a);
	const PosKey key = key_of(a->game_.now());
	a->book_id_ = 51;
	a->requests_[51] = {Request::Book, key};
	a->eng_[0].ping_id = 7;
	feed(*a, "!@begin id=51 ply=0 turn=black moves=4 book=1\n"
	         "!@move id=51 sq=f5 book=1 value=3 level=4 lines=1 stage=2\n"
	         "!WARNING: @end id=51 ms=1\n"
	         "!echo: \"M7\" is not on or off; ignored\n");
	const PosInfo *info = a->cache_.peek(a->game_.now());
	CHECK(info && info->book_done);
	CHECK(a->book_id_ == 0);
	CHECK(a->requests_.empty());
	CHECK(a->eng_[0].ping_id == 0);

	// the first part of a line waits for its end, which may come later
	a->book_id_ = 52;
	a->requests_[52] = {Request::Book, key};
	a->cache_.get(a->game_.now()).book_done = false;
	feed(*a, "!{board:x; moves: [c3:-44]@move id=52 sq=d3 book=1 value=-3 level=4 lines=1 stage=5\n");
	feed(*a, "!@end id=52 ms=0\n!<g3:-47>}\n");
	info = a->cache_.peek(a->game_.now());
	CHECK(info && info->book_done && info->find(parse_square("d3"))->stage == 5);
	CHECK(logged(*a, L"{board:x; moves: [c3:-44]<g3:-47>}"));

	// an '@' that does not begin an analysis line ("@<word> id=<number>") stays in the line
	feed(*a, "!mail@begin id=x\n!a @move id=3x b\n");
	CHECK(logged(*a, L"mail@begin id=x"));
	CHECK(logged(*a, L"a @move id=3x b"));

	// nor an analysis line quoted in a warning (its words are not all "key=value")
	a->book_id_ = 53;
	a->requests_[53] = {Request::Book, key};
	a->cache_.get(a->game_.now()).book_done = false;
	feed(*a, "!WARNING: echo: \"@end id=53 ms=0\" is not on or off; ignored\n");
	CHECK(a->book_id_ == 53 && a->requests_.count(53));
	CHECK(logged(*a, L"WARNING: echo: \"@end id=53 ms=0\" is not on or off; ignored"));
}

// The book settings changed while the counts and the levels are coming: what came of them stays (they do not
// depend on the settings), and they come to their end.
static void test_settings_keep_counts()
{
	auto a = make_app();
	start_engine(*a);
	a->show_eval_ = a->use_book_ = a->count_ = true;
	a->tab_ = Tab::Book;
	a->request_analysis();
	const int count = a->count_id_, levels = a->levels_id_;
	CHECK(count && levels);
	const std::string c = std::to_string(count), l = std::to_string(levels);
	feed(*a, "!@begin id=" + c + " ply=0 turn=black moves=4 book=1\n!@move id=" + c + " sq=d3 part=count positions=123\n!@level id=" + l +
	             " level=4 count=409494\n");
	a->base_ = "deviate2";
	a->request_analysis();
	CHECK(a->count_id_ == count && a->levels_id_ == levels);
	feed(*a, "!@end id=" + c + " ms=1\n!@end id=" + l + " positions=409494 ms=1\n");
	const PosInfo *info = a->cache_.peek(a->game_.now());
	CHECK(info && info->count_done && info->find(parse_square("d3"))->positions == 123);
	CHECK(info && info->levels_done && info->levels.size() == 1 && info->levels_positions == 409494);
	CHECK(info && !info->book_done && info->book_params == a->book_params());  // the values of the book: asked again
}

// The commands of a script and those typed are UTF-8; the engine reads them in the code page of the system. The last
// learning command is kept in config.ini as UTF-8.
static void test_command_letters()
{
	auto a = make_app();
	start_engine(*a);
	const std::wstring dir = L"D:\\ng-test\\\u65e5\u672c\u8a9e\\";  // (letters of code page 932)
	const auto queued = acp(L"book save " + dir + L"queued.dat"), sent = acp(L"book save " + dir + L"sent.dat");
	std::wstring err;
	CHECK(a->script_command("learn book save " + narrow(dir) + "queued.dat", 1, &err));
	CHECK(!a->learn_queue_.empty() && queued && a->learn_queue_.back().command == *queued);
	CHECK(a->script_command("edax book save " + narrow(dir) + "sent.dat", 1, &err));
	CHECK(a->learning_ && sent && a->learn_cmd_ == *sent);
	CHECK(a->ini_.get("learn", "last_command") == narrow(L"book save " + dir + L"sent.dat"));
	a->learning_ = false;
	// a name in "..." (given as the engine can open it) and other letters: each in the code page once
	const auto mixed = acp(L"echo 日本語 " + dir + L"q.dat");
	CHECK(a->script_command("edax echo " + narrow(L"日本語 \"" + dir + L"q.dat\""), 1, &err));
	CHECK(mixed && a->learn_cmd_ == *mixed);
	a->learning_ = false;
	// a letter the engine cannot read: the command is not given
	a->learn_queue_.clear();
	CHECK(!a->script_command("learn book save D:\\\xF0\x9F\x90\xBE.dat", 1, &err));  // (U+1F43E)
	CHECK(a->learn_queue_.empty());
}

// An answer of a background analysis asked with other book settings is not taken for the settings of now.
static void test_old_settings()
{
	auto a = make_app();
	start_engine(*a);
	a->show_eval_ = a->use_book_ = true;
	a->count_ = false;
	a->pump_graph();
	const int old = a->graph_id_;
	CHECK(old != 0);
	a->base_ = "deviate2";
	a->request_analysis();
	const int fresh = a->book_id_;
	CHECK(fresh != 0 && fresh != old);
	feed(*a, "!@begin id=" + std::to_string(old) + " ply=0 turn=black moves=4 book=1\n!@move id=" + std::to_string(old) +
	             " sq=f5 book=1 value=3 level=4 lines=1 stage=7\n!@end id=" + std::to_string(old) + " ms=1\n");
	const PosInfo *info = a->cache_.peek(a->game_.now());
	CHECK(info && !info->book_done);
	CHECK(info && info->find(parse_square("f5"))->stage != 7);
	feed(*a, "!@begin id=" + std::to_string(fresh) + " ply=0 turn=black moves=4 book=1\n!@move id=" + std::to_string(fresh) +
	             " sq=f5 book=1 value=3 level=4 lines=1 stage=3\n!@end id=" + std::to_string(fresh) + " ms=1\n");
	info = a->cache_.peek(a->game_.now());
	CHECK(info && info->book_done && info->find(parse_square("f5"))->stage == 3);
}

// The number of a request stopped is not given again before the end of its answer has come.
static void test_stopped_numbers()
{
	auto a = make_app();
	start_engine(*a);
	const int old = a->new_id();
	a->book_id_ = old;
	a->requests_[old] = {Request::Book, key_of(a->game_.now())};
	a->stop_main_analyses();
	int again = 0;
	for (int i = 0; i < kMaxRequestId && !again; ++i)  // every number once
		if (a->new_id() == old) again = 1;
	CHECK(!again);
	feed(*a, "!@cancel id=" + std::to_string(old) + "\n");
	for (int i = 0; i < kMaxRequestId && !again; ++i)
		if (a->new_id() == old) again = 1;
	CHECK(again);
}

// The main engine ending by itself takes its requests with it, as the other ways of closing it do.
static void test_engine_ended()
{
	auto a = make_app();
	const PosKey key = key_of(a->game_.now());
	a->graph_id_ = 41;
	a->missing_id_ = 42;
	a->book_id_ = 43;
	a->requests_[41] = {Request::Graph, key};
	a->requests_[42] = {Request::Missing, key};
	a->requests_[43] = {Request::Book, key};
	a->eng_[0].alive = true;
	a->on_engine(engine_tag(0, 0), true);
	CHECK(a->graph_id_ == 0 && a->missing_id_ == 0 && a->book_id_ == 0);
	CHECK(a->requests_.empty());
}

// "Recompute": the answers of the analyses stopped do not come to the positions cleared.
static void test_recompute()
{
	auto a = make_app();
	start_engine(*a);
	const PosKey key = key_of(a->game_.now());
	a->book_id_ = 81;
	a->graph_id_ = 82;
	a->requests_[81] = {Request::Book, key};
	a->requests_[82] = {Request::Graph, key};
	a->cache_.get(a->game_.now()).book_done = true;
	a->on_command(cmd::Recompute);
	feed(*a, "!@move id=81 sq=f5 book=1 value=55 stage=7\n!@end id=82 ms=1\n!@cancel id=81\n");
	const PosInfo *info = a->cache_.peek(a->game_.now());
	CHECK(info && !info->book_done && !info->find(parse_square("f5"))->in_book);
}

// The expected line of the move under the mouse is drawn on the board only when it is set so (off at first).
static void test_pv_on_board()
{
	auto a = make_app();
	a->show_eval_ = true;
	MoveEval *m = a->cache_.get(a->game_.now()).find(parse_square("f5"));
	m->searched = true;
	m->depth = 10;
	m->pv = {parse_square("f5"), parse_square("d6"), parse_square("c3")};
	a->hover_sq_ = parse_square("f5");
	CHECK(a->cell_view(parse_square("d6")).pv_num == 0);
	std::wstring err;
	CHECK(a->script_command("pv on", 1, &err));
	CHECK(a->cell_view(parse_square("d6")).pv_num == 1 && a->cell_view(parse_square("c3")).pv_num == 2);
	CHECK(a->script_command("pv off", 1, &err));
	CHECK(a->cell_view(parse_square("d6")).pv_num == 0);
}

// The precision stages can be left out (heavy books): book moveinfo is asked without them, a book move shows its
// value without the level of the search; with them again, the position is asked again.
static void test_precision_off()
{
	auto a = make_app();
	start_engine(*a);
	a->show_eval_ = a->use_book_ = true;
	a->count_ = false;
	std::wstring err;
	CHECK(a->script_command("precision off", 1, &err));
	a->request_analysis();
	const int off = a->book_id_;
	CHECK(off != 0 && a->requests_.count(off) && a->requests_[off].params.find("parts=umi") != std::string::npos &&
	      a->requests_[off].params.find("parts=stage") == std::string::npos);
	MoveEval *m = a->cache_.get(a->game_.now()).find(parse_square("f5"));
	m->searched = true;
	m->depth = 12;
	feed(*a, "!@begin id=" + std::to_string(off) + " ply=0 turn=black moves=4 book=1\n!@move id=" + std::to_string(off) +
	             " sq=f5 book=1 value=3 level=4 lines=10\n!@end id=" + std::to_string(off) + " ms=0\n");
	const CellView v = a->cell_view(parse_square("f5"));
	CHECK(v.has_value && v.value == 3 && v.stage == 0 && v.level == 0);
	CHECK(a->script_command("precision on", 1, &err));
	a->request_analysis();
	CHECK(a->book_id_ != 0 && a->book_id_ != off && a->requests_[a->book_id_].params.find("parts=stage") != std::string::npos);
}

// The position shown goes first: a point of the graph being computed for another position of the game is stopped
// while the position shown is not known (asked again later); one for the position shown becomes its request.
static void test_graph_yields()
{
	auto a = make_app();
	start_engine(*a);
	a->show_eval_ = a->use_book_ = true;
	a->count_ = false;
	for (const char *mv : {"f5", "d6", "c3", "d3"}) a->game_.play(parse_square(mv));
	a->game_.go_to(0);
	a->request_analysis();
	const int first = a->book_id_;
	CHECK(first != 0);
	feed(*a, "!@begin id=" + std::to_string(first) + " ply=0 turn=black moves=4 book=1\n!@end id=" + std::to_string(first) + " ms=0\n");
	const int graph = a->graph_id_;  // a point of the graph, asked after the position shown
	CHECK(graph != 0 && a->requests_.count(graph) && a->requests_[graph].kind == Request::Graph);
	const PosKey graph_pos = graph ? a->requests_[graph].pos : PosKey{};
	a->game_.go_to(3);
	a->request_analysis();
	CHECK(a->graph_id_ != graph && a->stopped_.count(graph) && a->book_id_ != 0);
	// the point of the graph asked for the position shown is the answer of the position
	feed(*a, "!@cancel id=" + std::to_string(graph) + "\n");
	a->game_.go_to(0);
	a->request_analysis();
	a->pump_graph();
	const int g2 = a->graph_id_;
	CHECK(g2 != 0);
	for (int i = 0; g2 && i <= a->game_.size(); ++i)
		if (key_of(a->game_.at(i)) == a->requests_[g2].pos) a->game_.go_to(i);
	a->request_analysis();
	CHECK(a->book_id_ == g2 && a->graph_id_ == 0 && a->requests_[g2].kind == Request::Book);
	(void) graph_pos;
}

// Every level of Edax (0 to 60) can be chosen.
static void test_levels()
{
	auto a = make_app();
	std::wstring err;
	CHECK(a->script_command("level 0", 1, &err) && a->level_ == 0);
	CHECK(a->script_command("level 60", 1, &err) && a->level_ == 60);
	CHECK(a->script_command("level 33", 1, &err) && a->level_ == 33);
	CHECK(!a->script_command("level 61", 1, &err) && a->level_ == 33);
}

// The positions of the game are searched in the background from the end (backward) or from the start (forward).
static void test_forward_analysis()
{
	auto a = make_app();
	start_search_engine(*a);
	a->show_eval_ = true;
	for (const char *mv : {"f5", "d6", "c3", "d3"}) a->game_.play(parse_square(mv));
	PosInfo &shown = a->cache_.get(a->game_.now());  // the position shown is known: the background goes on
	shown.search_done = true;
	shown.search_level = a->level_;
	std::wstring err;
	CHECK(a->script_command("analysis forward", 1, &err));
	CHECK(a->hint_busy_ && a->hint_pos_ == key_of(a->game_.at(0)));
	a->stop_search(1);
	CHECK(a->script_command("analysis backward", 1, &err));
	CHECK(a->hint_busy_ && a->hint_pos_ == key_of(a->game_.at(a->game_.size() - 1)));
	a->stop_search(1);
	CHECK(a->script_command("analysis off", 1, &err));
	CHECK(!a->hint_busy_);
}

// "Stop the background work" also stops the book analyses (the precision stages, the points of the graph) and the
// search of the other positions; nothing starts again before the position changes.
static void test_stop_background()
{
	auto a = make_app();
	start_engine(*a);
	a->show_eval_ = a->use_book_ = true;
	a->count_ = false;
	a->game_.play(parse_square("f5"));
	a->request_analysis();
	const int book = a->book_id_;
	CHECK(book != 0);
	a->on_command(cmd::TaskCancel);
	CHECK(a->book_id_ == 0 && a->stopped_.count(book));
	feed(*a, "!@cancel id=" + std::to_string(book) + "\n");
	CHECK(a->book_id_ == 0 && a->graph_id_ == 0 && a->requests_.empty());
	a->game_.play(parse_square("d6"));
	a->position_changed();
	CHECK(a->book_id_ != 0);
}

// The kept lines (bookmarks) are chosen from their pictures above the graph: "bookmark <n>" shows the n-th one.
static void test_bookmarks()
{
	auto a = make_app();
	a->kept_.clear();
	for (const char *mv : {"f5", "d6", "c3"}) a->game_.play(parse_square(mv));
	a->keep_now(true);
	a->set_game(Game{}, true);
	std::wstring err;
	CHECK(a->script_command("bookmark 1", 1, &err));
	CHECK(a->game_.size() == 3 && a->game_.cursor() == 3);
	CHECK(!a->script_command("bookmark 2", 1, &err));
	// asked again: at the same place it stays one, at another place of the same line it is a new one
	CHECK(a->script_command("keep", 1, &err) && a->kept_.size() == 1);
	a->game_.go_to(1);
	CHECK(a->script_command("keep", 1, &err) && a->kept_.size() == 2 && a->kept_[1].cursor == 1);
	CHECK(a->script_command("bookmark 1", 1, &err) && a->game_.cursor() == 3);
	CHECK(!a->script_command("tab kept", 1, &err));  // the tab of the kept lines is gone
	a->kept_.clear();
}

// The design is switched with one key (D) or "design toggle".
static void test_design_toggle()
{
	auto a = make_app();
	std::wstring err;
	const Design d = a->design_;
	CHECK(a->script_command("design toggle", 1, &err) && a->design_ != d);
	CHECK(a->script_command("design toggle", 1, &err) && a->design_ == d);
}

// With a real engine (app_test --real <engine exe> <book>, not run by ctest): the position changed again and again, so
// that the book analyses asked are stopped before their end; once the engine is quiet, every number stopped has had
// the end of its answer (none kept taken for good), and only the requests of the position shown are left.
static int real_engine(const std::wstring &exe, const std::wstring &book)
{
	auto a = make_app();
	a->show_eval_ = a->use_book_ = a->count_ = true;
	a->tab_ = Tab::Book;
	const std::wstring dir = exe.substr(0, exe.find_last_of(L'\\'));
	EngineSlot &s = a->eng_[0];
	s.generation = 1;
	const std::wstring args = L"-q -eval-file \"" + dir + L"\\data\\eval.dat\" -n-tasks 1 -level 2 -book-file \"" + book + L"\"";
	s.alive = s.proc.start(exe, args, dir, a->hwnd_, WM_ENGINE, engine_tag(0, 1), nullptr);
	s.loaded = true;
	CHECK(s.alive);
	a->send(0, "mode 3\n");
	auto pump = [&](int ms) {
		const ULONGLONG end = GetTickCount64() + ms;
		while (GetTickCount64() < end) {
			MSG m{};
			while (PeekMessageW(&m, a->hwnd_, WM_ENGINE, WM_ENGINE, PM_REMOVE)) a->on_engine(m.wParam, m.lParam != 0);
			Sleep(1);
		}
	};
	const char *line = "f5d6c3d3c4f4f6f3e6e7d7c5b6";
	size_t stopped_max = 0;
	for (int round = 0; round < 30; ++round) {
		a->game_ = Game{};
		for (int i = 0; line[i] && line[i + 1]; i += 2) {
			a->game_.play(parse_square(std::string(line + i, 2)));
			a->request_analysis();
			pump(round % 3 == 0 ? 0 : 5);
			stopped_max = std::max(stopped_max, a->stopped_.size());
		}
		a->base_ = round % 2 ? "deviate" : "deviate2";  // other settings: the points of the graph stopped too
	}
	a->request_analysis();  // (with the settings of now)
	// quiet: the answers of the position shown come to their end
	const ULONGLONG end = GetTickCount64() + 60000;
	while (GetTickCount64() < end && !(a->book_id_ == 0 && a->count_id_ == 0 && a->levels_id_ == 0 && a->best_id_ == 0 &&
	                                   a->graph_id_ == 0 && a->stopped_.empty()))
		pump(50);
	std::printf("stopped at most %zu at a time; now: stopped %zu, requests %zu (book %d graph %d missing %d)\n", stopped_max,
	            a->stopped_.size(), a->requests_.size(), a->book_id_, a->graph_id_, a->missing_id_);
	CHECK(stopped_max > 0);
	CHECK(a->stopped_.empty());
	for (const auto &[id, r] : a->requests_) CHECK(id == a->missing_id_);  // the list of missing lines may go on
	const PosInfo *info = a->cache_.peek(a->game_.now());
	CHECK(info && info->book_done && info->book_params == a->book_params());
	return failures ? 1 : 0;
}

// The light of the effects of each stage (app_test --glow <folder>, not run by ctest): the moves of a position get the
// stages 0 to 7; each square is drawn at many times with the effects on and off, and the light the effects add (the
// sum of the gains of brightness over the square, in pixels at full white) is written per stage, with its largest
// value and how much it moves from one time to the next. Pictures of the board go to the folder.
static bool render(App &a, std::vector<BYTE> &px, double t, bool board)
{
	Gfx &g = a.gfx_;
	auto *dc = g.dc();
	const UINT w = g.width(), h = g.height();
	const D2D1_PIXEL_FORMAT pf = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
	Microsoft::WRL::ComPtr<ID2D1Bitmap1> frame, read;
	if (FAILED(dc->CreateBitmap({w, h}, nullptr, 0, D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, pf, 96, 96), &frame))) return false;
	Microsoft::WRL::ComPtr<ID2D1Image> old;
	dc->GetTarget(&old);
	dc->SetTarget(frame.Get());
	dc->BeginDraw();
	dc->SetTransform(D2D1::Matrix3x2F::Identity());
	dc->Clear(theme(a.design_).bg);
	if (board) draw_board(a);
	for (int sq = 0; sq < 64; ++sq) draw_cell(a, sq, t);
	const HRESULT hr = dc->EndDraw();
	dc->SetTarget(old.Get());
	if (FAILED(hr)) return false;
	if (FAILED(dc->CreateBitmap({w, h}, nullptr, 0, D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, pf, 96, 96), &read)))
		return false;
	const D2D1_POINT_2U at{0, 0};
	const D2D1_RECT_U all{0, 0, w, h};
	if (FAILED(read->CopyFromBitmap(&at, frame.Get(), &all))) return false;
	D2D1_MAPPED_RECT mr{};
	if (FAILED(read->Map(D2D1_MAP_OPTIONS_READ, &mr))) return false;
	px.resize(static_cast<size_t>(w) * h * 4);
	for (UINT y = 0; y < h; ++y) std::memcpy(px.data() + static_cast<size_t>(y) * w * 4, mr.bits + static_cast<size_t>(y) * mr.pitch, w * 4);
	read->Unmap();
	return true;
}

static double brightness(const std::vector<BYTE> &px, UINT w, const D2D1_RECT_F &r, const std::vector<BYTE> *base, double *gain)
{
	double sum = 0, g = 0;
	for (int y = static_cast<int>(r.top); y < static_cast<int>(r.bottom); ++y)
		for (int x = static_cast<int>(r.left); x < static_cast<int>(r.right); ++x) {
			const size_t i = (static_cast<size_t>(y) * w + x) * 4;
			auto lum = [&](const std::vector<BYTE> &p) { return (0.0722 * p[i] + 0.7152 * p[i + 1] + 0.2126 * p[i + 2]) / 255.0; };
			const double v = lum(px);
			sum += v;
			if (base) g += std::max(0.0, v - lum(*base));
		}
	if (gain) *gain = g;
	return sum;
}

static int glow_measure(const std::wstring &dir)
{
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	auto a = make_app();
	DestroyWindow(a->hwnd_);
	a->hwnd_ = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 900, 900, nullptr, nullptr, nullptr, nullptr);
	if (!a->gfx_.init(a->hwnd_)) return 2;
	a->gfx_.resize();
	a->panel_hidden_ = true;
	a->show_eval_ = a->use_book_ = true;
	a->layout();
	// a position with many moves: the first eight get the stages 0 to 7, all with the same value
	for (const char *mv : {"f5", "d6", "c3", "d3", "c4", "f4", "f6", "f3", "e6", "e7"}) a->game_.play(parse_square(mv));
	PosInfo &info = a->cache_.get(a->game_.now());
	std::vector<int> sqs;
	for (auto &m : info.moves) {
		if (sqs.size() == 8) break;
		m.book_row = m.in_book = true;
		m.book_value = 4;
		m.book_level = 21;
		m.stage = static_cast<int>(sqs.size());
		sqs.push_back(m.sq);
	}
	if (sqs.size() < 8) return 3;
	const int frames = 120;  // 6 seconds
	std::printf("design top stage light peak motion\n");
	for (const Design d : {Design::Standard, Design::Paw})
		for (const TopEffect top : {TopEffect::Strong, TopEffect::Normal, TopEffect::Weak}) {
			a->design_ = d;
			a->top_effect_ = top;
			a->gfx_.clear_shapes();
			std::vector<BYTE> off, on, prev;
			a->effects_ = Effects::Off;
			if (!render(*a, off, 0, false)) return 4;
			a->effects_ = Effects::On;
			a->prepare_glows();
			double light[8] = {}, peak[8] = {}, motion[8] = {};
			for (int f = 0; f < frames; ++f) {
				const double t = 0.05 * f;
				if (!render(*a, on, t, false)) return 4;
				for (int k = 0; k < 8; ++k) {
					const D2D1_RECT_F r = a->lay_.cell_rect(sqs[static_cast<size_t>(k)]);
					double g = 0;
					brightness(on, a->gfx_.width(), r, &off, &g);
					light[k] += g / frames;
					peak[k] = std::max(peak[k], g);
					if (!prev.empty()) {
						double mv = 0;
						brightness(on, a->gfx_.width(), r, &prev, &mv);
						motion[k] += mv / (frames - 1);
					}
				}
				prev = on;
			}
			for (int k = 3; k < 8; ++k) {
				if (k == 7 || top == TopEffect::Normal)
					std::printf("%s %s %d %.0f %.0f %.1f\n", d == Design::Paw ? "paw" : "standard",
					            top == TopEffect::Strong ? "strong" : top == TopEffect::Weak ? "weak" : "normal", k, light[k], peak[k], motion[k]);
			}
			// the squares of the stages 3 to 7 side by side, at a few times
			if (top == TopEffect::Normal) {
				for (const double t : {0.4, 1.3, 2.2}) {
					for (int k = 3; k < 8; ++k) {
						const D2D1_RECT_F r = a->lay_.cell_rect(sqs[static_cast<size_t>(k)]);
						const std::wstring name = dir + L"\\cell-" + (d == Design::Paw ? L"paw" : L"std") + L"-" + std::to_wstring(k) + L"-" +
						                          std::to_wstring(static_cast<int>(t * 10)) + L".png";
						a->gfx_.save_png(name, {static_cast<UINT32>(r.left), static_cast<UINT32>(r.top), static_cast<UINT32>(r.right), static_cast<UINT32>(r.bottom)}, [&] {
							a->gfx_.dc()->Clear(theme(a->design_).bg);
							draw_board(*a);
							for (int sq = 0; sq < 64; ++sq) draw_cell(*a, sq, t);
						});
					}
				}
			}
			// pictures of the board at two times
			if (top == TopEffect::Normal || top == TopEffect::Strong) {
				for (const double t : {0.4, 1.3}) {
					const std::wstring name = dir + L"\\glow-" + (d == Design::Paw ? L"paw" : L"std") + L"-" +
					                          (top == TopEffect::Strong ? L"strong" : L"normal") + L"-" + std::to_wstring(static_cast<int>(t * 10)) + L".png";
					a->gfx_.save_png(name, {0, 0, a->gfx_.width(), a->gfx_.height()}, [&] {
						a->gfx_.dc()->Clear(theme(a->design_).bg);
						draw_board(*a);
						for (int sq = 0; sq < 64; ++sq) draw_cell(*a, sq, t);
					});
				}
			}
		}
	return 0;
}

// The lines an engine wrote (app_test --replay <log>, not run by ctest), given again to the app through the engine
// of the tests: every book analysis of the log comes to its end however its lines were cut.
static int replay(const std::wstring &file)
{
	FILE *f = nullptr;
	if (_wfopen_s(&f, file.c_str(), L"rb") || !f) return 2;
	std::string text, line;
	for (int c; (c = std::fgetc(f)) != EOF;) text += static_cast<char>(c);
	std::fclose(f);
	auto a = make_app();
	start_engine(*a);
	const PosKey key = key_of(a->game_.now());
	std::string out;
	int ends = 0;
	for (size_t i = 0, e; i < text.size(); i = e + 1) {
		e = text.find('\n', i);
		if (e == std::string::npos) e = text.size();
		line = text.substr(i, e - i);
		if (!line.empty() && line.back() == '\r') line.pop_back();
		for (size_t k = 0; (k = line.find("@begin id=", k)) != std::string::npos; ++k) {
			const int id = std::atoi(line.c_str() + k + 10);
			a->requests_[id] = {Request::Book, key};
			++ends;
		}
		out += "!" + line + "\n";
	}
	feed(*a, out);
	std::printf("analyses %d: %zu left without their end\n", ends, a->requests_.size());
	CHECK(a->requests_.empty());
	return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
	wchar_t exe[MAX_PATH * 4];
	GetModuleFileNameW(nullptr, exe, MAX_PATH * 4);
	self = exe;
	if (argc == 3 && std::string(argv[1]) == "--replay") {
		lang::load(L".", "ja");
		const int r = replay(widen(argv[2]));
		std::printf(r ? "%d failures\n" : "all passed\n", failures);
		return r;
	}
	if (argc == 4 && std::string(argv[1]) == "--glow") {
		lang::load(widen(argv[3]), "ja");
		return glow_measure(widen(argv[2]));
	}
	if (argc == 4 && std::string(argv[1]) == "--real") {
		lang::load(L".", "ja");
		const int r = real_engine(widen(argv[2]), widen(argv[3]));
		std::printf(r ? "%d failures\n" : "all passed\n", failures);
		return r;
	}
	if (argc > 1 && std::string(argv[1]) == "--engine") {
		for (std::string s; std::getline(std::cin, s);)
			if (s.starts_with("!")) std::cout << s.substr(1) << std::endl;
		return 0;
	}
	lang::load(argc > 1 ? widen(argv[1]) : std::wstring(L"."), "ja");
	std::setvbuf(stdout, nullptr, _IONBF, 0);  // (the lines before a crash are kept)
	std::printf("%s\n", "test_mixed_lines");
	test_mixed_lines();
	std::printf("%s\n", "test_old_settings");
	test_old_settings();
	std::printf("%s\n", "test_settings_keep_counts");
	test_settings_keep_counts();
	std::printf("%s\n", "test_command_letters");
	test_command_letters();
	std::printf("%s\n", "test_stopped_numbers");
	test_stopped_numbers();
	std::printf("%s\n", "test_engine_ended");
	test_engine_ended();
	std::printf("%s\n", "test_recompute");
	test_recompute();
	std::printf("%s\n", "test_reduce_script");
	test_reduce_script();
	std::printf("%s\n", "test_reduce_unknown");
	test_reduce_unknown();
	std::printf("%s\n", "test_pv_on_board");
	test_pv_on_board();
	std::printf("%s\n", "test_precision_off");
	test_precision_off();
	std::printf("%s\n", "test_graph_yields");
	test_graph_yields();
	std::printf("%s\n", "test_levels");
	test_levels();
	std::printf("%s\n", "test_forward_analysis");
	test_forward_analysis();
	std::printf("%s\n", "test_stop_background");
	test_stop_background();
	std::printf("%s\n", "test_bookmarks");
	test_bookmarks();
	std::printf("%s\n", "test_design_toggle");
	test_design_toggle();
	if (failures) {
		std::printf("%d failures\n", failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
