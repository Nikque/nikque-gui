// The app with the main engine: the lines the engine writes and what becomes of the requests. The engine is this
// program started again with "--engine": it writes back the lines it reads that begin with '!' (without the '!').
#include <windows.h>

#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <share.h>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "app.h"
#include "form.h"
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
static std::wstring src_dir;  // the folder of the sources (the first argument)

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

// the main engine: this program, writing back what it is given ("--engine read": and keeping the rest in a file)
static void start_engine(App &a, const wchar_t *args = L"--engine")
{
	EngineSlot &s = a.eng_[0];
	s.generation = 1;
	s.alive = s.proc.start(self, args, a.temp_dir_, a.hwnd_, WM_ENGINE, engine_tag(0, 1), nullptr);
	s.loaded = true;
	CHECK(s.alive);
}

// the file of the lines an engine started with "--engine read" has read, but those it writes back
static std::wstring engine_read_file()
{
	wchar_t tmp[MAX_PATH];
	GetTempPathW(MAX_PATH, tmp);
	return std::wstring(tmp) + L"ng-app-test-read.txt";
}

static std::string engine_read()
{
	std::string text;
	if (FILE *f = _wfsopen(engine_read_file().c_str(), L"rb", _SH_DENYNO)) {
		for (int c; (c = std::fgetc(f)) != EOF;) text += static_cast<char>(c);
		std::fclose(f);
	}
	return text;
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

// the values of the book are asked before the rest, a position at a time
static void answer_values(App &a)
{
	while (const int id = a.values_id_) feed(a, "!@end id=" + std::to_string(id) + " ms=0\n");
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

// the position has been looked up in the book file (the file is open then)
static bool peeked(App &a, const Position &p)
{
	const ULONGLONG end = GetTickCount64() + 10000;
	while (GetTickCount64() < end && !a.cache_.get(p).book_peeked) {
		pump_app(a);
		Sleep(1);
	}
	return a.cache_.get(p).book_peeked;
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

// "book deviate2 2 4 export <file> ends" of a script (as the window of the book menu): the command goes to the main
// engine from the position shown, its result line gives the number of lines in the log, and the analyses of the book
// stay (the book does not change). The words are checked first.
static void test_deviate_export()
{
	auto a = make_app();
	start_engine(*a);
	a->game_.play(parse_square("f5"));
	std::wstring err;
	const std::wstring file = a->temp_dir_ + L"\\ng-export.txt";
	// a main engine that has not said it has the export (an older one would expand the book): nothing is sent
	CHECK(!a->eng_[0].exports);
	CHECK(!a->script_command("book deviate2 2 4 export " + narrow(file) + " ends", 1, &err) && !a->learning_);
	// the answer to the first "stop id=<n>" says it: "@cancel id=<n> state=none export=1"
	a->eng_[0].probe_id = 42;
	a->eng_[0].proc.send("!@cancel id=42 state=none export=1\n");
	for (const ULONGLONG end = GetTickCount64() + 10000; a->eng_[0].probe_id && GetTickCount64() < end; Sleep(1)) {
		MSG m{};
		while (PeekMessageW(&m, a->hwnd_, WM_ENGINE, WM_ENGINE, PM_REMOVE)) a->on_engine(m.wParam, m.lParam != 0);
	}
	CHECK(a->eng_[0].probe_id == 0 && a->eng_[0].exports);
	CHECK(a->script_command("book deviate2 2 4 export " + narrow(file) + " ends", 1, &err));
	CHECK(a->learning_ && a->learn_cmd_.starts_with("book deviate2 2 4 export ") && a->learn_cmd_.ends_with(" ends"));
	CHECK(a->ini_.get("learn", "last_command").empty());  // (not a learning to go on with)
	a->cache_.get(a->game_.now()).values_done = true;
	feed(*a, "!Book deviate2 2 4 export ends...finished: 7 lines written (x)\n!echo: \"M" + std::to_string(a->learn_end_id_) +
	             "\" is not on or off; ignored\n");
	CHECK(!a->learning_);
	CHECK(logged(*a, lang::f("devexp.written", {L"7", file})));
	CHECK(logged(*a, lang::t("devexp.ended").substr(0, 5)) && !logged(*a, lang::t("learn.ended").substr(0, 5)));  // (not "learning")
	CHECK(a->cache_.get(a->game_.now()).values_done);

	// leaf when the kind is left out; a position out of the book
	CHECK(a->script_command("book deviate 1 2 export " + narrow(file), 1, &err));
	CHECK(a->learning_ && a->learn_cmd_.starts_with("book deviate 1 2 export ") && a->learn_cmd_.ends_with(" leaf"));
	feed(*a, "!Book deviate 1 2 export leaf...finished: the position is not in the book, 0 lines written (x)\n!echo: \"M" +
	             std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
	CHECK(!a->learning_);
	CHECK(logged(*a, lang::f("devexp.notinbook", {file})));

	// a name with spaces in "..." (as the files of edax and tool), with or without the kind: the engine is given a name
	// without spaces (it reads a name up to a space)
	const std::wstring dir = a->temp_dir_ + L"\\ng dir sp";
	CreateDirectoryW(dir.c_str(), nullptr);
	const std::wstring spaced = dir + L"\\ng export.txt";
	for (const std::string kind : {" all", ""}) {
		CHECK(a->script_command("book deviate2 2 4 export \"" + narrow(spaced) + "\"" + kind, 1, &err));
		const std::string head = "book deviate2 2 4 export ", tail = kind.empty() ? " leaf" : kind;
		CHECK(a->learning_ && a->learn_cmd_.starts_with(head) && a->learn_cmd_.ends_with(tail));
		CHECK(a->learn_cmd_.size() > head.size() + tail.size() &&
		      a->learn_cmd_.substr(head.size(), a->learn_cmd_.size() - head.size() - tail.size()).find(' ') == std::string::npos);
		feed(*a, "!Book deviate2 2 4 export" + tail + "...finished: 3 lines written (x)\n!echo: \"M" +
		             std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
		CHECK(!a->learning_);
		CHECK(logged(*a, lang::f("devexp.written", {L"3", spaced})));
	}

	for (const char *bad : {"book deviate4 2 4 export x.txt", "book deviate2 2 4 export x.txt some", "book deviate2 2 export x.txt",
	                        "book deviate 2 70 export x.txt", "book deviate2 2 255 export x.txt", "book deviate2 2 4 export",
	                        "book deviate2 2 4 expand x.txt", "book deviate2 2 4 export a b.txt ends", "book deviate2 2 4 export \"a b.txt ends",
	                        "book deviate2 2 4 export \"a b.txt\"ends", "book deviate2 2 4 export \"a b.txt\" ends more",
	                        "book deviate2 2 4 export \"\" ends"})
		CHECK(!a->script_command(bad, 1, &err));
	CHECK(!a->learning_);
}

// An export of the lines of book deviate that ends without its end marker (here the main engine ends in the middle of
// it; stopping it ends it so too) leaves nothing for the next command: that one is a learning to go on with, said as a
// learning, and the analyses of the book go after it.
static void test_export_then_learning()
{
	auto a = make_app();
	start_engine(*a);
	a->eng_[0].exports = true;
	CHECK(a->start_deviate_export("deviate2", 2, 4, "ends", a->temp_dir_ + L"\\ng-export.txt"));
	CHECK(a->learning_ && a->learn_keeps_book_);
	a->on_engine(engine_tag(0, 1), true);  // (the engine has ended)
	CHECK(!a->learning_);
	a->eng_[0].alive = true;  // (started again)
	a->start_learning("book deviate 2 4");
	CHECK(a->learning_ && a->ini_.get("learn", "last_command") == "book deviate 2 4");
	CHECK(logged(*a, lang::f("learn.started", {L"book deviate 2 4"})));
	a->cache_.get(a->game_.now()).values_done = true;
	feed(*a, "!echo: \"M" + std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
	CHECK(!a->learning_);
	CHECK(logged(*a, lang::t("learn.ended").substr(0, 5)));
	CHECK(!a->cache_.get(a->game_.now()).values_done);  // (the book has changed)
}

// The lines of best moves to a file (book bestlines ... file=): the command goes to the main engine from the position
// shown, and its answer gives the numbers in the log.
static void test_best_export()
{
	auto a = make_app();
	start_engine(*a, L"--engine read");
	a->game_.play(parse_square("f5"));
	const std::wstring file = a->temp_dir_ + L"\\ng-best.txt";
	CHECK(a->start_best_export(2, 30, "follow", "all", file));
	const int id = only_request(*a);
	CHECK(id != 0 && a->requests_[id].kind == Request::BestExport && logged(*a, lang::f("best.started", {file})));
	feed(*a, "!@end id=" + std::to_string(id) + " written=3 lines=2 incomplete_ends=1\n");
	CHECK(a->requests_.empty() && logged(*a, lang::f("best.written", {L"3", file, L"2", L"1"})));
	const std::string got = engine_read();
	CHECK(got.find("play f5\nbook bestlines id=" + std::to_string(id) + " loss=2 depth=30 solved=follow which=all file=\"") !=
	      std::string::npos);
	// no line written: newedax makes no file then, which the log does not say was written
	DeleteFileW(file.c_str());
	CHECK(a->start_best_export(2, 30, "follow", "all", file));
	const int id0 = only_request(*a);
	feed(*a, "!@end id=" + std::to_string(id0) + " written=0 lines=0 incomplete_ends=0\n");
	CHECK(a->requests_.empty() && logged(*a, lang::f("best.none", {file})) &&
	      !logged(*a, lang::f("best.written", {L"0", file, L"0", L"0"})));
	// an engine that makes the file, empty: as written
	FILE *empty = nullptr;
	if (_wfopen_s(&empty, file.c_str(), L"wb") == 0 && empty) fclose(empty);
	CHECK(a->start_best_export(2, 30, "follow", "all", file));
	const int id1 = only_request(*a);
	feed(*a, "!@end id=" + std::to_string(id1) + " written=0 lines=0 incomplete_ends=0\n");
	CHECK(a->requests_.empty() && logged(*a, lang::f("best.written", {L"0", file, L"0", L"0"})));
	DeleteFileW(file.c_str());
	// a learning started, or the engine gone, while the windows were open: nothing is sent
	a->learning_ = true;
	CHECK(!a->start_best_export(0, 60, "stop", "all", file) && a->requests_.empty());
	a->learning_ = false;
	a->eng_[0].alive = false;
	CHECK(!a->start_best_export(0, 60, "stop", "all", file) && a->requests_.empty());
}

// While the main engine reads its book it has not said yet whether it has the export (its answer to the first
// "stop id=" comes after the book): a script is told so, not that the engine is an old one.
static void test_export_while_loading()
{
	auto a = make_app();
	start_engine(*a);
	a->eng_[0].loaded = false;
	a->eng_[0].probe_id = 77;  // (as start_engines sends it)
	std::wstring err;
	CHECK(!a->script_command("book deviate2 2 4 export x.txt ends", 1, &err) && !a->learning_);
	CHECK(err == lang::t("devexp.loading"));
	a->eng_[0].loaded = true;
	a->eng_[0].probe_id = 0;  // (an engine that answered without the export)
	CHECK(!a->script_command("book deviate2 2 4 export x.txt ends", 1, &err) && err == lang::t("devexp.old_engine"));
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
	CHECK(!a->script_command("book reduce 2 4 here 1", 1, &err));
	CHECK(a->requests_.empty());
}

// The count of book reduce goes when the background work is stopped (while it counts, or after its answer before it
// is taken) and when the engines start again: no book command follows it, and a script waits no longer.
static void test_reduce_stopped()
{
	for (int k = 0; k < 3; ++k) {
		auto a = make_app();
		start_engine(*a);
		std::wstring err;
		CHECK(a->script_command("book reduce 2 4", 1, &err));
		const int id = only_request(*a);
		CHECK(id != 0);
		const std::string end = "!@end id=" + std::to_string(id) + " kept=6 removed=1541 positions=1547 ms=0\n";
		if (k == 0) {
			a->stop_background();
			CHECK(a->requests_.empty() && a->stopped_.count(id));  // the engine is asked to stop it
			feed(*a, end);
		} else {
			feed(*a, end);
			if (k == 1) {
				a->stop_background();
			} else {
				a->stop_engines();
				start_engine(*a);
			}
		}
		pump_app(*a);
		CHECK(!a->learning_);
		CHECK(a->reduce_.state == App::ReduceJob::None);
		CHECK(!a->script_waiting());
		CHECK(logged(*a, lang::t("reduce.stopped")));
		a->learning_ = false;
	}
}

// A damaged book (a link to a square already taken, a link back to the same position) is not counted: nothing is done.
static void test_reduce_damaged()
{
	auto a = make_app();
	start_engine(*a);
	std::wstring err;
	CHECK(a->script_command("book reduce 0 0", 1, &err));
	const int id = only_request(*a);
	feed(*a, "!@error id=" + std::to_string(id) + " damaged=1\n!@end id=" + std::to_string(id) + " error=1\n");
	pump_app(*a);
	CHECK(!a->learning_ && a->requests_.empty() && !a->script_waiting());
	CHECK(logged(*a, lang::f("reduce.damaged", {lang::t("reduce.at.initial")})));
	a->learning_ = false;
}

// The count of book reduce says in the log where it counts from; a count that removes nothing, a position out of the
// book and one deeper than the book change nothing, and the log says so with the place.
static void test_reduce_results()
{
	auto a = make_app();
	start_engine(*a);
	a->game_.play(parse_square("f5"));
	const std::wstring initial = lang::t("reduce.at.initial"), here = lang::t("reduce.at.here");
	struct Case {
		const char *command, *answer;
		std::wstring counting, result;
	};
	const Case cases[] = {
		{"book reduce 2 4", " kept=1547 removed=0 positions=1547", lang::f("reduce.counting", {initial, L"2", L"4"}),
		 lang::f("reduce.nothing", {initial, L"2", L"4", lang::number(1547)})},
		{"book reduce 1 3 here", " kept=0 removed=0 positions=6 notinbook=1", lang::f("reduce.counting", {here, L"1", L"3"}),
		 lang::f("reduce.notinbook", {here})},
		{"book reduce 2 4 here", " kept=0 removed=0 positions=6 deep=1", lang::f("reduce.counting", {here, L"2", L"4"}),
		 lang::f("reduce.deep", {here})},
	};
	for (const Case &c : cases) {
		std::wstring err;
		CHECK(a->script_command(c.command, 1, &err));
		const int id = only_request(*a);
		CHECK(id != 0);
		CHECK(logged(*a, c.counting));
		feed(*a, "!@end id=" + std::to_string(id) + c.answer + " ms=0\n");
		pump_app(*a);
		CHECK(!a->learning_ && a->requests_.empty() && !a->script_waiting());
		CHECK(a->reduce_.state == App::ReduceJob::None);
		CHECK(logged(*a, c.result));
	}
}

// The count of book reduce has come, but meanwhile a learning has started or the main engine has gone: book reduce is
// not done, and the log says why.
static void test_reduce_not_run()
{
	for (const bool gone : {false, true}) {
		auto a = make_app();
		start_engine(*a);
		std::wstring err;
		CHECK(a->script_command("book reduce 2 4", 1, &err));
		const int id = only_request(*a);
		CHECK(id != 0);
		feed(*a, "!@end id=" + std::to_string(id) + " kept=6 removed=1541 positions=1547 ms=0\n");
		if (gone) a->eng_[0].alive = false;
		else a->learning_ = true;
		pump_app(*a);
		CHECK(a->reduce_.state == App::ReduceJob::None && a->learn_cmd_ != "book reduce 2 4");
		CHECK(logged(*a, lang::f("reduce.not_run", {lang::t(gone ? "learn.no_engine" : "learn.busy")})));
		a->learning_ = false;
	}
}

// The missing lines of the book tab become jobs of the learning queue: from the start of the game shown up to the
// position of the line, book store, then the command of the stage base with X and Y. The queue runs them one after
// another and says when it has ended.
static void test_learn_queue()
{
	auto a = make_app();
	start_engine(*a);
	a->game_.play(parse_square("f5"));
	a->base_ = "deviate2";
	a->loss_x_ = 3;
	a->loss_y_ = 5;
	a->queue_missing();  // no missing line: nothing is put in
	CHECK(a->learn_queue_.empty());
	MissingLine one, two;
	one.moves = {parse_square("d6")};
	two.moves = {parse_square("f6"), parse_square("e6")};
	a->missing_.pos = key_of(a->game_.now());  // (the list of the position shown)
	a->missing_.lines = {one, two};
	a->queue_missing();
	CHECK(a->learn_queue_.size() == 2);
	const std::string command = "book store\nbook deviate2 3 5";
	for (const auto &j : a->learn_queue_)
		CHECK(j.command == command && j.start.to_board_string() == Position{}.to_board_string());
	CHECK(a->learn_queue_.size() == 2 && a->learn_queue_[0].moves == "f5d6" && a->learn_queue_[1].moves == "f5f6e6");
	CHECK(logged(*a, lang::f("queue.added", {L"2", L"2", L"book deviate2 3 5"})));
	CHECK(!a->learning_);  // (put in only)
	a->queue_running_ = true;
	a->next_in_queue();
	CHECK(a->learning_ && a->learn_cmd_ == "book store/book deviate2 3 5" && a->learn_queue_.size() == 1);
	CHECK(a->ini_.get("learn", "last_moves") == "f5d6");
	feed(*a, "!echo: \"M" + std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
	CHECK(a->learning_ && a->learn_queue_.empty() && a->ini_.get("learn", "last_moves") == "f5f6e6");
	feed(*a, "!echo: \"M" + std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
	CHECK(!a->learning_ && !a->queue_running_);
	CHECK(logged(*a, lang::t("queue.done")));
	// started without the main engine (the menu, a script): nothing starts, which would be learning for ever (the
	// commands not sent, no end coming), and the jobs stay
	auto b = make_app();
	b->learn_queue_.push_back({Position{}, "f5", "book store"});
	int asked = 0;
	b->message_hook_ = [&](const std::wstring &, UINT) {
		++asked;
		return IDOK;
	};
	b->on_command(cmd::QueueStart);
	CHECK(!b->learning_ && !b->queue_running_ && b->learn_queue_.size() == 1 && asked == 0);
	CHECK(b->status_ == lang::t("learn.no_engine"));
	std::wstring err;
	CHECK(!b->script_command("learn start", 1, &err) && err == lang::t("learn.no_engine"));
	CHECK(!b->learning_ && !b->queue_running_ && b->learn_queue_.size() == 1);
	b->queue_running_ = true;
	b->next_in_queue();
	CHECK(!b->learning_ && !b->queue_running_ && b->learn_queue_.size() == 1);
}

// The settings of book learning in config.ini go to the main engine as commands, in the order of the window; an empty
// value (the engine's own setting) and one that cannot be read are left out.
static void test_learn_options()
{
	auto a = make_app();
	CHECK(a->learn_options_commands().empty());
	a->ini_.set("learn_options", "book-store-tasks", "4");
	a->ini_.set("learn_options", "book-expand-tasks", "auto");
	a->ini_.set("learn_options", "book-seed", "12x");
	a->ini_.set("learn_options", "book-save-interval", "1234567890");  // (more than 9 figures)
	a->ini_.set("learn_options", "book-reproducible", "on");
	a->ini_.set("learn_options", "book-store-auto-save", "off");
	a->ini_.set("learn_options", "book-merge-auto-save", "");
	CHECK(a->learn_options_commands() == "book-expand-tasks auto\nbook-store-tasks 4\nbook-reproducible on\nbook-store-auto-save off\n");
}

// The learning settings changed while the main engine learns go to it when that learning has ended, before the next
// job of the queue (a learning in progress keeps its own); with no engine running they go when it starts. The log
// says when.
static void test_learn_options_later()
{
	std::vector<std::wstring> texts(9);
	{
		auto a = make_app();  // (no engine)
		texts[0] = L"4";
		a->apply_learn_options(texts);
		CHECK(a->ini_.get("learn_options", "book-expand-tasks") == "4");
		CHECK(logged(*a, lang::t("learnopt.at_start")) && !logged(*a, lang::t("learnopt.sent")));
	}
	auto a = make_app();
	start_engine(*a, L"--engine read");
	a->learn_queue_.push_back({Position{}, "f5", "book store"});
	a->learn_queue_.push_back({Position{}, "f5d6", "book deviate 2 4"});
	a->queue_running_ = true;
	a->next_in_queue();
	CHECK(a->learning_);
	texts[0].clear();
	texts[1] = L"3";
	a->apply_learn_options(texts);
	CHECK(a->ini_.get("learn_options", "book-store-tasks") == "3");
	CHECK(logged(*a, lang::t("learnopt.after_learning")) && !logged(*a, lang::t("learnopt.sent")));
	feed(*a, "");
	CHECK(engine_read().find("book-store-tasks") == std::string::npos);
	feed(*a, "!echo: \"M" + std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
	CHECK(a->learning_ && a->learn_queue_.empty());  // (the second job)
	feed(*a, "");
	const std::string got = engine_read();
	const size_t sent = got.find("book-store-tasks 3\n"), next = got.find("book deviate 2 4");
	CHECK(sent != std::string::npos && next != std::string::npos && sent < next);
	CHECK(logged(*a, lang::t("learnopt.sent")));
	feed(*a, "!echo: \"M" + std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
	CHECK(!a->learning_);
	feed(*a, "");
	CHECK(engine_read().find("book-store-tasks 3\n", sent + 1) == std::string::npos);  // (sent once)
	// while it does not learn: at once
	texts[1] = L"5";
	a->apply_learn_options(texts);
	feed(*a, "");
	CHECK(engine_read().find("book-store-tasks 5\n") != std::string::npos);
}

// A learning setting emptied (the engine's own setting) cannot be sent: the engine reads no option without a value and
// has no word for its own setting. The log says that the main engine keeps the value sent before until it starts again;
// a setting it was never sent says nothing.
static void test_learn_options_emptied()
{
	std::vector<std::wstring> texts(9);
	const std::wstring said = lang::f("learnopt.emptied", {L"book-store-tasks"});
	auto a = make_app();
	start_engine(*a, L"--engine read");
	texts[1] = L"3";
	a->apply_learn_options(texts);
	CHECK(logged(*a, lang::t("learnopt.sent")) && !logged(*a, said));
	texts[1].clear();
	texts[0] = L"2";
	a->apply_learn_options(texts);
	CHECK(logged(*a, said));
	feed(*a, "");
	const std::string got = engine_read();
	CHECK(got.find("book-store-tasks 3\n") != std::string::npos && got.find("book-expand-tasks 2\n") != std::string::npos);
	a->log_.clear();
	texts[0].clear();  // (book-expand-tasks: sent, emptied now)
	a->apply_learn_options(texts);
	CHECK(logged(*a, lang::f("learnopt.emptied", {L"book-expand-tasks, book-store-tasks"})));
	auto b = make_app();  // (nothing sent to its engine)
	start_engine(*b);
	b->apply_learn_options(std::vector<std::wstring>(9));
	CHECK(logged(*b, lang::t("learnopt.sent")) && !logged(*b, said));
}

// The missing lines are those of the position they were asked for: once another position is shown (the list shown in
// the Book tab, then moves played in another tab) they are not put in the learning queue.
static void test_missing_other_position()
{
	auto a = make_app();
	start_engine(*a);
	a->game_.play(parse_square("f5"));
	MissingLine one;
	one.moves = {parse_square("d6")};
	a->missing_.pos = key_of(a->game_.now());
	a->missing_.lines = {one};
	a->game_.play(parse_square("d6"));
	a->queue_missing();
	CHECK(a->learn_queue_.empty());
	CHECK(a->status_ == lang::t("status.missing_none"));
	a->status_.clear();
	a->export_missing();  // (it ends before its save dialog)
	CHECK(a->status_ == lang::t("status.missing_none"));
	a->game_.go_to(1);  // (back to the position of the list)
	a->queue_missing();
	CHECK(a->learn_queue_.size() == 1 && a->learn_queue_[0].moves == "f5d6");
	// written as games, said in the log with the file, as the other exports; a file that cannot be opened the same
	const std::wstring file = a->temp_dir_ + L"ng-missing-test.txt", bad = a->temp_dir_ + L"ng-no-such-folder\\missing.txt";
	a->status_.clear();
	CHECK(a->write_missing(file));
	std::string text;
	CHECK(read_file(file, &text) && text == "f5d6\r\n" && logged(*a, lang::f("status.missing_written", {L"1", file})));
	CHECK(!a->write_missing(bad) && logged(*a, lang::f("status.missing_failed", {bad})) && a->status_ != lang::t("status.write_failed"));
	DeleteFileW(file.c_str());
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

// An engine reading an eval file not made for it (newedax.13 with the eval.dat of eval2 or of Edax 4.5) warns on its
// error output and goes on with a wrong evaluation: the app says so in the log (each time, with the engine's own line)
// and in the status line, and once in a box, after the lines are read (not while a script runs: it would stop it).
static void test_eval_mismatch()
{
	const std::string warning = "!\n!WARNING: data\\eval.dat has no weights for the extra groups of this build (EVAL_SLOTS 64): "
	                            "the evaluation is wrong; use the eval.dat of the release of this build\n";
	const std::wstring main_log = lang::f("log.eval_mismatch", {lang::t("engine.main")});
	{
		auto a = make_app();
		start_engine(*a);
		int asked = 0;
		std::wstring text;
		UINT flags = 0;
		a->message_hook_ = [&](const std::wstring &t, UINT f) {
			++asked;
			text = t;
			flags = f;
			return IDOK;
		};
		// another warning is only a line of the log
		feed(*a, "!WARNING: echo: something else\n");
		pump_app(*a);
		CHECK(asked == 0 && !logged(*a, main_log));
		feed(*a, warning);
		CHECK(asked == 0);  // (not while the lines of the engine are read)
		CHECK(logged(*a, main_log));
		CHECK(logged(*a, L"has no weights for the extra groups of this build (EVAL_SLOTS 64)"));
		CHECK(a->status_ == main_log);
		pump_app(*a);
		CHECK(asked == 1 && text == lang::f("warn.eval_mismatch", {lang::t("engine.main")}) && (flags & MB_ICONWARNING));
		// again (the engines started again): the log, but no second box
		const size_t lines = a->log_.size();
		feed(*a, warning);
		pump_app(*a);
		CHECK(asked == 1 && a->log_.size() > lines);
	}
	{  // while a script runs: the log and the status line only
		auto a = make_app();
		start_engine(*a);
		int asked = 0;
		a->message_hook_ = [&](const std::wstring &, UINT) {
			++asked;
			return IDOK;
		};
		a->script_wait_ = App::ScriptWait::Task;
		feed(*a, warning);
		pump_app(*a);
		CHECK(asked == 0 && logged(*a, main_log));
		a->script_wait_ = App::ScriptWait::None;
	}
	{  // the search engine too (each engine of the app reads the eval file its section names)
		auto a = make_app();
		start_search_engine(*a);
		a->message_hook_ = [&](const std::wstring &, UINT) { return IDOK; };
		const std::wstring search_log = lang::f("log.eval_mismatch", {lang::t("engine.search")});
		a->eng_[1].proc.send(warning);
		for (const ULONGLONG end = GetTickCount64() + 10000; !logged(*a, search_log) && GetTickCount64() < end; Sleep(1)) {
			MSG m{};
			while (PeekMessageW(&m, a->hwnd_, WM_ENGINE, WM_ENGINE, PM_REMOVE)) a->on_engine(m.wParam, m.lParam != 0);
		}
		CHECK(logged(*a, search_log));
		pump_app(*a);
	}
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
	answer_values(*a);
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
	a->cache_.get(a->game_.now()).values_done = true;  // (the stages are asked)
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

// An engine that cannot be sent a command any more is named in the log by its role (the tool, play and compare engines
// too, not as the search engine).
static void test_engine_lost_names()
{
	const char *const names[] = {"engine.main", "engine.search", "engine.tool", "engine.play", "engine.compare"};
	for (int role = 0; role < 5; ++role) {
		auto a = make_app();
		a->eng_[role].alive = true;  // (no process: the command cannot be written)
		a->send(role, "mode 3\n");
		CHECK(!a->eng_[role].alive);
		CHECK(logged(*a, lang::f("log.engine_lost", {lang::t(names[role])})));
	}
}

// An engine that has ended before a command could be written to it (here the tool engine, which ends at once as an
// engine without its eval file) is still taken as ended when the end of its output comes: the tool is not left busy.
static void test_engine_lost_then_ended()
{
	auto a = make_app();
	a->kind_ = EngineKind::Nikque18;  // (the default is newedax)
	a->ini_.set("nikque", "dir", narrow(a->temp_dir_));
	a->ini_.set("nikque", "exe", narrow(self));
	a->ini_.set("nikque", "eval", narrow(self));
	CHECK(a->start_tool("echo x", L"x") && a->tool_busy_);
	// (a command is written until it cannot be: the engine has ended; its end is not taken before the loop below)
	for (const ULONGLONG end = GetTickCount64() + 10000; a->eng_[2].alive && GetTickCount64() < end; Sleep(50))
		a->send(2, "echo y\n");
	CHECK(!a->eng_[2].alive && logged(*a, lang::f("log.engine_lost", {lang::t("engine.tool")})));
	for (const ULONGLONG end = GetTickCount64() + 5000; a->tool_busy_ && GetTickCount64() < end; Sleep(1)) {
		MSG m{};
		while (PeekMessageW(&m, a->hwnd_, WM_ENGINE, WM_ENGINE, PM_REMOVE)) a->on_engine(m.wParam, m.lParam != 0);
	}
	CHECK(!a->tool_busy_ && logged(*a, lang::t("tool.stopped").substr(0, 4)));
}

// The tool line of a script whose engine cannot start (here none is there) says so, not that a tool is running.
static void test_script_tool_not_started()
{
	auto a = make_app();
	std::wstring err;
	CHECK(!a->script_command("tool bench", 1, &err) && !a->tool_busy_);
	CHECK(err == lang::t("tool.not_started"));
	CHECK(logged(*a, lang::f("log.no_exe", {a->engine_dir()})) || logged(*a, lang::f("log.no_exe", {a->engine_exe()})));
}

// The script lines that start a job on the main engine say why it cannot start: book reduce while the count of another
// is not over (asked before, as the menu does), list book while it learns or without an engine.
static void test_script_busy_reasons()
{
	auto a = make_app();
	start_engine(*a);
	CHECK(a->reduce_book(2, 4, true, false));  // (its count in progress)
	std::wstring err;
	CHECK(!a->script_command("book reduce 1 3", 1, &err) && err == lang::t("reduce.busy"));
	a->learning_ = true;
	CHECK(!a->script_command("list book 0 60 100", 1, &err) && err == lang::t("learn.busy"));
	a->learning_ = false;
	a->eng_[0].alive = false;
	CHECK(!a->script_command("list book 0 60 100", 1, &err) && err == lang::t("learn.no_engine"));
}

// An engine is started only when its exe and its eval file are there (files, not folders), and the log says which one
// is missing: the main and search engines, the tool, play and compare engines alike.
static void test_engine_files()
{
	const std::wstring tmp = make_app()->temp_dir_;
	const std::wstring folder = tmp + L"ng-engine-folder", no_eval = tmp + L"ng-no-such-eval.dat";
	CreateDirectoryW(folder.c_str(), nullptr);
	DeleteFileW(no_eval.c_str());
	struct Case {
		std::wstring exe, eval, said;
	};
	const Case cases[] = {
		{folder, self, lang::f("log.no_exe", {folder})},      // a folder named as the exe
		{self, no_eval, lang::f("log.no_eval", {no_eval})},  // the eval file not there (this program as the exe)
		{self, folder, lang::f("log.no_eval", {folder})},    // a folder named as the eval file
	};
	for (const Case &c : cases) {
		auto a = make_app();
		for (const char *sec : {"nikque", "newedax"}) {
			a->ini_.set(sec, "dir", narrow(tmp));
			a->ini_.set(sec, "exe", narrow(c.exe));
			a->ini_.set(sec, "eval", narrow(c.eval));
		}
		a->start_engines();
		CHECK(!a->eng_[0].alive && !a->eng_[1].alive && logged(*a, c.said));
		a->log_.clear();
		CHECK(!a->start_tool("echo x", L"x") && !a->tool_busy_ && logged(*a, c.said));
		a->log_.clear();
		CHECK(!a->start_play_engine() && logged(*a, c.said));
		a->log_.clear();
		a->set_compare(true);
		CHECK(!a->compare_ && !a->eng_[4].alive && logged(*a, c.said));
	}
	RemoveDirectoryW(folder.c_str());
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
	answer_values(*a);
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
	answer_values(*a);
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

// With the precision stages the engine writes the rows of the book moves at the end of its answer, which may take
// minutes: the values are asked first, by themselves, and replace the values of the search at once; those of the other
// positions of the game come before their stages too. Without the stages the answer starts with them.
static void test_book_values_first()
{
	auto a = make_app();
	start_engine(*a);
	a->show_eval_ = a->use_book_ = true;
	a->count_ = false;
	for (const char *mv : {"f5", "d6", "c3"}) a->game_.play(parse_square(mv));
	a->game_.go_to(0);
	MoveEval *m = a->cache_.get(a->game_.now()).find(parse_square("f5"));
	m->searched = true;
	m->score = 9;
	m->depth = 12;
	a->request_analysis();
	const int values = a->values_id_;
	CHECK(values != 0 && a->book_id_ == 0);  // (the analyses are done one after the other)
	CHECK(a->requests_.count(values) && a->requests_[values].kind == Request::Values);
	CHECK(a->cell_view(parse_square("f5")).value == 9);
	const std::string id = std::to_string(values);
	feed(*a, "!@begin id=" + id + " ply=0 turn=black moves=4 book=1\n!@move id=" + id + " sq=f5 book=1 value=3 level=4 lines=10\n!@end id=" + id + " ms=0\n");
	CHECK(a->values_id_ != values);
	CellView v = a->cell_view(parse_square("f5"));
	CHECK(v.has_value && v.value == 3);  // the book, while the stages are not known
	CHECK(a->cache_.get(a->game_.now()).values_done && !a->cache_.get(a->game_.now()).book_done);
	// the other positions: their values, and only then the stages
	CHECK(a->values_id_ != 0 && a->book_id_ == 0 && a->graph_id_ == 0);
	while (const int next = a->values_id_) {
		CHECK(a->book_id_ == 0 && a->requests_[next].kind == Request::Values && a->requests_[next].pos != key_of(a->game_.now()));
		feed(*a, "!@end id=" + std::to_string(next) + " ms=0\n");
	}
	const int book = a->book_id_;
	CHECK(book != 0 && a->requests_[book].kind == Request::Book && a->requests_[book].pos == key_of(a->game_.now()));
	const std::string bid = std::to_string(book);
	feed(*a, "!@move id=" + bid + " sq=f5 book=1 value=3 level=4 lines=10 stage=2 missing=0\n!@end id=" + bid + " ms=0\n");
	v = a->cell_view(parse_square("f5"));
	CHECK(v.value == 3 && v.stage == 2);
	CHECK(a->graph_id_ != 0 && a->requests_[a->graph_id_].kind == Request::Graph);
	// leaving the position stops its values
	a->cache_.clear_book();
	a->request_analysis();
	const int left = a->values_id_;
	CHECK(left != 0);
	a->game_.go_to(1);
	a->request_analysis();
	CHECK(a->stopped_.count(left) && a->values_id_ != 0 && a->values_id_ != left);
	CHECK(a->requests_[a->values_id_].pos == key_of(a->game_.now()));
	// a point of the graph for a position not known yet holds its values back: it is not taken for the position
	const int v3 = a->values_id_;
	feed(*a, "!@cancel id=" + std::to_string(left) + "\n");
	a->game_.go_to(2);
	a->graph_id_ = 77;
	a->requests_[77] = {Request::Graph, key_of(a->game_.now()), a->book_params()};
	a->request_analysis();
	CHECK(a->stopped_.count(77) && a->stopped_.count(v3) && a->graph_id_ == 0 && a->values_id_ != 0);
	// another position's list of missing leaves is stopped too
	a->stopped_.clear();
	a->requests_[78] = {Request::Missing, key_of(a->game_.at(0))};
	a->missing_id_ = 78;
	a->missing_.pos = key_of(a->game_.at(0));
	a->game_.go_to(3);
	a->request_analysis();
	CHECK(a->stopped_.count(78) && a->missing_id_ == 0);
}

// The book analyses as the main engine reads them: the position when it is another one, then each command with its
// number. The values of the positions of the game come first, then the rest of the position shown, then a point of the
// graph; the count of book reduce is from the initial position.
static void test_main_commands()
{
	auto a = make_app();
	start_engine(*a, L"--engine read");
	a->show_eval_ = a->use_book_ = a->count_ = true;
	a->tab_ = Tab::Book;
	a->selected_sq_ = parse_square("f5");  // (its missing leaves are asked)
	a->game_.play(parse_square("f5"));
	a->game_.go_to(0);
	const std::string s0 = "setboard " + a->game_.at(0).to_board_string() + "\n";
	const std::string s1 = "setboard " + a->game_.at(1).to_board_string() + "\n";
	const std::string params = a->book_params();
	auto id = [](int n) { return " id=" + std::to_string(n); };
	a->request_analysis();
	const int v0 = a->values_id_;
	feed(*a, "!@end" + id(v0) + " ms=0\n");
	const int v1 = a->values_id_;
	feed(*a, "!@end" + id(v1) + " ms=0\n");
	const int book = a->book_id_, count = a->count_id_, levels = a->levels_id_, best = a->best_id_, missing = a->missing_id_;
	feed(*a, "!@end" + id(book) + " ms=0\n");
	const int graph = a->graph_id_;
	std::wstring err;
	CHECK(a->script_command("book reduce 2 4", 1, &err));
	const int reduce = a->reduce_.id;
	feed(*a, "");  // (the engine has read them all)
	CHECK(v0 && v1 && book && count && levels && best && missing && graph && reduce);
	const std::string expected = s0 + "book moveinfo" + id(v0) + " parts=\n" + s1 + "book moveinfo" + id(v1) + " parts=\n" + s0 +
	                             "book moveinfo" + id(book) + " " + params + "\nbook moveinfo" + id(count) + " parts=count\n" +
	                             "book levels" + id(levels) + "\nbook bestlines" + id(best) + "\nbook missing" + id(missing) +
	                             " sq=f5 base=" + a->base_ + " stages=" + a->stages_ + " x=" + std::to_string(a->loss_x_) +
	                             " y=" + std::to_string(a->loss_y_) + " n=500\n" + s1 + "book moveinfo" + id(graph) + " " +
	                             params + "\n" + s0 + "book reducecount" + id(reduce) + " x=2 y=4\n";
	const std::string got = engine_read();
	CHECK(got == expected);
	if (got != expected) std::printf("read:\n%sexpected:\n%s", got.c_str(), expected.c_str());
}

// The engine reads no command before its book is read: the book analyses wait, and only the position shown then is
// asked (one that was left is not, and the commands do not fill the pipe of the engine).
static void test_requests_wait_for_book()
{
	auto a = make_app();
	start_engine(*a);
	a->eng_[0].loaded = false;
	a->show_eval_ = a->use_book_ = true;
	a->count_ = false;
	for (const char *mv : {"f5", "d6", "c3"}) {
		a->game_.play(parse_square(mv));
		a->request_analysis();
		a->pump_graph();
		CHECK(a->requests_.empty() && a->book_id_ == 0 && a->graph_id_ == 0);
	}
	a->book_bytes_ = 84;
	feed(*a, "");  // (the answer to the "stop id=" sent first: the book is read)
	CHECK(a->eng_[0].loaded);
	answer_values(*a);
	CHECK(a->book_id_ != 0 && a->requests_.count(a->book_id_) && a->requests_[a->book_id_].pos == key_of(a->game_.now()));
	CHECK(logged(*a, L"book を読み込みました") && logged(*a, L"（84 B）"));  // it is said, with the size of the book
	auto none = make_app();
	start_engine(*none);
	none->eng_[0].loaded = false;
	feed(*none, "");
	CHECK(logged(*none, L"book のファイルがありません"));
}

// a book file of the positions with the values given, written as a book of the engine writes it
struct FilePosition {
	Position pos;
	int value = 0;
	int level = 0;
	int leaf = kNoMove;      // on the position given
	int leaf_value = 0;
};
constexpr unsigned kFileLines = 300007;  // (three bytes in the form 2)
static void write_book_file(const std::wstring &path, const std::vector<FilePosition> &list, bool shuffled = false, int form = 1)
{
	std::vector<std::pair<u32, std::string>> rows;
	for (const FilePosition &f : list) {
		const BookBoard b = book_board(f.pos);
		const char leaf = static_cast<char>(f.leaf == kNoMove ? 65 : symmetric_square(f.leaf, b.symmetry));
		std::string r(form == 2 ? 23 : 40, '\0');
		std::memcpy(r.data(), &b.player, 8);
		std::memcpy(r.data() + 8, &b.opponent, 8);
		if (form == 2) {
			r[17] = static_cast<char>(f.level);
			r[18] = r[19] = r[20] = static_cast<char>(f.value);
			r[21] = static_cast<char>(f.leaf_value);
			r[22] = leaf;
			r.append(3, '\0');  // wins, draws, losses, then the lines
			for (unsigned v = kFileLines; v; v >>= 7) r.push_back(static_cast<char>((v & 0x7f) | (v >= 0x80 ? 0x80 : 0)));
		} else {
			std::memcpy(r.data() + 28, &kFileLines, 4);
			const short v = static_cast<short>(f.value);
			std::memcpy(r.data() + 32, &v, 2);
			std::memcpy(r.data() + 34, &v, 2);
			std::memcpy(r.data() + 36, &v, 2);
			r[39] = static_cast<char>(f.level);
			r.push_back(static_cast<char>(f.leaf_value));  // the leaf: its value, its move
			r.push_back(leaf);
		}
		rows.emplace_back(static_cast<u32>(book_hash(b.player, b.opponent) & 65535), std::move(r));
	}
	if (!shuffled) std::stable_sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	std::string out = form == 2 ? "NEDXBOOK" : "XADEKOOB";
	if (form == 2) {
		out.resize(64, '\0');
		out[8] = 2;
		out[10] = 64;
		out[16] = 4;
		out[17] = 5;
		out[18] = 1;
		const unsigned long long n = rows.size();
		std::memcpy(out.data() + 48, &n, 8);
	} else {
		out.resize(42, '\0');
		out[8] = 4;
		out[9] = 5;
		const unsigned n = static_cast<unsigned>(rows.size());
		std::memcpy(out.data() + 38, &n, 4);
	}
	for (const auto &[bucket, r] : rows) out += r;
	if (form == 2) {
		out += "BEND";
		out.resize(out.size() + 12, '\xff');
	}
	FILE *f = nullptr;
	CHECK(_wfopen_s(&f, path.c_str(), L"wb") == 0 && f);
	if (f) {
		fwrite(out.data(), 1, out.size(), f);
		fclose(f);
	}
}

// Positions are found in a book file of any size, in any of their symmetries; a position not in it is not found; a
// file not in the order of an engine, or of another form, is not used.
static void test_book_file()
{
	const std::wstring path = make_app()->temp_dir_ + L"ng-book-file-test.dat";
	std::vector<FilePosition> list;
	std::vector<Position> outside;
	std::unordered_set<u64> seen;
	unsigned seed = 12345;
	auto random = [&] { return seed = seed * 1103515245 + 12345, (seed >> 16) & 0x7fff; };
	for (int game = 0; list.size() < 30000 && game < 100000; ++game) {
		Position p;
		for (int ply = 0; ply < 30 && !p.game_over(); ++ply) {
			if (p.must_pass()) p = p.play(kPass);
			u64 m = p.moves();
			for (unsigned k = random() % std::popcount(m); k; --k) m &= m - 1;
			p = p.play(std::countr_zero(m));
			const BookBoard b = book_board(p);
			if (!seen.insert(b.player ^ std::rotl(b.opponent, 17)).second) continue;
			if (random() % 8 == 0) outside.push_back(p);
			else list.push_back({p, static_cast<int>(random() % 129) - 64, static_cast<int>(random() % 61)});
		}
	}
	BookFile file;
	for (const int form : {1, 2}) {
		write_book_file(path, list, false, form);
		CHECK(file.open(path));
		int wrong = 0;
		for (size_t k = 0; k < list.size(); k += 97) {
			const auto r = file.find(symmetric(list[k].pos, static_cast<int>(k % 8)));
			if (!r || r->value != list[k].value || r->level != list[k].level || r->lines != kFileLines) ++wrong;
		}
		CHECK(wrong == 0);
		wrong = 0;
		for (size_t k = 0; k < outside.size(); k += 13)
			if (file.find(outside[k])) ++wrong;
		CHECK(wrong == 0 && file.usable());
		file.close();
		write_book_file(path, list, true, form);
		CHECK(!file.open(path));
	}
	std::string other = "NEDXBOOK";
	other.resize(1000, '\0');
	FILE *f = nullptr;
	if (_wfopen_s(&f, path.c_str(), L"wb") == 0 && f) {
		fwrite(other.data(), 1, other.size(), f);
		fclose(f);
	}
	CHECK(!file.open(path));
	DeleteFileW(path.c_str());
}

// While the main engine reads its book, the values come from the book file (the same as its lines: the value of the
// position after the move, with its sign changed, or the leaf); then they are the engine's, and a position it does
// not know any more is not kept from the file.
static void test_values_while_reading()
{
	auto a = make_app();
	const std::wstring path = a->temp_dir_ + L"ng-book-peek-test.dat";
	const Position start = Position{}.play(parse_square("f5"));  // (no two of its moves are the same by a symmetry)
	const int d6 = parse_square("d6"), f4 = parse_square("f4"), f6 = parse_square("f6");
	write_book_file(path, {{start, 0, 18, f6, 2}, {start.play(d6), -3, 21}});
	start_engine(*a);
	a->eng_[0].loaded = false;
	a->show_eval_ = a->use_book_ = true;
	a->count_ = false;
	a->game_.play(parse_square("f5"));
	// the search engine has given its own value
	MoveEval *m = a->cache_.get(start).find(d6);
	m->searched = true;
	m->score = 9;
	m->depth = 18;
	a->book_peek_.start(path, a->hwnd_, WM_BOOK_PEEK, ++a->book_peek_generation_);
	a->request_analysis();
	const ULONGLONG end = GetTickCount64() + 10000;
	while (GetTickCount64() < end && !a->cache_.get(start).book_peeked) {
		pump_app(*a);
		Sleep(1);
	}
	CHECK(a->cache_.get(start).book_peeked && a->cache_.get(start).in_book);
	CellView v = a->cell_view(d6);
	CHECK(v.has_value && v.value == 3 && v.level == 0);  // not the search's 9
	v = a->cell_view(f6);
	CHECK(v.has_value && v.value == 2);  // the leaf
	CHECK(!a->cell_view(f4).has_value);
	CHECK(!a->cache_.get(start).book_done);
	feed(*a, "");  // the engine has read its book
	answer_values(*a);
	const int id = a->book_id_;
	CHECK(id != 0 && !a->book_peek_.started());
	feed(*a, "!@begin id=" + std::to_string(id) + " ply=1 turn=white moves=3 book=1\n!@move id=" + std::to_string(id) +
	             " sq=d6 book=1 value=4 level=21 lines=9\n!@move id=" + std::to_string(id) + " sq=f6 book=0\n!@end id=" +
	             std::to_string(id) + " ms=0\n");
	CHECK(a->cell_view(d6).value == 4);
	CHECK(!a->cell_view(f6).has_value);
	DeleteFileW(path.c_str());
}

// What needs the values of the book does not wait for the rest of the analyses, and the values come while the engine
// cannot answer: a long job goes first (it is not stopped by moving the position) or the book is learning.
static void test_values_not_held_back()
{
	const int f5 = parse_square("f5"), d6 = parse_square("d6");
	const Position after = Position{}.play(f5);
	auto ready = [&] {
		auto a = make_app();
		start_engine(*a);
		a->show_eval_ = a->use_book_ = true;
		a->count_ = false;
		return a;
	};
	{  // the computer's opening move: the stages are not known yet
		auto a = ready();
		a->play_mode_ = PlayMode::HumanWhite;
		a->opening_ = Opening::BookBest;
		a->opening_until_ = 20;
		PosInfo &info = a->cache_.get(a->game_.now());
		info.values_done = true;
		MoveEval *m = info.find(f5);
		m->in_book = true;
		m->book_value = 1;
		a->com_think();
		CHECK(!a->com_waiting_book_ && a->com_book_move_ == f5);
		a->com_stop();
	}
	{  // an open node of the tree: its values go before the stages running for another position
		auto a = ready();
		a->tab_ = Tab::Tree;
		a->tree_sync();
		a->tree_open_.insert(std::vector<int>{f5});
		a->cache_.get(a->game_.now()).values_done = true;
		a->graph_id_ = 77;
		a->requests_[77] = {Request::Graph, key_of(a->game_.now()), a->book_params()};
		a->pump_graph();
		CHECK(a->stopped_.count(77) && a->graph_id_ == 0);
		CHECK(a->values_id_ != 0 && a->requests_[a->values_id_].kind == Request::Values && a->requests_[a->values_id_].pos == key_of(after));
	}
	{  // without the stages but with the umigame numbers: the end of the answer may be far too
		auto a = ready();
		a->precision_ = false;
		a->request_analysis();
		CHECK(a->values_id_ != 0);
	}
	{  // nothing that takes long: the answer is the values
		auto a = ready();
		a->precision_ = false;
		a->umi_ = UmiMode::Off;
		a->request_analysis();
		CHECK(a->values_id_ == 0 && a->book_id_ != 0);
	}
	{  // the list of missing leaves of another position stops, shown or not
		auto a = ready();
		a->precision_ = false;
		a->umi_ = UmiMode::Off;
		a->requests_[78] = {Request::Missing, key_of(after)};
		a->missing_id_ = 78;
		a->request_analysis();
		CHECK(a->stopped_.count(78) && a->missing_id_ == 0 && !a->missing_.asked);
	}
	auto a = ready();
	const std::wstring path = a->temp_dir_ + L"ng-book-held-test.dat";
	write_book_file(path, {{after, 0, 18}, {after.play(d6), -3, 21}});
	a->ini_.set(engine_section(a->kind_), "book", narrow(path));
	{  // a long job: nothing is asked behind it, the file is looked up, and the engine is asked after it
		a->requests_[90] = {Request::BestExport, key_of(Position{})};
		a->best_file_ = L"x";
		a->game_.play(f5);
		a->request_analysis();
		CHECK(a->book_id_ == 0 && a->values_id_ == 0 && a->count_id_ == 0);
		CHECK(peeked(*a, after));
		CHECK(a->cell_view(d6).has_value && a->cell_view(d6).value == 3);
		a->pump_graph();
		CHECK(a->values_id_ == 0 && a->graph_id_ == 0);
		feed(*a, "!@end id=90 written=1 lines=1 incomplete_ends=0\n");
		CHECK(!a->book_peek_.started());
		CHECK(a->values_id_ != 0 && a->requests_[a->values_id_].pos == key_of(after));
	}
	{  // learning: the same, from the book as it was saved
		auto b = ready();
		b->ini_.set(engine_section(b->kind_), "book", narrow(path));
		b->learning_ = true;
		b->game_.play(f5);
		b->request_analysis();
		CHECK(b->book_id_ == 0 && b->values_id_ == 0 && b->requests_.empty());
		CHECK(peeked(*b, after));
		CHECK(b->cell_view(d6).value == 3);
		b->book_peek_.stop();
	}
	DeleteFileW(path.c_str());
}

// The lookups of the book file end by themselves when the file cannot be used (here there is none): they are then not
// running. The app does not start them again for each position while the engine is busy (the file is the same).
static void test_book_peek_ended()
{
	const std::wstring missing = make_app()->temp_dir_ + L"ng-no-such-book.dat";
	DeleteFileW(missing.c_str());
	{
		BookPeek peek;
		peek.start(missing, nullptr, 0, 1);
		for (const ULONGLONG end = GetTickCount64() + 5000; peek.running() && GetTickCount64() < end;) Sleep(1);
		CHECK(!peek.running());
	}
	auto a = make_app();
	a->ini_.set(engine_section(a->kind_), "book", narrow(missing));
	start_engine(*a);
	a->show_eval_ = a->use_book_ = true;
	a->count_ = false;
	a->start_learning("book deviate 2 4");  // (the file is looked up while it learns)
	a->request_analysis();
	const unsigned generation = a->book_peek_generation_;
	Sleep(100);  // (the lookup has found no file)
	a->game_.play(parse_square("f5"));
	a->request_analysis();
	CHECK(a->book_peek_generation_ == generation);
	a->learning_ = false;
}

// The book file is not kept open while the main engine may replace it (a handle on it, even one that shares it all,
// makes the MoveFileEx of the engine fail): not while a command that saves the book runs, the position moved meanwhile,
// nor when the engine is closed (it saves its book as it ends). The other commands still have the values of the file.
static void test_book_replaced()
{
	for (const char *c : {"book save", "b save", "BOOK Save D:\\x.dat", " book\tsave", "book deviate2 2 4\nbook save", "quit", "Q", "exit", "eof"})
		CHECK(replaces_book_file(c));
	for (const char *c : {"book store", "book deviate2 8 16", "book saved", "echo book save", "book", "", "book load x.dat\nbook deviate 2 4"})
		CHECK(!replaces_book_file(c));
	const int f5 = parse_square("f5"), d6 = parse_square("d6");
	const Position after = Position{}.play(f5);
	const std::wstring path = make_app()->temp_dir_ + L"ng-book-replaced-test.dat", saved = path + L".tmp";
	write_book_file(path, {{after, 0, 18}, {after.play(d6), -3, 21}});
	// the book saved, as the engines save it: written next to the book file, then put in its place
	auto replaced = [&] {
		write_book_file(saved, {{after, 0, 18}, {after.play(d6), -5, 21}});
		return MoveFileExW(saved.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
	};
	auto ready = [&](const wchar_t *args) {
		auto x = make_app();
		x->ini_.set(engine_section(x->kind_), "book", narrow(path));
		start_engine(*x, args);
		x->show_eval_ = x->use_book_ = true;
		x->count_ = false;
		x->game_.play(f5);
		return x;
	};
	auto a = ready(L"--engine");
	auto move_meanwhile = [&] {
		a->game_.go_to(0);
		a->request_analysis();
		a->game_.go_to(1);
		a->request_analysis();
	};
	auto end_learning = [&] {
		feed(*a, "!echo: \"M" + std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
		CHECK(!a->learning_ && !a->book_peek_.started());
	};
	// a command that does not save the book: the file is looked up
	a->start_learning("book deviate2 2 4");
	move_meanwhile();
	CHECK(peeked(*a, after) && a->cell_view(d6).value == 3);
	end_learning();
	// book save (the menu, the command box): the file is not opened until the command has ended
	a->start_learning("book save");
	move_meanwhile();
	CHECK(!a->book_peek_.started());
	if (a->book_peek_.running()) peeked(*a, after);  // (open once a position is looked up)
	CHECK(replaced());
	end_learning();
	// a long job had it open: it is closed before the command goes to the engine (here the second of two)
	a->requests_[90] = {Request::BestExport, key_of(Position{})};
	a->best_file_ = L"x";
	a->request_analysis();
	CHECK(peeked(*a, after));
	a->start_learning("book deviate2 2 4\nbook save");
	move_meanwhile();
	CHECK(!a->book_peek_.started());
	CHECK(replaced());
	a->requests_.erase(90);
	end_learning();

	// the engine closed (the window closed) while the file was open: it is closed before the engine is told to quit
	auto b = ready((L"--engine replace \"" + saved + L"\" \"" + path + L"\"").c_str());
	b->requests_[90] = {Request::BestExport, key_of(Position{})};
	b->best_file_ = L"x";
	b->request_analysis();
	CHECK(peeked(*b, after));
	write_book_file(saved, {{after, 0, 18}});
	b->stop_engines();
	CHECK(GetFileAttributesW(saved.c_str()) == INVALID_FILE_ATTRIBUTES);  // (the engine has put it in the place of the book)
	DeleteFileW(saved.c_str());
	DeleteFileW(path.c_str());
}

// the messages of the window of the app, as its loop takes them (the timers too), until `done` or `ms`; false: not done
template <typename F>
static bool pump_until(App &a, F done, DWORD ms)
{
	const ULONGLONG end = GetTickCount64() + ms;
	for (;;) {
		MSG m{};
		while (IsWindow(a.hwnd_) && PeekMessageW(&m, a.hwnd_, 0, 0, PM_REMOVE))
			if (m.message == WM_TIMER || m.message >= WM_APP) a.proc(a.hwnd_, m.message, m.wParam, m.lParam);
		if (done()) return true;
		if (GetTickCount64() >= end) return false;
		Sleep(5);
	}
}

static bool file_there(const std::wstring &f) { return GetFileAttributesW(f.c_str()) != INVALID_FILE_ATTRIBUTES; }

// the text of `key` up to its first argument ("{0}")
static std::wstring key_head(const char *key)
{
	const std::wstring &t = lang::t(key);
	return t.substr(0, t.find(L'{'));
}

// The main engine saves a changed book as it ends (a large book takes minutes, there is no limit): after a command that
// may change the book, closing it does not wait on the thread of the window, which answers meanwhile (the log says so),
// and the engine is not ended. What was asked is done when it has ended. Otherwise it is ended 2 seconds after "quit",
// as before. An export of the lines of book deviate does not change the book.
static void test_close_waits_for_save()
{
	const std::wstring saved = make_app()->temp_dir_ + L"ng-close-saved.txt";
	// the engine has saved its book (a command that changes it, ended), 2.6 seconds after "quit"
	auto saving_app = [&](const std::wstring &book = {}) {
		DeleteFileW(saved.c_str());
		auto a = make_app();
		if (!book.empty())
			for (const char *sec : {"nikque", "newedax"}) {  // (the engine started again: this program, which ends at once)
				a->ini_.set(sec, "dir", narrow(a->temp_dir_));
				a->ini_.set(sec, "exe", narrow(self));
				a->ini_.set(sec, "eval", narrow(self));
				a->ini_.set(sec, "book", narrow(book));
			}
		a->search_engine_ = false;
		start_engine(*a, (L"--engine slowquit 2600 \"" + saved + L"\"").c_str());
		a->start_learning("book store");
		feed(*a, "!echo: \"M" + std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
		CHECK(!a->learning_);
		return a;
	};
	auto quick = [](ULONGLONG t0) { return GetTickCount64() - t0 < 1000; };
	std::vector<std::wstring> asked;
	int answer = IDNO;
	auto hook = [&](App &a) {
		asked.clear();
		a.message_hook_ = [&](const std::wstring &text, UINT) {
			asked.push_back(text);
			return answer;
		};
	};
	auto asked_for = [&](const char *key) {
		const std::wstring head = key_head(key);
		return std::any_of(asked.begin(), asked.end(), [&](const std::wstring &s) { return s.starts_with(head); });
	};

	// no command that changes the book, or an export: ended 2 seconds after "quit" (the file is not made)
	for (const int k : {0, 2}) {
		DeleteFileW(saved.c_str());
		auto a = make_app();
		start_engine(*a, (L"--engine slowquit 2600 \"" + saved + L"\"").c_str());
		if (k == 2) {
			a->eng_[0].exports = true;
			CHECK(a->start_deviate_export("deviate", 2, 4, "leaf", a->temp_dir_ + L"\\ng-export.txt"));
			feed(*a, "!echo: \"M" + std::to_string(a->learn_end_id_) + "\" is not on or off; ignored\n");
			CHECK(!a->learning_);
		}
		a->stop_engines();
		CHECK(!a->eng_[0].proc.running() && !file_there(saved) && !logged(*a, lang::t("log.engine_saving")));
	}

	// the window closed: the call returns at once, the window is there and answers until the book is saved, then closes
	{
		auto a = saving_app();
		hook(*a);
		const HWND w = a->hwnd_;
		const ULONGLONG t0 = GetTickCount64();
		a->proc(w, WM_CLOSE, 0, 0);
		CHECK(quick(t0) && IsWindow(w) && !file_there(saved) && logged(*a, lang::t("log.engine_saving")));
		CHECK(pump_until(*a, [&] { return !IsWindow(w); }, 10000));
		CHECK(file_there(saved) && logged(*a, key_head("log.engine_saved")) && asked.empty());
	}
	// closed again meanwhile: asked whether to cut the save; no: it goes on (the book is saved), then the window closes
	{
		auto a = saving_app();
		hook(*a);
		const HWND w = a->hwnd_;
		a->proc(w, WM_CLOSE, 0, 0);
		answer = IDNO;
		const ULONGLONG t0 = GetTickCount64();
		a->proc(w, WM_CLOSE, 0, 0);
		CHECK(quick(t0) && asked_for("ask.cut_save") && IsWindow(w));
		CHECK(pump_until(*a, [&] { return !IsWindow(w); }, 10000) && file_there(saved));
	}
	// yes: the engine is ended now, and the window closes (the book is not saved)
	{
		auto a = saving_app();
		hook(*a);
		const HWND w = a->hwnd_;
		a->proc(w, WM_CLOSE, 0, 0);
		answer = IDYES;
		a->proc(w, WM_CLOSE, 0, 0);
		CHECK(asked_for("ask.cut_save"));
		CHECK(pump_until(*a, [&] { return !IsWindow(w); }, 2000) && !a->eng_[0].proc.running());
		CHECK(!file_there(saved) && logged(*a, key_head("log.engine_save_cut")));
	}
	// closed by "quit" of a script: not asked, the window closes after the save
	{
		auto a = saving_app();
		hook(*a);
		const HWND w = a->hwnd_;
		a->script_quit_ = true;
		a->proc(w, WM_CLOSE, 0, 0);
		answer = IDYES;
		a->proc(w, WM_CLOSE, 0, 0);
		CHECK(asked.empty());
		CHECK(pump_until(*a, [&] { return !IsWindow(w); }, 10000) && file_there(saved));
	}
	// the last lines of the engine (its save said, or why not) reach the log, also when its end is found before the
	// window has taken them (here the timer before the message of the lines)
	{
		DeleteFileW(saved.c_str());
		auto a = make_app();
		start_engine(*a, (L"--engine slowquit 0 \"" + saved + L"\"").c_str());
		a->eng_[0].book_changed = true;  // (a command that changes the book was sent)
		a->stop_engines();
		for (const ULONGLONG end = GetTickCount64() + 5000; !a->eng_[0].proc.exited() && GetTickCount64() < end;) Sleep(1);
		a->proc(a->hwnd_, WM_TIMER, kTimerSave, 0);
		CHECK(!a->eng_[0].proc.running() && file_there(saved) && logged(*a, L"Saving book to"));
		CHECK(logged(*a, key_head("log.engine_saved")));
	}
	// what was started while the book was saved (the window answers: here the search engine and a job of the list) is
	// stopped before the window goes
	{
		auto a = saving_app();
		hook(*a);
		const HWND w = a->hwnd_;
		a->proc(w, WM_CLOSE, 0, 0);
		start_search_engine(*a);
		a->run_task(L"ng-wait", [](std::stop_token st) {
			while (!st.stop_requested()) Sleep(5);
			return std::function<void()>{};
		});
		CHECK(a->eng_[1].proc.running() && a->task_.joinable());
		CHECK(pump_until(*a, [&] { return !IsWindow(w); }, 10000) && file_there(saved));
		CHECK(!a->eng_[1].proc.running() && !a->task_.joinable());
	}
	// the book read again: the main engine starts again after the save (once); meanwhile what would close or start the
	// engines, or use the main engine, is refused (said), and a script waits
	{
		const std::wstring book = make_app()->temp_dir_ + L"ng-close-book.dat";
		auto a = saving_app(book);
		hook(*a);
		const ULONGLONG t0 = GetTickCount64();
		a->restart_main_engine();
		CHECK(quick(t0) && !file_there(saved) && a->eng_[0].generation == 1);
		a->on_command(cmd::RestartEngines);
		a->on_command(cmd::ReloadBook);
		a->on_command(cmd::BookShowInfo);
		a->switch_book(book + L"2");
		a->restart_engines();
		a->restart_main_engine();
		CHECK(asked.size() == 6 && std::all_of(asked.begin(), asked.end(), [](const std::wstring &s) { return s == lang::t("save.busy"); }));
		CHECK(a->ini_.get("nikque", "book") == narrow(book));
		a->script_.push_back({1, "log ng-after-save"});
		a->pump_script();
		CHECK(!logged(*a, L"ng-after-save"));
		CHECK(pump_until(*a, [&] { return a->eng_[0].generation != 1; }, 10000));
		CHECK(file_there(saved) && a->eng_[0].generation == 2 && logged(*a, key_head("log.engine_saved")));
		CHECK(pump_until(*a, [&] { return logged(*a, L"ng-after-save"); }, 2000));
		a->stop_engines();
	}
	// the engines started meanwhile (the search engine after it hung): the book file is not opened, which the engine may
	// be replacing (here a book form 2 of newedax, which nikque.18 cannot read, would be said)
	{
		const std::wstring book = make_app()->temp_dir_ + L"ng-close-form2.dat";
		if (FILE *f = _wfsopen(book.c_str(), L"wb", _SH_DENYNO)) {
			std::fputs("NEDXBOOK", f);
			std::fclose(f);
		}
		auto a = saving_app(book);
		hook(*a);
		a->stop_engines();
		a->start_engines();
		CHECK(asked.empty() && a->eng_[0].generation == 1);
		CHECK(pump_until(*a, [&] { return !a->eng_[0].proc.running(); }, 10000) && file_there(saved));
		DeleteFileW(book.c_str());
	}
	// a learning in progress reads "quit" only after its end: ended after 2 seconds, without asking (as before)
	{
		DeleteFileW(saved.c_str());
		auto a = make_app();
		hook(*a);
		start_engine(*a, (L"--engine slowquit 2600 \"" + saved + L"\"").c_str());
		a->start_learning("book store");
		const HWND w = a->hwnd_;
		a->proc(w, WM_CLOSE, 0, 0);
		CHECK(!IsWindow(w) && asked.empty() && !a->eng_[0].proc.running() && !file_there(saved));
	}
	DeleteFileW(saved.c_str());
}

// The moves on the lines of the openings are marked: 2 the move makes an opening, 1 it is on the way to one; the same
// in every orientation, whether the values are shown or not, and not when the setting is off.
static void test_opening_marks()
{
	auto a = make_app();
	const std::wstring path = a->temp_dir_ + L"ng-openings-test.ini";
	FILE *f = nullptr;
	CHECK(_wfopen_s(&f, path.c_str(), L"wb") == 0 && f);
	if (f) {
		const std::string text = "// a note=F5\n虎=F5D6C3D3C4\nイエス流=F5D6C3D3C4B3\nクラシック=F5D6C3D3C4B3C5\nAubrey=F5D6C3D3C4B3D2F4\n";
		fwrite(text.data(), 1, text.size(), f);
		fclose(f);
	}
	CHECK(a->openings_.load(path) == 4);
	auto mark = [&](const char *sq) { return a->cell_view(parse_square(sq)).opening; };
	CHECK(mark("f5") == 1 && mark("d3") == 1 && mark("c4") == 1 && mark("e6") == 1);  // (the same by a symmetry)
	for (const char *mv : {"f5", "d6", "c3", "d3", "c4"}) a->game_.play(parse_square(mv));
	CHECK(mark("b3") == 2);
	a->game_.play(parse_square("b3"));
	CHECK(mark("c5") == 2 && mark("d2") == 1 && mark("d7") == 0);
	a->show_eval_ = false;
	CHECK(mark("c5") == 2);
	std::wstring err;
	CHECK(a->script_command("openingmarks off", 1, &err));
	CHECK(mark("c5") == 0 && mark("d2") == 0);
	CHECK(a->script_command("openingmarks on", 1, &err));
	// the name under the board: the position is the opening, then the name stays after it
	CHECK(a->openings_.name_at(a->game_) && *a->openings_.name_at(a->game_) == L"イエス流");
	CHECK(a->openings_.mark(a->game_.now()) == 2);
	a->game_.play(parse_square("d7"));
	CHECK(a->openings_.name_at(a->game_) && *a->openings_.name_at(a->game_) == L"イエス流");
	CHECK(a->openings_.mark(a->game_.now()) == 0);
	// the same line turned by a symmetry of the initial position
	a->game_ = Game{};
	const int s = 4;
	for (const char *mv : {"f5", "d6", "c3", "d3", "c4", "b3"}) a->game_.play(symmetric_square(parse_square(mv), s));
	CHECK(a->cell_view(symmetric_square(parse_square("c5"), s)).opening == 2);
	CHECK(a->cell_view(symmetric_square(parse_square("d2"), s)).opening == 1);
	DeleteFileW(path.c_str());
}

// The row above the graph: the text box shows the moves up to the position, what is typed stays until Enter or Esc,
// Enter loads it; it goes away with the setting and with the panel, and takes room from the table and the graph.
// The colors of the stages 3 (blue) and 4 (orange), and the light of the bands crossing the numbers: white, but the
// color of the number itself on stage 4 (white bands made its color pale)
static void test_stage_colors()
{
	auto same = [](const D2D1_COLOR_F &c, std::uint32_t hex) {
		const D2D1_COLOR_F e = rgb(hex);
		return std::abs(c.r - e.r) < 1e-4f && std::abs(c.g - e.g) < 1e-4f && std::abs(c.b - e.b) < 1e-4f;
	};
	for (const Design d : {Design::Standard, Design::Paw}) {
		const Theme &th = theme(d);
		CHECK(same(th.stage[3].top, 0x8ec5ff) && same(th.stage[3].mid, 0x155dfc) && same(th.stage[3].bottom, 0x1447e6));
		CHECK(same(th.stage[3].glow, 0x51a2ff));
		CHECK(same(th.stage[4].top, 0xffb86a) && same(th.stage[4].mid, 0xff6900) && same(th.stage[4].bottom, 0xca3500));
		CHECK(same(th.stage[4].glow, 0xff8904));
		CHECK(same(stage_band(th, 4), 0xffb86a));
		for (int k : {3, 5, 6, 7}) CHECK(same(stage_band(th, k), 0xffffff));
	}
}

// The ranges of the empty squares the levels of the engine read to the end (from the most empty squares, with the
// probability of each), and the switch of their lines on the graph
static void test_end_ranges()
{
	auto is = [](int level, const std::vector<EndRange> &want) { return end_ranges(level) == want; };
	CHECK(is(0, {}));
	CHECK(is(1, {{2, 100}}));
	CHECK(is(10, {{20, 100}}));
	CHECK(is(12, {{24, 98}, {21, 100}}));
	CHECK(is(18, {{27, 87}, {24, 98}, {21, 100}}));
	CHECK(is(21, {{30, 87}, {27, 98}, {24, 100}}));
	CHECK(is(24, {{33, 73}, {30, 95}, {27, 99}, {24, 100}}));
	CHECK(is(27, {{33, 87}, {30, 98}, {27, 100}}));
	CHECK(is(29, {{36, 73}, {33, 95}, {30, 99}, {27, 100}}));
	CHECK(is(31, {{36, 87}, {33, 98}, {30, 100}}));
	CHECK(is(33, {{39, 73}, {36, 95}, {33, 99}, {30, 100}}));
	CHECK(is(35, {{39, 87}, {36, 98}, {33, 99}, {30, 100}}));
	CHECK(is(40, {{49, 73}, {46, 87}, {43, 95}, {40, 98}, {37, 99}, {34, 100}}));
	CHECK(is(52, {{60, 73}, {58, 87}, {55, 95}, {52, 98}, {49, 99}, {46, 100}}));
	CHECK(is(55, {{60, 87}, {58, 95}, {55, 98}, {52, 99}, {49, 100}}));
	CHECK(is(60, {{60, 100}}));

	auto a = make_app();
	std::wstring err;
	CHECK(a->graph_end_);
	a->on_command(cmd::GraphEndShow);
	CHECK(!a->graph_end_);
	a->save_config();
	CHECK(a->ini_.get("view", "graph_end") == "0");
	CHECK(a->script_command("graphend on", 1, &err) && a->graph_end_);
	CHECK(a->script_command("graphend off", 1, &err) && !a->graph_end_);
	CHECK(!a->script_command("graphend 2", 1, &err));
}

static void test_record_box()
{
	auto a = make_app();
	DestroyWindow(a->hwnd_);
	a->hwnd_ = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1200, 800, nullptr, nullptr, nullptr, nullptr);
	CHECK(a->gfx_.init(a->hwnd_));
	a->gfx_.resize();
	a->create_record_box();
	const HWND edit = a->record_edit_;
	CHECK(edit != nullptr);
	if (!edit) return;
	a->layout();
	const Layout &L = a->lay_;
	CHECK(L.record.bottom > L.record.top && L.record.top >= L.splitter.bottom && L.record.bottom <= L.graph.top);
	CHECK(L.record_text.right <= L.record_copy.left && L.record_copy.right <= L.record_paste.left && L.record_paste.right <= L.record.right);
	auto shown = [&] { return (GetWindowLongPtrW(edit, GWL_STYLE) & WS_VISIBLE) != 0; };
	auto text = [&] {
		a->sync_record_box();
		wchar_t b[256];
		GetWindowTextW(edit, b, 256);
		return std::wstring(b);
	};
	auto type = [&](const wchar_t *s) {
		SetWindowTextW(edit, s);
		SendMessageW(edit, EM_SETMODIFY, TRUE, 0);
	};
	CHECK(shown());
	CHECK(text().empty());
	for (const char *mv : {"f5", "d6", "c3"}) a->game_.play(parse_square(mv));
	CHECK(text() == L"f5d6c3");
	a->game_.go_to(2);
	CHECK(text() == L"f5d6");
	// what is typed stays while the game moves, Enter loads it
	type(L"f5f6");
	a->game_.go_to(3);
	CHECK(text() == L"f5f6");
	SendMessageW(edit, WM_KEYDOWN, VK_RETURN, 0);
	CHECK(a->game_.size() == 2 && a->game_.moves()[1] == parse_square("f6"));
	CHECK(text() == L"f5f6");
	// not moves: nothing happens and the text stays to be corrected
	type(L"xyz");
	SendMessageW(edit, WM_KEYDOWN, VK_RETURN, 0);
	CHECK(a->game_.size() == 2 && text() == L"xyz");
	// Esc: the moves of the game again
	SendMessageW(edit, WM_KEYDOWN, VK_ESCAPE, 0);
	CHECK(text() == L"f5f6");
	// leaving the box for the window (a click on the board): the same
	type(L"f5");
	SendMessageW(edit, WM_KILLFOCUS, reinterpret_cast<WPARAM>(a->hwnd_), 0);
	CHECK(text() == L"f5f6");
	CHECK(!a->record_resumes());
	// to another window (another program by Alt+Tab, a message box): what was typed stays, and the keys come back to
	// the box when the window has them again (once)
	type(L"f5");
	SendMessageW(edit, WM_KILLFOCUS, 0, 0);
	CHECK(text() == L"f5" && SendMessageW(edit, EM_GETMODIFY, 0, 0));
	CHECK(a->record_resumes() && !a->record_resumes());
	const HWND other = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, nullptr, nullptr);
	SendMessageW(edit, WM_KILLFOCUS, reinterpret_cast<WPARAM>(other), 0);
	CHECK(text() == L"f5");
	DestroyWindow(other);
	// the box takes them again: then not twice
	SendMessageW(edit, WM_SETFOCUS, 0, 0);
	CHECK(!a->record_resumes());
	SendMessageW(edit, WM_KEYDOWN, VK_ESCAPE, 0);
	CHECK(text() == L"f5f6");
	// not when the moves were read in the meantime (the keys go to the board after it), nor to a box hidden
	SendMessageW(edit, WM_KILLFOCUS, 0, 0);
	CHECK(a->record_take("f5f6"));
	CHECK(!a->record_resumes());
	SendMessageW(edit, WM_KILLFOCUS, 0, 0);
	ShowWindow(edit, SW_HIDE);
	CHECK(!a->record_resumes());
	ShowWindow(edit, SW_SHOWNA);
	// Enter with nothing typed does not cut the game at the position shown
	a->game_.go_to(1);
	CHECK(text() == L"f5");
	SendMessageW(edit, WM_KEYDOWN, VK_RETURN, 0);
	CHECK(a->game_.size() == 2 && a->game_.cursor() == 1);
	// while a position is set up, nothing comes in
	a->editing_ = true;
	CHECK(!a->record_take("f5d6c3"));
	CHECK(a->game_.size() == 2);
	a->editing_ = false;
	// the commands of a script
	std::wstring err;
	CHECK(a->script_command("recordtext f5d6c3d3", 1, &err));
	CHECK(a->game_.size() == 4 && text() == L"f5d6c3d3");
	CHECK(!a->script_command("recordtext xyz", 1, &err));
	CHECK(a->game_.size() == 4);
	// capitals: the text given out only, it reads back, and what the engines are told stays as it was
	CHECK(a->script_command("uppermoves on", 1, &err));
	CHECK(a->upper_moves_ && a->record_text() == L"F5D6C3D3" && text() == L"F5D6C3D3");
	CHECK(a->game_.to_text(true) == "f5d6c3d3");
	type(L"F5D6");
	SendMessageW(edit, WM_KEYDOWN, VK_RETURN, 0);
	CHECK(a->game_.size() == 2 && text() == L"F5D6");
	a->game_.go_to(1);
	CHECK(text() == L"F5");
	a->game_.go_to(2);
	a->on_command(cmd::UpperMoves);
	CHECK(!a->upper_moves_ && a->record_text() == L"f5d6" && text() == L"f5d6");
	CHECK(!a->script_command("uppermoves maybe", 1, &err));
	const float graph_h = L.graph.bottom - L.graph.top, table_h = L.table.bottom - L.table.top;
	CHECK(a->script_command("recordbox off", 1, &err));
	CHECK(!a->record_box_ && !shown());
	CHECK(L.record.bottom <= L.record.top);
	CHECK(L.graph.bottom - L.graph.top >= graph_h && L.table.bottom - L.table.top >= table_h);
	CHECK(a->script_command("recordbox on", 1, &err));
	CHECK(a->record_box_ && shown());
	CHECK(!a->script_command("recordbox maybe", 1, &err));
	// the panel takes the row with it
	a->on_command(cmd::TogglePanel);
	CHECK(!shown());
	a->on_command(cmd::TogglePanel);
	CHECK(shown());
	// hidden while the keys are in another window: what was typed goes (the moves of the game again), and the keys do
	// not come back to it
	type(L"f5f4");
	SendMessageW(edit, WM_KILLFOCUS, 0, 0);
	a->on_command(cmd::TogglePanel);
	CHECK(!shown() && !SendMessageW(edit, EM_GETMODIFY, 0, 0) && !a->record_resume_);
	a->on_command(cmd::TogglePanel);
	CHECK(shown() && text() == L"f5d6" && !a->record_resumes());
	type(L"f5f4");
	SendMessageW(edit, WM_KILLFOCUS, 0, 0);
	CHECK(a->script_command("recordbox off", 1, &err) && !shown());
	CHECK(a->script_command("recordbox on", 1, &err) && text() == L"f5d6");
	// the font and the brush of the box, the last ones too, go with the app (after the window, as at the end of run):
	// deleting them again fails then (GetObjectType still answers OBJ_FONT for a font deleted)
	const HDC dc = GetDC(edit);
	a->record_color(dc);
	ReleaseDC(edit, dc);
	const HGDIOBJ font = a->record_font_, brush = a->record_brush_;
	CHECK(font && brush && GetObjectType(font) == OBJ_FONT && GetObjectType(brush) == OBJ_BRUSH);
	DestroyWindow(a->hwnd_);
	a.reset();
	CHECK(!DeleteObject(font));
	CHECK(!DeleteObject(brush));
}

// The line between the table and the graph, dragged: its lower edge stays under the mouse, with the rows of the
// bookmarks and of the moves above the graph or without them.
static void test_graph_drag()
{
	auto a = make_app();
	DestroyWindow(a->hwnd_);
	a->hwnd_ = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1200, 800, nullptr, nullptr, nullptr, nullptr);
	CHECK(a->gfx_.init(a->hwnd_));
	a->gfx_.resize();
	a->create_record_box();
	for (const bool marks : {false, true})
		for (const bool rec : {false, true}) {
			a->bookmarks_shown_ = marks;
			a->record_box_ = rec;
			a->graph_ratio_ = 0.34f;
			a->layout();
			const Layout &L = a->lay_;
			CHECK((L.bookmarks.bottom > L.bookmarks.top) == marks && (L.record.bottom > L.record.top) == rec);
			const int x = static_cast<int>((L.splitter.left + L.splitter.right) / 2);
			const int y0 = static_cast<int>((L.splitter.top + L.splitter.bottom) / 2);
			a->on_mouse_down(x, y0, false, false);
			CHECK(a->drag_ == App::Drag::Graph);
			for (const int y : {y0 - 60, y0 + 40, y0 - 10}) {
				a->on_mouse_move(x, y);
				CHECK(std::abs(L.splitter.bottom - y) <= 1);
			}
			a->on_mouse_up(x, y0 - 10);
			CHECK(a->drag_ == App::Drag::None);
		}
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

// The settings a script turns on and off, each as its menu command does; a word other than on or off is refused.
static void test_script_switches()
{
	auto a = make_app();
	DestroyWindow(a->hwnd_);
	a->hwnd_ = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1200, 800, nullptr, nullptr, nullptr, nullptr);
	CHECK(a->gfx_.init(a->hwnd_));
	a->gfx_.resize();
	const struct {
		const char *name;
		bool App::*flag;
	} switches[] = {{"book", &App::use_book_},          {"eval", &App::show_eval_},          {"numbers", &App::move_numbers_},
	                {"pv", &App::pv_on_board_},         {"precision", &App::precision_},     {"bookmarks", &App::bookmarks_shown_},
	                {"openingmarks", &App::opening_marks_}, {"recordbox", &App::record_box_}, {"graphend", &App::graph_end_},
	                {"uppermoves", &App::upper_moves_}, {"ponder", &App::ponder_}};
	std::wstring err;
	for (const auto &[name, flag] : switches) {
		for (const bool on : {true, false, false, true})
			CHECK(a->script_command(std::string(name) + (on ? " on" : " off"), 1, &err) && (*a).*flag == on);
		err.clear();
		CHECK(!a->script_command(std::string(name) + " maybe", 1, &err) && (*a).*flag && err == lang::t("script.on_off"));
	}
	// another engine while the book learns (started from the menu): the line fails, the kind of the engine stays (the
	// engines are not started again), and no box stops the script
	int asked = 0;
	a->message_hook_ = [&](const std::wstring &, UINT) {
		++asked;
		return IDOK;
	};
	a->kind_ = EngineKind::Nikque18;  // (the default is newedax)
	a->learning_ = true;
	err.clear();
	CHECK(!a->script_command("engine newedax", 1, &err) && err == lang::t("learn.busy"));
	CHECK(a->kind_ == EngineKind::Nikque18 && asked == 0);
	a->learning_ = false;
}

// The small helpers of the files: a filter of the file dialogs (the words and the patterns, then all files, each part
// ending with a null), the name of a file without its folders, a file that is there (not a folder), small letters.
static void test_file_helpers()
{
	using namespace std::string_literals;
	CHECK(file_filter("filter.games", L"*.txt") == lang::t("filter.games") + L" (*.txt)\0*.txt\0"s + lang::t("filter.all") + L"\0*.*\0"s);
	CHECK(file_filter("filter.books", L"*.dat;*.dev*") ==
	      lang::t("filter.books") + L" (*.dat;*.dev*)\0*.dat;*.dev*\0"s + lang::t("filter.all") + L"\0*.*\0"s);
	CHECK(file_name(L"D:\\a b\\c.txt") == L"c.txt" && file_name(L"d/e.dat") == L"e.dat" && file_name(L"f.dat") == L"f.dat");
	wchar_t tmp[MAX_PATH];
	GetTempPathW(MAX_PATH, tmp);
	CHECK(file_exists(self) && !file_exists(self + L".none") && !file_exists(tmp));
	CHECK(lower("Book DEVIATE2 X5") == "book deviate2 x5");
	// the numbers in groups of three (the counts of the panel too): the sign is not in a group
	CHECK(lang::number(0) == L"0" && lang::number(999) == L"999" && lang::number(1000) == L"1,000" && lang::number(1234567) == L"1,234,567");
	CHECK(lang::number(-1) == L"-1" && lang::number(-100) == L"-100" && lang::number(-123456) == L"-123,456");
	CHECK(lang::number(LLONG_MIN) == L"-9,223,372,036,854,775,808" && lang::number(LLONG_MAX) == L"9,223,372,036,854,775,807");
	// a field of a number of the small windows: as the fields were set one by one
	const FormField n = number_field(L"depth", 36, 1, 60, L"note"), m = number_field(L"level", -1, -1, 60);
	CHECK(n.kind == FormField::Number && n.label == L"depth" && n.text == L"36" && n.lo == 1 && n.hi == 60 && n.note == L"note");
	CHECK(m.text == L"-1" && m.lo == -1 && m.note.empty() && m.choices.empty() && !m.control && !m.slider);
	// the other kinds of fields, the same way
	const FormField s = slider_field(L"level", 21, 0, 60, L"note"), x = text_field(L"seed", L"12", L"n");
	CHECK(s.kind == FormField::Slider && s.label == L"level" && s.text == L"21" && s.lo == 0 && s.hi == 60 && s.note == L"note");
	CHECK(x.kind == FormField::Text && x.label == L"seed" && x.text == L"12" && x.note == L"n" && x.lo == 0 && x.hi == 0);
	const FormField c = choice_field(L"from", {L"a", L"b"}, 1, L"c"), k = check_field(L"follow", true);
	CHECK(c.kind == FormField::Choice && c.label == L"from" && (c.choices == std::vector<std::wstring>{L"a", L"b"}) && c.choice == 1);
	CHECK(c.note == L"c" && c.text.empty() && !c.checked);
	CHECK(k.kind == FormField::Check && k.label == L"follow" && k.checked && k.note.empty() && k.choices.empty() && k.choice == 0);
}

// The names of files in config.ini and in scripts: the engine folder, its files and the book, the files of a script,
// and the script run at the start, the file of the opening names and the folder of the screenshots, are as they are
// with a drive or "\\" at their start, else in the folder they belong to (the last three: next to the exe).
static void test_file_places()
{
	auto a = make_app();
	const std::wstring exe = a->exe_dir_;
	a->ini_.set("nikque", "dir", "D:\\e n");
	CHECK(a->engine_dir(EngineKind::Nikque18) == L"D:\\e n");
	a->ini_.set("nikque", "dir", "\\\\srv\\e");
	CHECK(a->engine_dir(EngineKind::Nikque18) == L"\\\\srv\\e");
	a->ini_.set("nikque", "dir", "エンジン");
	const std::wstring dir = exe + L"\\" + widen("エンジン");
	CHECK(a->engine_dir(EngineKind::Nikque18) == dir);
	a->ini_.set("nikque", "exe", "w.exe");
	CHECK(a->engine_exe(EngineKind::Nikque18) == dir + L"\\w.exe");
	a->ini_.set("nikque", "exe", "C:\\w.exe");
	CHECK(a->engine_exe(EngineKind::Nikque18) == L"C:\\w.exe");
	a->ini_.set("nikque", "eval", "d\\e.dat");
	CHECK(a->engine_eval(EngineKind::Nikque18) == dir + L"\\d\\e.dat");
	a->ini_.set("nikque", "eval", "");
	CHECK(a->engine_eval(EngineKind::Nikque18) == dir + L"\\data\\eval.dat");
	a->kind_ = EngineKind::Nikque18;
	CHECK(a->book_file() == dir + L"\\data\\book.dat");
	a->ini_.set("nikque", "book", "b.dat");
	CHECK(a->book_file() == dir + L"\\b.dat");
	a->ini_.set("nikque", "book", "\\\\srv\\b.dat");
	CHECK(a->book_file() == L"\\\\srv\\b.dat");
	a->ini_.set("nikque", "book", "x:b.dat");
	CHECK(a->book_file() == L"x:b.dat");

	CHECK(a->images_dir() == exe + L"\\screenshots");
	RemoveDirectoryW((exe + L"\\screenshots").c_str());
	a->ini_.set("view", "screenshot_dir", "ng-app-test-shots");
	CHECK(a->images_dir() == exe + L"\\ng-app-test-shots");
	RemoveDirectoryW((exe + L"\\ng-app-test-shots").c_str());
	a->ini_.set("view", "screenshot_dir", narrow(exe + L"ng-app-test-shots2"));
	CHECK(a->images_dir() == exe + L"ng-app-test-shots2");
	RemoveDirectoryW((exe + L"ng-app-test-shots2").c_str());
	// (a name of a share of the network would be looked for there: one of this computer, as "\\" begins it)
	const std::wstring shots = L"\\\\?\\" + exe + L"ng-app-test-shots4";
	a->ini_.set("view", "screenshot_dir", narrow(shots));
	CHECK(a->images_dir() == shots && GetFileAttributesW(shots.c_str()) != INVALID_FILE_ATTRIBUTES);
	RemoveDirectoryW(shots.c_str());
	// [script] startup and [view] opening_file
	CHECK(a->from_exe_dir("\\\\srv\\s.txt") == L"\\\\srv\\s.txt" && a->from_exe_dir("C:\\s.txt") == L"C:\\s.txt");
	CHECK(a->from_exe_dir("s.txt") == exe + L"\\s.txt" && a->from_exe_dir("d\\s.txt") == exe + L"\\d\\s.txt");

	std::wstring err;
	CHECK(a->script_command("savelog ng-app-test-1.log", 1, &err) && file_exists(exe + L"\\ng-app-test-1.log"));
	a->script_dir_ = a->temp_dir_ + L"ng-app-test-dir";
	CreateDirectoryW(a->script_dir_.c_str(), nullptr);
	CHECK(a->script_command("savelog \"ng-app-test-2.log\"", 1, &err) && file_exists(a->script_dir_ + L"\\ng-app-test-2.log"));
	CHECK(a->script_command("savelog " + narrow(exe) + "ng-app-test-3.log", 1, &err) && file_exists(exe + L"ng-app-test-3.log"));
	for (const wchar_t *f : {L"\\ng-app-test-1.log", L"ng-app-test-3.log"}) DeleteFileW((exe + f).c_str());
	DeleteFileW((a->script_dir_ + L"\\ng-app-test-2.log").c_str());
	RemoveDirectoryW(a->script_dir_.c_str());
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
	answer_values(*a);
	const int book = a->book_id_;
	CHECK(book != 0);
	a->on_command(cmd::TaskCancel);
	CHECK(a->book_id_ == 0 && a->stopped_.count(book));
	feed(*a, "!@cancel id=" + std::to_string(book) + "\n");
	CHECK(a->book_id_ == 0 && a->graph_id_ == 0 && a->requests_.empty());
	a->game_.play(parse_square("d6"));
	a->position_changed();
	answer_values(*a);
	CHECK(a->book_id_ != 0);
	// stopped before the rest was asked: the position shown is asked all the same
	a->on_command(cmd::TaskCancel);
	a->game_.play(parse_square("c3"));
	a->background_paused_ = true;
	a->request_analysis();
	answer_values(*a);
	CHECK(a->book_id_ != 0 && a->graph_id_ == 0);
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

// A bookmark asked for stays where it was asked: when the same game goes away from another place, that place is kept
// as another one (a line kept by itself follows the place). Which ones were asked for is kept in kept.txt.
static void test_bookmark_asked()
{
	auto a = make_app();
	a->kept_.clear();
	for (const char *mv : {"f5", "d6", "c3"}) a->game_.play(parse_square(mv));
	a->game_.go_to(1);
	a->keep_now(false);
	a->game_.go_to(3);
	a->set_game(Game{}, true);
	CHECK(a->kept_.size() == 2 && a->kept_[0].cursor == 1 && a->kept_[1].cursor == 3);
	CHECK(a->kept_[0].asked && !a->kept_[1].asked);
	// kept by itself: its place follows
	for (const char *mv : {"f5", "d6", "c3"}) a->game_.play(parse_square(mv));
	a->game_.go_to(2);
	a->set_game(Game{}, true);
	CHECK(a->kept_.size() == 2 && a->kept_[1].cursor == 2);
	a->kept_.clear();
	a->load_kept();
	CHECK(a->kept_.size() == 2 && a->kept_[0].cursor == 1 && a->kept_[1].cursor == 2);
	CHECK(a->kept_[0].asked && !a->kept_[1].asked);
	// bookmarked where it was kept by itself: asked from then on
	for (const char *mv : {"f5", "d6", "c3"}) a->game_.play(parse_square(mv));
	a->game_.go_to(2);
	a->keep_now(false);
	CHECK(a->kept_.size() == 2 && a->kept_[1].asked);
	a->kept_.clear();
	a->save_kept();
}

// A right click on the picture of a bookmark selects it and opens its menu (to the position, compare, delete): it is
// not a move back.
static void test_bookmark_right_click()
{
	auto a = make_app();
	a->kept_.clear();
	for (const char *mv : {"f5", "d6", "c3"}) a->game_.play(parse_square(mv));
	a->keep_now(false);
	a->game_.go_to(1);
	a->keep_now(false);
	a->game_.go_to(3);
	CHECK(a->kept_.size() == 2);
	a->bookmarks_shown_ = true;
	a->lay_.panel = D2D1::RectF(480, 0, 1000, 700);
	a->lay_.bookmarks = D2D1::RectF(500, 300, 900, 360);
	a->kept_rects_ = {{0, D2D1::RectF(500, 300, 560, 360)}, {1, D2D1::RectF(570, 300, 630, 360)}};
	// (no screen: the menu, if it is shown, is closed by a timer of this thread)
	const UINT_PTR timer = SetTimer(nullptr, 0, 200, [](HWND, UINT, UINT_PTR, DWORD) { EndMenu(); });
	a->on_mouse_down(600, 330, true, false);
	KillTimer(nullptr, timer);
	CHECK(a->game_.cursor() == 3);
	CHECK(a->kept_sel_ == 1);
	a->on_command(cmd::KeptDelete);
	CHECK(a->kept_.size() == 1 && a->kept_[0].cursor == 3);
	a->kept_.clear();
}

// The engine of a new installation is newedax (from 0.4.1): the app before its settings are read, a config.ini without
// [engine] kind, and the config.ini of the distribution (res\default-config.ini). A kind written in config.ini stays.
static void test_engine_default()
{
	CHECK(App().kind_ == EngineKind::Newedax);
	auto a = make_app();
	const auto load = [&](const char *text) {
		FILE *f = nullptr;
		_wfopen_s(&f, a->ini_path_.c_str(), L"wb");
		CHECK(f != nullptr);
		if (f) {
			std::fputs(text, f);
			std::fclose(f);
		}
		a->kind_ = EngineKind::Nikque18;  // (not the value before)
		a->load_config();
	};
	load("[engine]\r\nsearch=1\r\n");
	CHECK(a->kind_ == EngineKind::Newedax);
	load("[engine]\r\nkind=nikque\r\n");
	CHECK(a->kind_ == EngineKind::Nikque18);
	load("[engine]\r\nkind=NikQue\r\n");
	CHECK(a->kind_ == EngineKind::Nikque18);
	load("[engine]\r\nkind=newedax\r\n");
	CHECK(a->kind_ == EngineKind::Newedax);
	load("[engine]\r\nkind=other\r\n");
	CHECK(a->kind_ == EngineKind::Newedax);
	DeleteFileW(a->ini_path_.c_str());
	// the config.ini of the distribution
	Ini dist;
	dist.load(src_dir + L"\\res\\default-config.ini");
	CHECK(dist.get("engine", "kind") == "newedax");
	lang::load(src_dir, "ja");  // (load_config read the words of the temporary folder)
}

// The C version is "nikque": the folder engines\nikque, the section [nikque], kind=nikque. A config.ini of 0.2.0 or
// before ([nikque18], kind=nikque18) is read as it is and saved with the new names; without engines\nikque, the folder
// engines\nikque18 of an older distribution is used. (Last: the words are read again from the folder of the test.)
static void test_engine_names()
{
	auto a = make_app();
	const std::wstring dir = a->temp_dir_ + L"ng-app-test-engine";
	CreateDirectoryW(dir.c_str(), nullptr);
	{
		FILE *f = nullptr;
		_wfopen_s(&f, a->ini_path_.c_str(), L"wb");
		CHECK(f != nullptr);
		if (f) {
			std::fprintf(f, "[engine]\r\nkind=nikque18\r\n[nikque18]\r\ndir=%s\r\nexe=auto\r\n", narrow(dir).c_str());
			std::fclose(f);
		}
	}
	a->load_config();
	CHECK(a->kind_ == EngineKind::Nikque18);
	CHECK(a->engine_dir(EngineKind::Nikque18) == dir);
	CHECK(a->ini_.section("nikque18").empty() && !a->ini_.section("nikque").empty());
	a->save_config();
	CHECK(a->ini_.get("engine", "kind") == "nikque");
	// dir= empty: engines\nikque next to the exe, else engines\nikque18 when only that one is there
	const std::wstring e = a->exe_dir_ + L"\\engines";
	CreateDirectoryW(e.c_str(), nullptr);
	CreateDirectoryW((e + L"\\nikque18").c_str(), nullptr);
	a->ini_.set("nikque", "dir", "");
	CHECK(a->engine_dir(EngineKind::Nikque18) == e + L"\\nikque18");
	CreateDirectoryW((e + L"\\nikque").c_str(), nullptr);
	CHECK(a->engine_dir(EngineKind::Nikque18) == e + L"\\nikque");
	RemoveDirectoryW((e + L"\\nikque").c_str());
	RemoveDirectoryW((e + L"\\nikque18").c_str());
	RemoveDirectoryW(e.c_str());
	RemoveDirectoryW(dir.c_str());
	DeleteFileW(a->ini_path_.c_str());
}

// A game against the computer starts from the start position with the time of each side and the clock of Black,
// the game before kept as a bookmark; the next game of the computer against itself starts the same way (here the
// play engine cannot start, so the games stop there).
static void test_play_new_game()
{
	auto a = make_app();
	for (const char *mv : {"f5", "d6"}) a->game_.play(parse_square(mv));
	a->list_index_ = 3;
	a->clock_minutes_ = 5;
	const size_t kept = a->kept_.size();
	a->set_play_mode(PlayMode::HumanBlack);
	CHECK(a->play_mode_ == PlayMode::HumanBlack && a->game_.size() == 0 && a->list_index_ == -1 && a->kept_.size() == kept + 1);
	CHECK(a->game_clock_ && a->clock_left_ms_[0] == 300000 && a->clock_left_ms_[1] == 300000 && a->clock_side_ == kBlack);
	CHECK(play_bar_text(*a).starts_with(lang::t("play.human_black")));
	CHECK(logged(*a, lang::f("play.started", {lang::t("play.human_black"), std::to_wstring(a->com_level_)})));

	a->game_.play(parse_square("f5"));
	a->clock_left_ms_[0] = 1000;
	a->clock_minutes_ = 2;
	a->play_mode_ = PlayMode::Engines;
	a->engine_games_left_ = 2;
	a->game_end_done_ = true;
	CHECK(play_timer(*a, kTimerNextGame));
	CHECK(a->game_.size() == 0 && a->kept_.size() == kept + 1);
	CHECK(a->game_clock_ && a->clock_left_ms_[0] == 120000 && a->clock_left_ms_[1] == 120000);
	CHECK(a->play_mode_ == PlayMode::Free && a->clock_side_ == -1 && play_bar_text(*a).empty());
	DeleteFileW((a->exe_dir_ + L"\\kept.txt").c_str());
}

// The wheel over the table moves the rows of the tab shown three at a time (not before the first), over the row of the
// bookmarks one bookmark at a time (while it is shown), over the number X or Y that number.
static void test_wheel()
{
	auto a = make_app();
	a->lay_.table = {0, 100, 400, 300};
	a->lay_.bookmarks = {0, 300, 400, 360};
	a->bookmarks_shown_ = true;
	const struct {
		Tab tab;
		int App::*scroll;
	} tabs[] = {{Tab::Moves, &App::table_scroll_}, {Tab::Book, &App::missing_scroll_}, {Tab::Record, &App::table_scroll_},
	            {Tab::List, &App::list_scroll_}, {Tab::Tree, &App::tree_scroll_}};
	for (const auto &[tab, scroll] : tabs) {
		a->tab_ = tab;
		a->table_scroll_ = a->missing_scroll_ = a->list_scroll_ = a->tree_scroll_ = a->kept_scroll_ = 0;
		a->on_wheel(10, 299, -120);
		a->on_wheel(10, 100, -120);
		CHECK((*a).*scroll == 6);
		a->on_wheel(10, 200, 120);
		CHECK((*a).*scroll == 3);
		a->on_wheel(10, 200, 120);
		a->on_wheel(10, 200, 120);
		CHECK((*a).*scroll == 0);
		a->on_wheel(10, 300, -120);
		a->on_wheel(10, 99, -120);
		CHECK(a->kept_scroll_ == 1 && a->table_scroll_ + a->missing_scroll_ + a->list_scroll_ + a->tree_scroll_ == 0);
	}
	a->bookmarks_shown_ = false;
	a->on_wheel(10, 300, -120);
	CHECK(a->kept_scroll_ == 1);
	a->lay_.fx.box = {0, 10, 40, 30};
	a->lay_.fy.track = {100, 10, 140, 30};
	a->on_wheel(10, 20, 120);
	a->on_wheel(110, 20, -120);
	CHECK(a->loss_x_ == 3 && a->loss_y_ == 3);
}

// The game on the board goes to the end of the list without its passes ("list addgame", and at the end of a game
// against the computer when it is set so); the games found by a search are not shown after "list clear" or the menu.
static void test_list_add_game()
{
	auto a = make_app();
	Position s;
	s.black = u64{1} << 0 | u64{1} << 56;  // a1 a8
	s.white = u64{1} << 1 | u64{1} << 57;  // b1 b8
	s.side = kBlack;
	a->game_.reset(s);
	CHECK(a->game_.play(parse_square("c1")) && a->game_.play(parse_square("c8")));  // (white passes between)
	CHECK(a->game_.size() == 3 && a->game_.moves()[1] == kPass && a->game_.now().game_over());
	std::wstring err;
	CHECK(a->script_command("list addgame", 1, &err) && a->list_.size() == 1 && a->list_index_ == 0);
	const auto mv = a->list_.moves(0);
	CHECK(a->list_.start(0) == s && mv.size() == 2 && mv[0] == parse_square("c1") && mv[1] == parse_square("c8"));
	a->play_mode_ = PlayMode::HumanBlack;
	a->end_keep_ = false;
	a->end_list_ = true;
	a->game_end_check();
	CHECK(a->game_end_done_ && a->list_.size() == 2 && a->list_.moves(1).size() == 2 && a->kept_.empty());
	a->play_mode_ = PlayMode::Free;
	a->hits_.push_back(ListHit{1, 2, 0});
	a->hits_shown_ = true;
	a->on_command(cmd::ListSearchClear);
	CHECK(a->hits_.empty() && !a->hits_shown_ && a->list_.size() == 2);
	a->hits_.push_back(ListHit{1, 2, 0});
	a->hits_shown_ = true;
	a->list_file_ = L"D:\\games.txt";  // (read or saved before: its name is over the list)
	CHECK(a->script_command("list clear", 1, &err) && a->list_.empty() && a->list_index_ == -1 && a->hits_.empty() && !a->hits_shown_);
	CHECK(a->list_file_.empty());  // as the menu
}

// An engine that does not read its input for a while: the lines sent wait in the pipe (up to a megabyte), and the
// window does not wait for the engine (200 KB here, the lines of some hundreds of positions).
static void test_send_not_read()
{
	wchar_t tmp[MAX_PATH];
	GetTempPathW(MAX_PATH, tmp);
	EngineProcess p;
	CHECK(p.start(self, L"--engine deaf", tmp, nullptr, 0, 0, nullptr));
	const std::string line = std::string(999, 'x') + "\n";
	std::atomic<bool> sent{false};
	std::thread t([&] {
		for (int i = 0; i < 200; ++i) p.send(line);
		sent = true;
	});
	const ULONGLONG end = GetTickCount64() + 2000;
	while (!sent && GetTickCount64() < end) Sleep(10);
	CHECK(sent);
	t.join();  // (a send that waits fails when the engine ends)
	p.close(0);
}

// config.ini that cannot be written (here a folder where it is written first): the log says the settings were not
// saved, and the file before stays.
static void test_config_not_saved()
{
	auto a = make_app();
	const std::wstring tmp = a->ini_path_ + L".tmp", failed = lang::f("config.save_failed", {a->ini_path_});
	DeleteFileW(a->ini_path_.c_str());
	a->level_ = 30;
	a->save_config();
	std::string text;
	CHECK(read_file(a->ini_path_, &text) && text.find("level=30") != std::string::npos && !logged(*a, failed));
	// every line with CRLF, the last one too
	CHECK(text.ends_with("\r\n") && std::count(text.begin(), text.end(), '\n') == static_cast<std::ptrdiff_t>(std::count(text.begin(), text.end(), '\r')));
	CHECK(!file_there(tmp));
	CreateDirectoryW(tmp.c_str(), nullptr);
	a->level_ = 31;
	a->save_config();
	CHECK(logged(*a, failed));
	CHECK(read_file(a->ini_path_, &text) && text.find("level=31") == std::string::npos && text.find("level=30") != std::string::npos);
	RemoveDirectoryW(tmp.c_str());
	a->log_.clear();
	a->save_config();  // (the settings are written the next time)
	CHECK(!logged(*a, failed) && read_file(a->ini_path_, &text) && text.find("level=31") != std::string::npos);
	DeleteFileW(a->ini_path_.c_str());
}

// A text format or a gradient that cannot be made (a size of 0, no stops) is null and not kept: drawing and measuring
// with it do nothing, and the gradient is made when it can be.
static void test_gfx_failed()
{
	auto a = make_app();
	DestroyWindow(a->hwnd_);
	a->hwnd_ = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 200, 100, nullptr, nullptr, nullptr, nullptr);
	Gfx &gx = a->gfx_;
	CHECK(gx.init(a->hwnd_));
	gx.resize();
	CHECK(gx.text(0, false) == nullptr && gx.text(-3, true, DWRITE_TEXT_ALIGNMENT_CENTER, Face::Number) == nullptr);
	CHECK(gx.text(12, false) != nullptr && gx.text(0, false) == nullptr);
	CHECK(gx.text_width(L"f5", nullptr) == 0);
	if (gx.begin_frame()) {
		gx.draw_text(L"f5", nullptr, D2D1::RectF(0, 0, 50, 20), D2D1::ColorF(0, 0, 0));
		gx.end_frame();
	}
	const D2D1_GRADIENT_STOP stops[2] = {{0, D2D1::ColorF(1, 0, 0)}, {1, D2D1::ColorF(0, 0, 1)}};
	CHECK(gx.gradient(999, stops, 0) == nullptr);
	CHECK(gx.gradient(999, stops, 2) != nullptr);
	// a shape without its outline (the font or the geometry could not be made: Gfx::shape) is drawn as nothing
	const NumberShape none{};
	if (gx.begin_frame()) {
		gx.fill_shape(none, gx.brush(D2D1::ColorF(0, 0, 0)));
		gx.stroke_shape(none, gx.brush(D2D1::ColorF(0, 0, 0)), 1.0f);
		gx.end_frame();
	}
	CHECK(!none.fill_real && !none.stroke_real);
	const NumberShape &s = gx.shape(L"-12", 20);
	CHECK(s.geom && s.bounds.right > s.bounds.left);
	DestroyWindow(a->hwnd_);
}

// The bookmarks are kept in kept.txt next to the exe, put in its place whole. When it cannot be written, the log says
// so and the file before stays; with no bookmarks it goes.
static void test_kept_file()
{
	auto a = make_app();
	const std::wstring dir = a->temp_dir_ + L"ng-app-test-kept", file = dir + L"\\kept.txt", tmp = file + L".tmp";
	CreateDirectoryW(dir.c_str(), nullptr);
	DeleteFileW(file.c_str());
	a->exe_dir_ = dir;
	const std::wstring failed = lang::f("kept.save_failed", {file});
	a->game_.play(parse_square("f5"));
	a->keep_now(false);
	std::string text;
	CHECK(read_file(file, &text) && text.find("\tf5\t1\tasked") != std::string::npos);
	CHECK(GetFileAttributesW(tmp.c_str()) == INVALID_FILE_ATTRIBUTES && !logged(*a, failed));
	// a folder where the file is written first: it cannot be put in the place of kept.txt
	CreateDirectoryW(tmp.c_str(), nullptr);
	a->game_.play(parse_square("d6"));
	a->keep_now(false);
	CHECK(a->kept_.size() == 2 && logged(*a, failed));
	CHECK(read_file(file, &text) && text.find("f5d6") == std::string::npos && text.find("\tf5\t1\tasked") != std::string::npos);
	RemoveDirectoryW(tmp.c_str());
	a->log_.clear();
	for (int k = 0; k < 2; ++k) {
		a->kept_sel_ = 0;
		a->kept_command(cmd::KeptDelete);
	}
	CHECK(a->kept_.empty() && GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES && !logged(*a, failed));
	RemoveDirectoryW(dir.c_str());
}

// A board given as text ("board" of a script) starts a game there, the game before kept as a bookmark and the board
// said in the log; a text that is no board is refused.
static void test_board_text()
{
	auto a = make_app();
	a->game_.play(parse_square("f5"));
	Position s;
	s.black = u64{1} << 0 | u64{1} << 56;
	s.white = u64{1} << 1 | u64{1} << 57;
	s.side = kBlack;
	std::wstring err;
	CHECK(a->script_command("board " + s.to_board_string(), 1, &err));
	CHECK(a->game_.start() == s && a->game_.size() == 0 && a->kept_.size() == 1 && logged(*a, lang::t("status.pasted_board")));
	err.clear();
	CHECK(!a->script_command("board xyz", 1, &err) && err == lang::t("script.bad_board") && a->game_.start() == s);
	DeleteFileW((a->exe_dir_ + L"\\kept.txt").c_str());
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

// The settings of config.ini given by their names, in any case: each name is read and written again as it is, another
// name gives the default. The language is its code as it is. The script commands of these settings take the same names.
static void test_config_names()
{
	auto a = make_app();
	const struct {
		const char *section, *key;
		std::vector<const char *> names;
		int def;
		int (*value)(const App &);
	} settings[] = {
		{"view", "design", {"standard", "paw"}, 0, [](const App &x) { return static_cast<int>(x.design_); }},
		{"view", "effects", {"on", "subdued", "off"}, 0, [](const App &x) { return static_cast<int>(x.effects_); }},
		{"view", "top_effect", {"strong", "normal", "weak"}, 1, [](const App &x) { return static_cast<int>(x.top_effect_); }},
		{"view", "cell_info", {"none", "positions", "level", "stage", "umi"}, 0, [](const App &x) { return static_cast<int>(x.cell_info_); }},
		{"analysis", "umi", {"best", "loss", "off", "range"}, 0, [](const App &x) { return static_cast<int>(x.umi_); }},
		{"window", "tab", {"moves", "book", "record", "list", "tree"}, 0, [](const App &x) { return static_cast<int>(x.tab_); }},
		{"play", "opening", {"search", "best", "wide"}, 1, [](const App &x) { return static_cast<int>(x.opening_); }},
	};
	auto load = [&](const std::string &text) {
		FILE *f = nullptr;
		_wfopen_s(&f, a->ini_path_.c_str(), L"wb");
		CHECK(f != nullptr);
		if (f) {
			std::fputs(text.c_str(), f);
			std::fclose(f);
		}
		a->load_config();
	};
	for (const auto &s : settings) {
		const std::string head = std::string("[") + s.section + "]\r\n" + s.key + "=";
		for (int k = 0; k < static_cast<int>(s.names.size()); ++k) {
			std::string upper = s.names[k];
			for (char &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			for (const std::string &name : {std::string(s.names[k]), upper}) {
				load(head + name + "\r\n");
				CHECK(s.value(*a) == k);
				a->save_config();
				CHECK(a->ini_.get(s.section, s.key) == s.names[k]);
			}
		}
		load(head + "other\r\n");
		CHECK(s.value(*a) == s.def);
		a->save_config();
		CHECK(a->ini_.get(s.section, s.key) == s.names[s.def]);
	}
	for (const char *code : {"ja", "en", "zh-Hans", "zh-Hant"}) {
		load(std::string("[view]\r\nlanguage=") + code + "\r\n");
		CHECK(a->lang_ == code);
	}
	load("[view]\r\nlanguage=EN\r\n");
	CHECK(a->lang_ == "ja");
	DeleteFileW(a->ini_path_.c_str());

	std::wstring err;
	const char *tabs[] = {"moves", "book", "record", "list", "tree"};
	for (int k : {1, 4, 2, 3, 0}) CHECK(a->script_command(std::string("tab ") + tabs[k], 1, &err) && a->tab_ == static_cast<Tab>(k));
	CHECK(a->script_command("tab TREE", 1, &err) && a->tab_ == Tab::Tree);
	const char *cells[] = {"none", "positions", "level", "stage", "umi"};
	for (int k : {3, 1, 4, 2, 0}) CHECK(a->script_command(std::string("cellinfo ") + cells[k], 1, &err) && a->cell_info_ == static_cast<CellInfo>(k));
	const char *effects[] = {"on", "subdued", "off"};
	for (int k : {2, 1, 0}) CHECK(a->script_command(std::string("effects ") + effects[k], 1, &err) && a->effects_ == static_cast<Effects>(k));
	const char *openings[] = {"search", "best", "wide"};
	for (int k : {2, 0, 1}) CHECK(a->script_command(std::string("opening ") + openings[k], 1, &err) && a->opening_ == static_cast<Opening>(k));
	CHECK(a->script_command("design paw", 1, &err) && a->design_ == Design::Paw);
	CHECK(a->script_command("design standard", 1, &err) && a->design_ == Design::Standard);
	for (const char *bad : {"tab other", "cellinfo other", "effects subdue", "opening other", "design other", "lang EN", "lang de"}) {
		err.clear();
		CHECK(!a->script_command(bad, 1, &err) && err == lang::t("script.bad_word"));
	}
	CHECK(a->script_command("lang ja", 1, &err) && a->lang_ == "ja");
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
	while (GetTickCount64() < end && !(a->book_id_ == 0 && a->values_id_ == 0 && a->count_id_ == 0 && a->levels_id_ == 0 && a->best_id_ == 0 &&
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
	if (argc == 3 && std::string(argv[1]) == "--peek") {
		// the moves of the positions given as "board side" lines on the input, as the book file says (time in ms)
		BookFile file;
		if (!file.open(widen(argv[2]))) {
			std::printf("not usable\n");
			return 1;
		}
		for (std::string s; std::getline(std::cin, s);) {
			Position p;
			if (s.size() < 66) continue;
			p.black = p.white = 0;
			for (int k = 0; k < 64; ++k) {
				if (s[k] == 'X') p.black |= u64{1} << k;
				else if (s[k] == 'O') p.white |= u64{1} << k;
			}
			p.side = s[65] == 'X' ? kBlack : kWhite;
			const ULONGLONG t0 = GetTickCount64();
			const BookPeekAnswer a = book_peek_moves(file, p);
			std::printf("position %s book=%d ms=%llu\n", s.c_str(), a.in_book ? 1 : 0, GetTickCount64() - t0);
			for (const BookPeekMove &e : a.moves)
				std::printf("move %s book=%d leaf=%d value=%d level=%d lines=%lld\n", square_name(e.sq).c_str(), e.in_book ? 1 : 0,
				            e.leaf ? 1 : 0, e.value, e.level, e.lines);
		}
		return 0;
	}
	if (argc > 1 && std::string(argv[1]) == "-q") {
		// started with the options the GUI gives a real engine: as Edax without its eval file, it says so (without ending
		// the line) and ends
		std::fputs("Cannot open the eval file", stderr);
		return 1;
	}
	if (argc > 1 && std::string(argv[1]) == "--engine") {
		// "--engine deaf": an engine that reads nothing for 5 s (busy, or stuck), then ends
		if (argc > 2 && std::string(argv[2]) == "deaf") {
			Sleep(5000);
			return 0;
		}
		FILE *read =argc > 2 && std::string(argv[2]) == "read" ? _wfsopen(engine_read_file().c_str(), L"wb", _SH_DENYNO) : nullptr;
		// "--engine replace <from> <to>": at "quit", <from> is put in the place of <to> (an engine saves its book as it ends)
		const bool replace = argc == 5 && std::string(argv[2]) == "replace";
		// "--engine slowquit <ms> <file>": at "quit", <file> is made <ms> after, then the engine writes a last line (as
		// Edax its error of the save, or the save with its option -info) and ends (a large book saved)
		const bool slow = argc == 5 && std::string(argv[2]) == "slowquit";
		for (std::string s; std::getline(std::cin, s);) {
			if (s.starts_with("!")) {
				std::cout << s.substr(1) << std::endl;
			} else if (replace && s == "quit") {
				MoveFileExA(argv[3], argv[4], MOVEFILE_REPLACE_EXISTING);
			} else if (slow && s == "quit") {
				Sleep(static_cast<DWORD>(std::atoi(argv[3])));
				FILE *f = nullptr;
				if (fopen_s(&f, argv[4], "wb") == 0 && f) std::fclose(f);
				std::cout << "Saving book to " << argv[4] << "...done" << std::endl;
				return 0;
			} else if (read) {
				std::fputs((s + "\n").c_str(), read);
				std::fflush(read);
			}
		}
		if (read) std::fclose(read);
		return 0;
	}
	lang::load(argc > 1 ? widen(argv[1]) : std::wstring(L"."), "ja");
	src_dir = argc > 1 ? widen(argv[1]) : std::wstring(L".");
	std::setvbuf(stdout, nullptr, _IONBF, 0);  // (the lines before a crash are kept)
	std::printf("%s\n", "test_engine_default");
	test_engine_default();
	std::printf("%s\n", "test_mixed_lines");
	test_mixed_lines();
	std::printf("%s\n", "test_old_settings");
	test_old_settings();
	std::printf("%s\n", "test_eval_mismatch");
	test_eval_mismatch();
	std::printf("%s\n", "test_settings_keep_counts");
	test_settings_keep_counts();
	std::printf("%s\n", "test_command_letters");
	test_command_letters();
	std::printf("%s\n", "test_stopped_numbers");
	test_stopped_numbers();
	std::printf("%s\n", "test_engine_ended");
	test_engine_ended();
	std::printf("%s\n", "test_engine_lost_names");
	test_engine_lost_names();
	std::printf("%s\n", "test_engine_lost_then_ended");
	test_engine_lost_then_ended();
	std::printf("%s\n", "test_script_tool_not_started");
	test_script_tool_not_started();
	std::printf("%s\n", "test_script_busy_reasons");
	test_script_busy_reasons();
	std::printf("%s\n", "test_engine_files");
	test_engine_files();
	std::printf("%s\n", "test_recompute");
	test_recompute();
	std::printf("%s\n", "test_reduce_script");
	test_reduce_script();
	std::printf("%s\n", "test_deviate_export");
	test_deviate_export();
	std::printf("%s\n", "test_export_then_learning");
	test_export_then_learning();
	std::printf("%s\n", "test_export_while_loading");
	test_export_while_loading();
	std::printf("%s\n", "test_best_export");
	test_best_export();
	std::printf("%s\n", "test_reduce_unknown");
	test_reduce_unknown();
	std::printf("%s\n", "test_reduce_stopped");
	test_reduce_stopped();
	std::printf("%s\n", "test_reduce_damaged");
	test_reduce_damaged();
	std::printf("%s\n", "test_reduce_results");
	test_reduce_results();
	std::printf("%s\n", "test_reduce_not_run");
	test_reduce_not_run();
	std::printf("%s\n", "test_learn_queue");
	test_learn_queue();
	std::printf("%s\n", "test_learn_options");
	test_learn_options();
	std::printf("%s\n", "test_learn_options_later");
	test_learn_options_later();
	std::printf("%s\n", "test_learn_options_emptied");
	test_learn_options_emptied();
	std::printf("%s\n", "test_missing_other_position");
	test_missing_other_position();
	std::printf("%s\n", "test_pv_on_board");
	test_pv_on_board();
	std::printf("%s\n", "test_precision_off");
	test_precision_off();
	std::printf("%s\n", "test_graph_yields");
	test_graph_yields();
	std::printf("%s\n", "test_book_values_first");
	test_book_values_first();
	std::printf("%s\n", "test_main_commands");
	test_main_commands();
	std::printf("%s\n", "test_requests_wait_for_book");
	test_requests_wait_for_book();
	std::printf("%s\n", "test_book_file");
	test_book_file();
	std::printf("%s\n", "test_values_while_reading");
	test_values_while_reading();
	std::printf("%s\n", "test_values_not_held_back");
	test_values_not_held_back();
	std::printf("%s\n", "test_book_peek_ended");
	test_book_peek_ended();
	std::printf("%s\n", "test_book_replaced");
	test_book_replaced();
	std::printf("%s\n", "test_close_waits_for_save");
	test_close_waits_for_save();
	std::printf("%s\n", "test_opening_marks");
	test_opening_marks();
	std::printf("%s\n", "test_stage_colors");
	test_stage_colors();
	std::printf("%s\n", "test_end_ranges");
	test_end_ranges();
	std::printf("%s\n", "test_record_box");
	test_record_box();
	std::printf("%s\n", "test_graph_drag");
	test_graph_drag();
	std::printf("%s\n", "test_kept_file");
	test_kept_file();
	std::printf("%s\n", "test_gfx_failed");
	test_gfx_failed();
	std::printf("%s\n", "test_config_not_saved");
	test_config_not_saved();
	std::printf("%s\n", "test_send_not_read");
	test_send_not_read();
	std::printf("%s\n", "test_levels");
	test_levels();
	std::printf("%s\n", "test_script_switches");
	test_script_switches();
	std::printf("%s\n", "test_file_helpers");
	test_file_helpers();
	std::printf("%s\n", "test_file_places");
	test_file_places();
	std::printf("%s\n", "test_forward_analysis");
	test_forward_analysis();
	std::printf("%s\n", "test_stop_background");
	test_stop_background();
	std::printf("%s\n", "test_bookmarks");
	test_bookmarks();
	std::printf("%s\n", "test_bookmark_right_click");
	test_bookmark_right_click();
	std::printf("%s\n", "test_bookmark_asked");
	test_bookmark_asked();
	std::printf("%s\n", "test_design_toggle");
	test_design_toggle();
	std::printf("%s\n", "test_play_new_game");
	test_play_new_game();
	std::printf("%s\n", "test_wheel");
	test_wheel();
	std::printf("%s\n", "test_list_add_game");
	test_list_add_game();
	std::printf("%s\n", "test_board_text");
	test_board_text();
	std::printf("%s\n", "test_engine_names");
	test_engine_names();
	std::printf("%s\n", "test_config_names");
	test_config_names();
	if (failures) {
		std::printf("%d failures\n", failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
