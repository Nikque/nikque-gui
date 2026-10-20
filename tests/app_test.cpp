// The app with the main engine: the lines the engine writes and what becomes of the requests. The engine is this
// program started again with "--engine": it writes back the lines it reads that begin with '!' (without the '!').
#include <windows.h>

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include "app.h"
#include "lang.h"

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
	test_mixed_lines();
	test_old_settings();
	test_stopped_numbers();
	test_engine_ended();
	test_recompute();
	if (failures) {
		std::printf("%d failures\n", failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
