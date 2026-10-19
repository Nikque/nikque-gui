#include "app.h"

#include <windows.h>

#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

#include "lang.h"
#include "measure.h"
#include "resource.h"

namespace ng {

namespace {

constexpr UINT_PTR kTimerAnim = 1;
constexpr UINT_PTR kTimerWatch = 2;  // the engines are watched every 5 seconds
constexpr UINT_PTR kTimerScript = 3; // a script waiting for the analysis of the position looks again

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	App *app = reinterpret_cast<App *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (msg == WM_NCCREATE) {
		app = static_cast<App *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
		app->hwnd_ = hwnd;
	}
	if (app) return app->proc(hwnd, msg, wp, lp);
	return DefWindowProcW(hwnd, msg, wp, lp);
}

std::string lower(std::string s)
{
	for (auto &c : s)
		if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
	return s;
}

// 974, 12.3K, 974K, 1.2M, 3.4G
std::wstring compact(long long n)
{
	wchar_t b[32];
	if (n < 1000) return std::to_wstring(n);
	if (n < 10'000) swprintf(b, 32, L"%.1fK", n / 1e3);
	else if (n < 1'000'000) swprintf(b, 32, L"%lldK", n / 1000);
	else if (n < 10'000'000) swprintf(b, 32, L"%.1fM", n / 1e6);
	else if (n < 1'000'000'000) swprintf(b, 32, L"%lldM", n / 1'000'000);
	else swprintf(b, 32, L"%.1fG", n / 1e9);
	return b;
}

// the small number of a move's square chosen in the view menu
std::wstring cell_info_text(const MoveEval &m, CellInfo what, bool use_book)
{
	const bool book = use_book && m.in_book;
	switch (what) {
	case CellInfo::Positions:
		if (!book) return {};
		if (m.positions >= 0) return compact(m.positions);
		return m.lines >= 0 ? L"\x2248" + compact(m.lines) : std::wstring();  // the lines of the book until counted
	case CellInfo::Level: {
		const int lv = book ? m.book_level : shown_level(m, use_book);
		return lv > 0 ? L"L" + std::to_wstring(lv) : std::wstring();
	}
	case CellInfo::Stage:
		return use_book && m.book_row && m.stage >= 0 ? L"S" + std::to_wstring(m.stage) : std::wstring();
	case CellInfo::Umi:
		if (!m.umi_known) return {};
		return m.umi_none ? L"-" : std::to_wstring(m.umi_black) + L":" + std::to_wstring(m.umi_white);
	default: return {};
	}
}

}  // namespace

int Layout::square_at(float x, float y) const
{
	const int ox = bx + frame + line, oy = by + frame + line;
	const int step = cell + line;
	if (x < ox || y < oy) return kNoMove;
	const int cx = static_cast<int>(x - ox) / step, cy = static_cast<int>(y - oy) / step;
	if (cx > 7 || cy > 7) return kNoMove;
	if (static_cast<int>(x - ox) % step >= cell || static_cast<int>(y - oy) % step >= cell) return kNoMove;
	return cy * 8 + cx;
}

int App::run(HINSTANCE inst, int show)
{
	inst_ = inst;
	measure::start();
	wchar_t path[MAX_PATH * 4];
	GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
	exe_dir_ = path;
	exe_dir_.resize(exe_dir_.find_last_of(L"\\/"));
	ini_path_ = exe_dir_ + L"\\config.ini";
	wchar_t tmp[MAX_PATH];
	GetTempPathW(MAX_PATH, tmp);
	temp_dir_ = std::wstring(tmp) + L"nikque-gui-" + std::to_wstring(GetCurrentProcessId());
	CreateDirectoryW(temp_dir_.c_str(), nullptr);

	load_config();

	WNDCLASSEXW wc{sizeof wc};
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = wndproc;
	wc.hInstance = inst;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(design_ == Design::Paw ? IDI_PAW : IDI_APP));
	wc.lpszClassName = L"NikqueGuiMain";
	RegisterClassExW(&wc);

	const int x = ini_.get_int("window", "x", CW_USEDEFAULT, -32000, 32000);
	const int y = ini_.get_int("window", "y", CW_USEDEFAULT, -32000, 32000);
	// the default size is for 96 DPI
	const int sdpi = static_cast<int>(GetDpiForSystem());
	const int w = ini_.get_int("window", "w", MulDiv(1280, sdpi, 96), 400, 16000);
	const int h = ini_.get_int("window", "h", MulDiv(820, sdpi, 96), 300, 16000);
	CreateWindowExW(0, wc.lpszClassName, lang::t("app.title").c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
	                CW_USEDEFAULT, w, h, nullptr, nullptr, inst, this);
	if (!hwnd_) return 1;
	if (x != CW_USEDEFAULT && y != CW_USEDEFAULT) {
		RECT r{x, y, x + w, y + h};
		if (MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) SetWindowPos(hwnd_, nullptr, x, y, w, h, SWP_NOZORDER);
	}
	if (!gfx_.init(hwnd_)) {
		MessageBoxW(hwnd_, lang::t("err.d2d").c_str(), lang::t("app.title").c_str(), MB_ICONERROR);
		return 1;
	}
	DragAcceptFiles(hwnd_, TRUE);
	build_menu();
	set_window_icon();
	gfx_.resize();
	layout();
	ShowWindow(hwnd_, ini_.get_bool("window", "max", false) ? SW_SHOWMAXIMIZED : show);
	UpdateWindow(hwnd_);
	if (!openings_.empty()) log(lang::f("log.openings", {std::to_wstring(openings_.positions() / 8)}));
	load_kept();
	start_engines();
	position_changed();
	SetTimer(hwnd_, kTimerWatch, 5000, nullptr);
	// the commands to run at the start (config.ini [script] startup)
	if (const std::string st = ini_.get("script", "startup"); !st.empty())
		run_script_file(st.size() > 1 && st[1] == ':' ? widen(st) : exe_dir_ + L"\\" + widen(st), 0);

	MSG m;
	while (GetMessageW(&m, nullptr, 0, 0) > 0) {
		TranslateMessage(&m);
		DispatchMessageW(&m);
	}
	measure::finish(exe_dir_);
	return 0;
}

LRESULT App::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
	case WM_SIZE:
		if (wp == SIZE_MINIMIZED) {  // the frame keeps its size (images can still be made)
			update_animation_timer();
			return 0;
		}
		gfx_.resize();
		layout();
		update_animation_timer();
		invalidate();
		return 0;
	case WM_DPICHANGED: {
		const RECT *r = reinterpret_cast<const RECT *>(lp);
		SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
		             SWP_NOZORDER | SWP_NOACTIVATE);
		gfx_.clear_shapes();
		layout();
		invalidate();
		return 0;
	}
	case WM_GETMINMAXINFO: {
		auto *mm = reinterpret_cast<MINMAXINFO *>(lp);
		const float s = GetDpiForWindow(hwnd) / 96.0f;
		mm->ptMinTrackSize = {static_cast<LONG>(480 * s), static_cast<LONG>(360 * s)};
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
		paint();
		return 0;
	}
	case WM_TIMER:
		if (wp == kTimerAnim) paint_animation();
		else if (wp == kTimerWatch) watch_engines();
		else if (wp == kTimerScript) pump_script();
		return 0;
	case WM_TASK:
		task_ended();
		return 0;
	case WM_ACTIVATEAPP:
		active_ = wp != 0;
		update_animation_timer();
		return 0;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (msg == WM_SYSKEYDOWN && wp != VK_LEFT && wp != VK_RIGHT && wp != VK_UP && wp != VK_DOWN) break;
		on_key(wp);
		return 0;
	case WM_CHAR:
		on_char(static_cast<wchar_t>(wp));
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		SetCapture(hwnd);
		on_mouse_down(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), false, msg == WM_LBUTTONDBLCLK);
		return 0;
	case WM_RBUTTONDOWN:
		on_mouse_down(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), true, false);
		return 0;
	case WM_LBUTTONUP:
		ReleaseCapture();
		on_mouse_up(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
		return 0;
	case WM_MOUSEMOVE:
		on_mouse_move(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
		return 0;
	case WM_MOUSELEAVE:
		on_mouse_move(-1, -1);
		return 0;
	case WM_MOUSEWHEEL: {
		POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
		ScreenToClient(hwnd, &p);
		on_wheel(p.x, p.y, GET_WHEEL_DELTA_WPARAM(wp));
		return 0;
	}
	case WM_SETCURSOR:
		if (LOWORD(lp) == HTCLIENT) {
			POINT p;
			GetCursorPos(&p);
			ScreenToClient(hwnd, &p);
			const auto in = [&](const D2D1_RECT_F &r) {
				return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
			};
			LPCWSTR c = IDC_ARROW;
			if (drag_ == Drag::Panel || (!panel_hidden_ && !lay_.panel_below && in(lay_.panel_split))) c = IDC_SIZEWE;
			else if (drag_ == Drag::Graph || (!panel_hidden_ && in(lay_.splitter))) c = IDC_SIZENS;
			else if (!panel_hidden_ && (in(lay_.fx.box) || in(lay_.fy.box))) c = IDC_IBEAM;
			SetCursor(LoadCursorW(nullptr, c));
			return TRUE;
		}
		break;
	case WM_DROPFILES: {
		HDROP drop = reinterpret_cast<HDROP>(wp);
		wchar_t file[MAX_PATH * 4];
		const bool got = DragQueryFileW(drop, 0, file, static_cast<UINT>(std::size(file))) > 0;
		DragFinish(drop);
		if (got) switch_book(file);
		return 0;
	}
	case WM_COMMAND:
		on_command(LOWORD(wp));
		return 0;
	case WM_ENGINE:
		on_engine(wp, lp != 0);
		return 0;
	case WM_CLOSE:
		save_config();
		KillTimer(hwnd, kTimerAnim);
		KillTimer(hwnd, kTimerWatch);
		KillTimer(hwnd, kTimerScript);
		script_.clear();
		if (task_.joinable()) {
			task_.request_stop();
			task_.join();
		}
		stop_engines();
		DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		DeleteFileW((temp_dir_ + L"\\nobook.dat").c_str());
		RemoveDirectoryW(temp_dir_.c_str());
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- config

void App::load_config()
{
	ini_.load(ini_path_);
	const std::string d = lower(ini_.get("view", "design", "standard"));
	design_ = d == "paw" ? Design::Paw : Design::Standard;
	const std::string e = lower(ini_.get("view", "effects", "on"));
	effects_ = e == "off" ? Effects::Off : e == "subdued" ? Effects::Subdued : Effects::On;
	const std::string te = lower(ini_.get("view", "top_effect", "normal"));
	top_effect_ = te == "strong" ? TopEffect::Strong : te == "weak" ? TopEffect::Weak : TopEffect::Normal;
	show_eval_ = ini_.get_bool("analysis", "show", true);
	use_book_ = ini_.get_bool("analysis", "book", true);
	level_ = ini_.get_int("analysis", "level", 21, 1, 60);
	threads_ = ini_.get_int("analysis", "threads", 0, 0, 1024);
	const std::string u = lower(ini_.get("analysis", "umi", "best"));
	umi_ = u == "loss" ? UmiMode::Loss : u == "off" ? UmiMode::Off : u == "range" ? UmiMode::Range : UmiMode::Best;
	umi_lo_ = ini_.get_int("analysis", "umi_lo", -64, -64, 64);
	umi_hi_ = ini_.get_int("analysis", "umi_hi", 64, -64, 64);
	umi_depth_ = ini_.get_int("analysis", "umi_depth", 60, 1, 60);
	loss_x_ = ini_.get_int("analysis", "x", 2, 0, 64);
	loss_y_ = ini_.get_int("analysis", "y", 4, 0, 128);
	base_ = lower(ini_.get("analysis", "base", "deviate"));
	if (base_ != "deviate" && base_ != "deviate2" && base_ != "deviate3") base_ = "deviate";
	stages_ = ini_.get("analysis", "stages", "1/1,2/2,3/3,4/4,5/5");
	if (stages_.empty() || stages_.find_first_not_of("0123456789/,") != std::string::npos)
		stages_ = "1/1,2/2,3/3,4/4,5/5";
	count_ = ini_.get_bool("analysis", "count", true);
	backward_ = ini_.get_bool("analysis", "backward", true);
	anim_fps_ = ini_.get_int("view", "fps", 20, 5, 60);
	font_px_ = ini_.get_int("view", "font", 0, 0, 72);
	kind_ = lower(ini_.get("engine", "kind", "nikque18")) == "newedax" ? EngineKind::Newedax : EngineKind::Nikque18;
	search_engine_ = ini_.get_bool("engine", "search", true);
	panel_w_ = static_cast<float>(ini_.get_int("window", "panel_w", 420, 200, 4000));
	graph_ratio_ = ini_.get_int("window", "graph", 34, 10, 90) / 100.0f;
	log_open_ = ini_.get_bool("window", "log", false);
	panel_hidden_ = ini_.get_bool("window", "panel_hidden", false);
	const std::string t = lower(ini_.get("window", "tab", "moves"));
	tab_ = t == "book" ? Tab::Book : t == "record" ? Tab::Record : t == "list" ? Tab::List : t == "kept" ? Tab::Kept : Tab::Moves;
	hash_bits_ = ini_.get_int("analysis", "hash", 0, 0, 30);
	if (hash_bits_ > 0 && hash_bits_ < 10) hash_bits_ = 10;
	move_numbers_ = ini_.get_bool("view", "numbers", false);
	const std::string ci = lower(ini_.get("view", "cell_info", "none"));
	cell_info_ = ci == "positions" ? CellInfo::Positions : ci == "level" ? CellInfo::Level : ci == "stage" ? CellInfo::Stage
	           : ci == "umi" ? CellInfo::Umi : CellInfo::None;
	list_start_ply_ = ini_.get_int("list", "start_ply", -1, -1, 60);
	lang_ = ini_.get("view", "language", "ja");
	if (lang_ != "en" && lang_ != "zh-Hans" && lang_ != "zh-Hant") lang_ = "ja";
	// listed even when empty, so that config.ini shows them
	if (ini_.section("script").empty()) ini_.set("script", "startup", "");
	keys_.load(ini_);
	// the engine folders are listed even when empty (empty: next to the exe, engines\nikque18 and engines\newedax)
	for (const char *sec : {"nikque18", "newedax"}) {
		const auto have = ini_.section(sec);
		for (const char *k : {"dir", "exe", "eval", "book"}) {
			const bool listed = std::any_of(have.begin(), have.end(), [&](const auto &kv) { return iequals(kv.first, k); });
			if (!listed) ini_.set(sec, k, std::strcmp(k, "exe") ? "" : "auto");
		}
	}
	lang::load(exe_dir_, lang_);
	// opening names: a file next to the exe unless set
	const std::string of = ini_.get("view", "opening_file");
	const std::wstring opath = of.empty() ? exe_dir_ + L"\\opening.ini"
	                                      : (of.size() > 1 && of[1] == ':' ? widen(of) : exe_dir_ + L"\\" + widen(of));
	openings_.load(opath);
	show_opening_ = ini_.get_bool("view", "opening", true);
}

void App::save_config()
{
	ini_.set("view", "design", design_ == Design::Paw ? "paw" : "standard");
	ini_.set("view", "effects", effects_ == Effects::Off ? "off" : effects_ == Effects::Subdued ? "subdued" : "on");
	ini_.set("view", "top_effect", top_effect_ == TopEffect::Strong ? "strong" : top_effect_ == TopEffect::Weak ? "weak" : "normal");
	ini_.set("view", "language", lang_);
	ini_.set_int("view", "numbers", move_numbers_);
	{
		const char *names[] = {"none", "positions", "level", "stage", "umi"};
		ini_.set("view", "cell_info", names[static_cast<int>(cell_info_)]);
	}
	ini_.set_int("analysis", "hash", hash_bits_);
	ini_.set_int("list", "start_ply", list_start_ply_);
	ini_.set_int("view", "fps", anim_fps_);
	ini_.set_int("view", "opening", show_opening_);
	if (ini_.get("view", "opening_file").empty() && ini_.section("view").size() > 0) ini_.set("view", "opening_file", "");
	ini_.set_int("view", "font", font_px_);
	ini_.set_int("analysis", "show", show_eval_);
	ini_.set_int("analysis", "book", use_book_);
	ini_.set_int("analysis", "level", level_);
	ini_.set_int("analysis", "threads", threads_);
	ini_.set("analysis", "umi", umi_ == UmiMode::Loss ? "loss" : umi_ == UmiMode::Off ? "off" : umi_ == UmiMode::Range ? "range" : "best");
	ini_.set_int("analysis", "umi_lo", umi_lo_);
	ini_.set_int("analysis", "umi_hi", umi_hi_);
	ini_.set_int("analysis", "umi_depth", umi_depth_);
	ini_.set_int("analysis", "x", loss_x_);
	ini_.set_int("analysis", "y", loss_y_);
	ini_.set("analysis", "base", base_);
	ini_.set("analysis", "stages", stages_);
	ini_.set_int("analysis", "count", count_);
	ini_.set_int("analysis", "backward", backward_);
	ini_.set("engine", "kind", kind_ == EngineKind::Newedax ? "newedax" : "nikque18");
	ini_.set_int("engine", "search", search_engine_);
	WINDOWPLACEMENT wp{sizeof wp};
	if (GetWindowPlacement(hwnd_, &wp)) {
		const RECT &r = wp.rcNormalPosition;
		ini_.set_int("window", "x", r.left);
		ini_.set_int("window", "y", r.top);
		ini_.set_int("window", "w", r.right - r.left);
		ini_.set_int("window", "h", r.bottom - r.top);
		ini_.set_int("window", "max", wp.showCmd == SW_SHOWMAXIMIZED);
	}
	ini_.set_int("window", "panel_w", static_cast<int>(panel_w_));
	ini_.set_int("window", "graph", static_cast<int>(std::lround(graph_ratio_ * 100)));
	ini_.set_int("window", "log", log_open_);
	ini_.set_int("window", "panel_hidden", panel_hidden_);
	{
		const char *names[] = {"moves", "book", "record", "list", "kept"};
		ini_.set("window", "tab", names[static_cast<int>(tab_)]);
	}
	if (ini_.dirty()) ini_.save(ini_path_);
}

// ---- menu

void App::build_menu()
{
	using lang::t;
	auto item = [](HMENU m, int id, const std::wstring &text, const std::wstring &key = {}) {
		std::wstring s = text;
		if (!key.empty()) s += L"\t" + key;
		AppendMenuW(m, MF_STRING, id, s.c_str());
	};
	auto sub = [](HMENU parent, HMENU m, const std::wstring &text) {
		AppendMenuW(parent, MF_POPUP, reinterpret_cast<UINT_PTR>(m), text.c_str());
	};
	auto sep = [](HMENU m) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); };
	HMENU bar = CreateMenu();

	HMENU game = CreatePopupMenu();
	item(game, cmd::NewGame, t("menu.game.new"));
	sep(game);
	item(game, cmd::Undo, t("menu.game.undo"), keys_.describe(Action::Back) + L" / " + t("key.right_click"));
	item(game, cmd::Redo, t("menu.game.redo"), keys_.describe(Action::Forward));
	item(game, cmd::ToStart, t("menu.game.start"), keys_.describe(Action::Start));
	item(game, cmd::ToEnd, t("menu.game.end"), keys_.describe(Action::End));
	sep(game);
	item(game, cmd::KeepNow, t("menu.kept.keep"), keys_.describe(Action::Keep));
	sub(bar, game, t("menu.game"));

	HMENU an = CreatePopupMenu();
	item(an, cmd::ShowEval, t("menu.analysis.show"), keys_.describe(Action::HideEval));
	item(an, cmd::Recompute, t("menu.analysis.recompute"));
	item(an, cmd::Backward, t("menu.analysis.backward"));
	HMENU lv = CreatePopupMenu();
	for (size_t i = 0; i < std::size(kLevels); ++i)
		item(lv, cmd::Level0 + static_cast<int>(i), L"Lv " + std::to_wstring(kLevels[i]));
	sub(an, lv, t("menu.analysis.level"));
	HMENU umi = CreatePopupMenu();
	item(umi, cmd::UmiBest, t("menu.analysis.umi.best"));
	item(umi, cmd::UmiLoss, t("menu.analysis.umi.loss"));
	item(umi, cmd::UmiOff, t("menu.analysis.umi.off"));
	sub(an, umi, t("menu.analysis.umi"));
	item(an, cmd::CountOn, t("menu.analysis.count"));
	sep(an);
	item(an, cmd::RangeSettings, t("menu.settings.range"));
	sub(bar, an, t("menu.analysis"));

	HMENU book = CreatePopupMenu();
	item(book, cmd::UseBook, t("menu.book.use"), keys_.describe(Action::ToggleBook));
	item(book, cmd::ReloadBook, t("menu.book.reload"));
	item(book, cmd::OpenBook, t("menu.book.open"));
	{
		HMENU rec = CreatePopupMenu();
		int shown = 0;
		for (int i = 1; i <= 8; ++i) {
			const std::string r = ini_.get("books", "recent" + std::to_string(i));
			if (r.empty()) continue;
			item(rec, cmd::RecentBook0 + i - 1, widen(r));
			++shown;
		}
		if (!shown) AppendMenuW(rec, MF_STRING | MF_GRAYED, cmd::RecentBook0, t("menu.book.recent.none").c_str());
		sub(book, rec, t("menu.book.recent"));
	}
	HMENU base = CreatePopupMenu();
	item(base, cmd::BaseDeviate, L"book deviate");
	item(base, cmd::BaseDeviate2, L"book deviate2");
	item(base, cmd::BaseDeviate3, L"book deviate3");
	sub(book, base, t("menu.book.base"));
	sep(book);
	item(book, cmd::BookInfo, t("menu.book.info"));
	HMENU show = CreatePopupMenu();
	item(show, cmd::BookShowInfo, t("menu.book.show_info"));
	item(show, cmd::BookShowStats, t("menu.book.show_stats"));
	item(show, cmd::BookShowPosition, t("menu.book.show_position"));
	sub(book, show, t("menu.book.show"));
	item(book, cmd::ExportMissing, t("menu.book.export_missing"));
	item(book, cmd::ExportBest, t("menu.book.export_best"));
	sep(book);
	HMENU keep = CreatePopupMenu();
	item(keep, cmd::BookSave, t("menu.book.save"));
	item(keep, cmd::BookSaveAs, t("menu.book.save_as"));
	item(keep, cmd::BookNewEmpty, t("menu.book.new"));
	item(keep, cmd::BookDepth, t("menu.book.depth"));
	sep(keep);
	item(keep, cmd::LearnMerge, t("menu.learn.merge"));
	item(keep, cmd::LearnSubtree, t("menu.learn.subtree"));
	item(keep, cmd::LearnPrune, t("menu.learn.prune"));
	sep(keep);
	item(keep, cmd::LearnExport, t("menu.learn.export"));
	item(keep, cmd::LearnImport, t("menu.learn.import"));
	item(keep, cmd::LearnExtract, t("menu.learn.extract"));
	item(keep, cmd::BookProblem, t("menu.book.problem"));
	sub(book, keep, t("menu.book.keep"));
	sub(bar, book, t("menu.book"));

	HMENU learn = CreatePopupMenu();
	item(learn, cmd::LearnDeviate, t("menu.learn.deviate"));
	item(learn, cmd::LearnDeviate2, t("menu.learn.deviate2"));
	item(learn, cmd::LearnDeviate3, t("menu.learn.deviate3"));
	item(learn, cmd::LearnEnhance, t("menu.learn.enhance"));
	item(learn, cmd::LearnLeafExpand, t("menu.learn.leaf_expand"));
	item(learn, cmd::LearnFill, t("menu.learn.fill"));
	item(learn, cmd::LearnFill2, t("menu.learn.fill2"));
	item(learn, cmd::LearnPlay, t("menu.learn.play"));
	sep(learn);
	item(learn, cmd::LearnStore, t("menu.learn.store"));
	item(learn, cmd::LearnAdd, t("menu.learn.add"));
	item(learn, cmd::LearnLearn, t("menu.learn.learn"));
	item(learn, cmd::LearnCheck, t("menu.learn.check"));
	sep(learn);
	item(learn, cmd::LearnNegamax, t("menu.learn.negamax"));
	item(learn, cmd::LearnFix, t("menu.learn.fix"));
	item(learn, cmd::LearnCorrect, t("menu.learn.correct"));
	HMENU leaf = CreatePopupMenu();
	item(leaf, cmd::LearnLeafRecalc1, t("menu.learn.leaf1"));
	item(leaf, cmd::LearnLeafRecalc2, t("menu.learn.leaf2"));
	item(leaf, cmd::LearnLeafRecalc3, t("menu.learn.leaf3"));
	item(leaf, cmd::LearnLeafRecalc4, t("menu.learn.leaf4"));
	sub(learn, leaf, t("menu.learn.leaf"));
	sep(learn);
	item(learn, cmd::LearnStop, t("menu.learn.stop"));
	item(learn, cmd::LearnContinue, t("menu.learn.continue"));
	sep(learn);
	item(learn, cmd::LearnSettings, t("menu.learn.settings"));
	sep(learn);
	item(learn, cmd::QueueMissing, t("menu.queue.add"));
	item(learn, cmd::QueueStart, t("menu.queue.start"));
	item(learn, cmd::QueueClear, t("menu.queue.clear"));
	sub(bar, learn, t("menu.learn"));

	HMENU eng = CreatePopupMenu();
	item(eng, cmd::EngineNikque, t("menu.engine.nikque"));
	item(eng, cmd::EngineNewedax, t("menu.engine.newedax"));
	sep(eng);
	item(eng, cmd::RestartEngines, t("menu.engine.restart"));
	item(eng, cmd::EngineVersion, t("menu.engine.version"));
	sep(eng);
	item(eng, cmd::ToolSolve, t("menu.tool.solve"));
	item(eng, cmd::ToolBench, t("menu.tool.bench"));
	item(eng, cmd::ToolCount, t("menu.tool.count"));
	item(eng, cmd::ToolPerft, t("menu.tool.perft"));
	item(eng, cmd::ToolEstimate, t("menu.tool.estimate"));
	item(eng, cmd::ToolStop, t("menu.tool.stop"));
	sep(eng);
	item(eng, cmd::EngineCommand, t("menu.engine.command"));
	sub(bar, eng, t("menu.engine"));

	HMENU io = CreatePopupMenu();
	item(io, cmd::CopyMoves, t("menu.io.copy"), keys_.describe(Action::CopyMoves));
	item(io, cmd::PasteMoves, t("menu.io.paste"), keys_.describe(Action::PasteMoves));
	item(io, cmd::FixRecord, t("menu.io.fix"));
	HMENU pos = CreatePopupMenu();
	item(pos, cmd::CopyBoardText, t("menu.io.copy_board"));
	item(pos, cmd::CopyObf, t("menu.io.copy_obf"));
	item(pos, cmd::CopyBitDecimal, t("menu.io.copy_dec"));
	item(pos, cmd::CopyBitHex, t("menu.io.copy_hex"));
	item(pos, cmd::CopyBitNtest, t("menu.io.copy_ntest"));
	sep(pos);
	item(pos, cmd::ImportPosition, t("menu.io.import"));
	sub(io, pos, t("menu.io.position"));
	HMENU ed = CreatePopupMenu();
	item(ed, cmd::EditBoard, t("menu.edit.start"), keys_.describe(Action::EditBoard));
	item(ed, cmd::EditDone, t("menu.edit.done"), L"Enter");
	item(ed, cmd::EditCancel, t("menu.edit.cancel"), L"Esc");
	item(ed, cmd::EditSide, t("menu.edit.side"), L"T");
	item(ed, cmd::EditClear, t("menu.edit.clear"), L"Delete");
	item(ed, cmd::EditInitial, t("menu.edit.initial"), L"I");
	sub(io, ed, t("menu.edit"));
	item(io, cmd::EstimateLine, t("menu.io.estimate"));
	sep(io);
	item(io, cmd::Screenshot, t("menu.io.screenshot"), keys_.describe(Action::Screenshot));
	item(io, cmd::SaveBoardImage, t("menu.io.board_image"));
	sep(io);
	HMENU lst = CreatePopupMenu();
	item(lst, cmd::ListOpen, t("menu.list.open"));
	item(lst, cmd::ListAdd, t("menu.list.add"));
	item(lst, cmd::ListSave, t("menu.list.save"));
	item(lst, cmd::ListAddGame, t("menu.list.add_game"));
	item(lst, cmd::ListFromBook, t("menu.list.book"));
	sep(lst);
	item(lst, cmd::ListSort, t("menu.list.sort"));
	item(lst, cmd::ListUnique, t("menu.list.unique"));
	item(lst, cmd::ListTruncate, t("menu.list.truncate"));
	item(lst, cmd::ListShuffle, t("menu.list.shuffle"));
	item(lst, cmd::ListNormalize, t("menu.list.normalize"));
	item(lst, cmd::ListScatter, t("menu.list.scatter"));
	sep(lst);
	item(lst, cmd::ListPrev, t("menu.list.prev"), keys_.describe(Action::ListPrev));
	item(lst, cmd::ListNext, t("menu.list.next"), keys_.describe(Action::ListNext));
	item(lst, cmd::ListReload, t("menu.list.reload"), keys_.describe(Action::ListReload));
	item(lst, cmd::ListDelete, t("menu.list.delete"), keys_.describe(Action::ListDelete));
	item(lst, cmd::ListClear, t("menu.list.clear"));
	item(lst, cmd::ListStartPly, t("menu.list.start"));
	sub(io, lst, t("menu.list"));
	item(io, cmd::ListSearch, t("menu.list.search"));
	item(io, cmd::ListSearchClear, t("menu.list.search_clear"));
	item(io, cmd::TaskCancel, t("menu.task.cancel"));
	sep(io);
	HMENU bs = CreatePopupMenu();
	item(bs, cmd::BaseConvert, t("menu.base.convert"));
	item(bs, cmd::BaseUnique, t("menu.base.unique"));
	item(bs, cmd::BaseCompare, t("menu.base.compare"));
	item(bs, cmd::BaseCheck, t("menu.base.check"));
	item(bs, cmd::BaseCorrect, t("menu.base.correct"));
	item(bs, cmd::BaseComplete, t("menu.base.complete"));
	item(bs, cmd::BaseProblem, t("menu.base.problem"));
	item(bs, cmd::BaseTofen, t("menu.base.tofen"));
	sub(io, bs, t("menu.base"));
	sep(io);
	item(io, cmd::RunScript, t("menu.io.script"));
	item(io, cmd::StopScript, t("menu.io.script_stop"));
	sub(bar, io, t("menu.io"));

	HMENU view = CreatePopupMenu();
	HMENU des = CreatePopupMenu();
	item(des, cmd::DesignStandard, t("menu.view.design.standard"));
	item(des, cmd::DesignPaw, t("menu.view.design.paw"));
	sub(view, des, t("menu.view.design"));
	HMENU eff = CreatePopupMenu();
	item(eff, cmd::EffectsOn, t("menu.view.effects.on"));
	item(eff, cmd::EffectsSubdued, t("menu.view.effects.subdued"));
	item(eff, cmd::EffectsOff, t("menu.view.effects.off"));
	sub(view, eff, t("menu.view.effects"));
	HMENU top = CreatePopupMenu();
	item(top, cmd::TopStrong, t("menu.view.top.strong"));
	item(top, cmd::TopNormal, t("menu.view.top.normal"));
	item(top, cmd::TopWeak, t("menu.view.top.weak"));
	sub(view, top, t("menu.view.top"));
	sep(view);
	HMENU ori = CreatePopupMenu();
	for (int s = 0; s < 8; ++s) item(ori, cmd::Rotate0 + s, t("orient." + std::to_string(s)));
	sep(ori);
	item(ori, cmd::RotateNext, t("menu.view.orient.next"), keys_.describe(Action::Rotate));
	sub(view, ori, t("menu.view.orient"));
	item(view, cmd::MoveNumbers, t("menu.view.numbers"));
	HMENU ci = CreatePopupMenu();
	item(ci, cmd::CellInfoNone, t("menu.view.cell.none"));
	item(ci, cmd::CellInfoPositions, t("menu.view.cell.positions"));
	item(ci, cmd::CellInfoLevel, t("menu.view.cell.level"));
	item(ci, cmd::CellInfoStage, t("menu.view.cell.stage"));
	item(ci, cmd::CellInfoUmi, t("menu.view.cell.umi"));
	sub(view, ci, t("menu.view.cell"));
	sep(view);
	item(view, cmd::TabMoves, t("menu.view.tab.moves"));
	item(view, cmd::TabBook, t("menu.view.tab.book"));
	item(view, cmd::TabRecord, t("menu.view.tab.record"));
	item(view, cmd::TabList, t("menu.view.tab.list"));
	item(view, cmd::TabKept, t("menu.view.tab.kept"));
	HMENU kp = CreatePopupMenu();
	item(kp, cmd::KeepNow, t("menu.kept.keep"), keys_.describe(Action::Keep));
	item(kp, cmd::KeptShow, t("menu.kept.show"));
	item(kp, cmd::KeptCompare, t("menu.kept.compare"));
	item(kp, cmd::KeptDelete, t("menu.kept.delete"), L"Delete");
	item(kp, cmd::KeptClear, t("menu.kept.clear"));
	sub(view, kp, t("menu.kept"));
	sep(view);
	item(view, cmd::TogglePanel, t("menu.view.panel"), keys_.describe(Action::TogglePanel));
	item(view, cmd::ToggleLog, t("menu.view.log"), keys_.describe(Action::ToggleLog));
	sub(bar, view, t("menu.view"));

	HMENU set = CreatePopupMenu();
	item(set, cmd::RangeSettings, t("menu.settings.range"));
	item(set, cmd::LearnSettings, t("menu.learn.settings"));
	item(set, cmd::OpenConfig, t("menu.settings.config"));
	HMENU lg = CreatePopupMenu();
	item(lg, cmd::LangJa, L"日本語");
	item(lg, cmd::LangEn, L"English");
	item(lg, cmd::LangZhHans, L"简体中文");
	item(lg, cmd::LangZhHant, L"繁體中文");
	sub(set, lg, t("menu.settings.lang"));
	sub(bar, set, t("menu.settings"));

	HMENU help = CreatePopupMenu();
	item(help, cmd::HelpKeys, t("menu.help.keys"));
	item(help, cmd::HelpAbout, t("menu.help.about"));
	sub(bar, help, t("menu.help"));

	HMENU old = GetMenu(hwnd_);
	SetMenu(hwnd_, bar);
	if (old) DestroyMenu(old);
	update_menu();
}

void App::update_menu()
{
	HMENU m = GetMenu(hwnd_);
	if (!m) return;
	auto check = [&](int id, bool on) { CheckMenuItem(m, id, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED)); };
	auto radio = [&](int first, int last, int on) { CheckMenuRadioItem(m, first, last, on, MF_BYCOMMAND); };
	auto enable = [&](int id, bool on) { EnableMenuItem(m, id, MF_BYCOMMAND | (on ? MF_ENABLED : MF_GRAYED)); };
	check(cmd::ShowEval, show_eval_);
	check(cmd::Backward, backward_);
	check(cmd::FixRecord, fixed_on_);
	enable(cmd::LearnStop, learning_);
	for (int id = cmd::LearnDeviate; id <= cmd::LearnJobLast; ++id) enable(id, !learning_);
	enable(cmd::LearnContinue, !learning_);
	enable(cmd::ExportBest, !learning_);
	enable(cmd::ListFromBook, !learning_ && !task_busy_);
	enable(cmd::QueueStart, !learning_ && !learn_queue_.empty());
	enable(cmd::QueueClear, !learn_queue_.empty());
	for (int id = cmd::ToolSolve; id < cmd::ToolStop; ++id) enable(id, !tool_busy_);
	for (int id = cmd::BaseConvert; id <= cmd::BaseTofen; ++id) enable(id, !tool_busy_);
	enable(cmd::ToolStop, tool_busy_);
	// the list waits for the job in progress
	for (int id = cmd::ListOpen; id < cmd::TaskCancel; ++id) enable(id, !task_busy_);
	const bool have_list = !list_.empty() && !task_busy_;
	for (int id : {cmd::ListSave, cmd::ListSort, cmd::ListUnique, cmd::ListTruncate, cmd::ListShuffle, cmd::ListNormalize,
	               cmd::ListScatter, cmd::ListClear, cmd::ListPrev, cmd::ListNext, cmd::ListSearch})
		enable(id, have_list);
	enable(cmd::ListDelete, have_list && list_index_ >= 0);
	enable(cmd::ListReload, have_list && list_index_ >= 0);
	enable(cmd::ListSearchClear, hits_shown_);
	enable(cmd::TaskCancel, task_busy_);
	enable(cmd::StopScript, !script_.empty() || script_wait_ != ScriptWait::None);
	for (int id : {cmd::KeptShow, cmd::KeptCompare, cmd::KeptDelete}) enable(id, kept_sel_ >= 0 && kept_sel_ < static_cast<int>(kept_.size()));
	enable(cmd::KeptClear, !kept_.empty());
	for (int id = cmd::EditDone; id <= cmd::EditInitial; ++id) enable(id, editing_);
	check(cmd::EditBoard, editing_);
	enable(cmd::EstimateLine, !editing_ && !findline_id_ && !task_busy_);
	check(cmd::UseBook, use_book_);
	check(cmd::CountOn, count_);
	check(cmd::TogglePanel, panel_hidden_);
	check(cmd::ToggleLog, log_open_);
	check(cmd::MoveNumbers, move_numbers_);
	int lv = -1;
	for (size_t i = 0; i < std::size(kLevels); ++i)
		if (kLevels[i] == level_) lv = static_cast<int>(i);
	for (size_t i = 0; i < std::size(kLevels); ++i) check(cmd::Level0 + static_cast<int>(i), static_cast<int>(i) == lv);
	radio(cmd::UmiBest, cmd::UmiOff, umi_ == UmiMode::Best ? cmd::UmiBest : umi_ == UmiMode::Loss ? cmd::UmiLoss : cmd::UmiOff);
	radio(cmd::BaseDeviate, cmd::BaseDeviate3,
	      base_ == "deviate2" ? cmd::BaseDeviate2 : base_ == "deviate3" ? cmd::BaseDeviate3 : cmd::BaseDeviate);
	radio(cmd::EngineNikque, cmd::EngineNewedax, kind_ == EngineKind::Newedax ? cmd::EngineNewedax : cmd::EngineNikque);
	radio(cmd::DesignStandard, cmd::DesignPaw, design_ == Design::Paw ? cmd::DesignPaw : cmd::DesignStandard);
	radio(cmd::EffectsOn, cmd::EffectsOff,
	      effects_ == Effects::On ? cmd::EffectsOn : effects_ == Effects::Subdued ? cmd::EffectsSubdued : cmd::EffectsOff);
	radio(cmd::TopStrong, cmd::TopWeak, cmd::TopStrong + static_cast<int>(top_effect_));
	radio(cmd::TabMoves, cmd::TabKept, cmd::TabMoves + static_cast<int>(tab_));
	radio(cmd::Rotate0, cmd::Rotate0 + 7, cmd::Rotate0 + orient_);
	radio(cmd::CellInfoNone, cmd::CellInfoUmi, cmd::CellInfoNone + static_cast<int>(cell_info_));
	const int lang_id = lang_ == "en" ? cmd::LangEn : lang_ == "zh-Hans" ? cmd::LangZhHans : lang_ == "zh-Hant" ? cmd::LangZhHant : cmd::LangJa;
	radio(cmd::LangJa, cmd::LangZhHant, lang_id);
}

void App::set_language(const std::string &code)
{
	lang_ = code;
	ini_.set("view", "language", code);
	lang::load(exe_dir_, code);
	gfx_.clear_fonts();
	SetWindowTextW(hwnd_, lang::t("app.title").c_str());
	build_menu();
	layout();
	invalidate();
}

void App::set_window_icon()
{
	const int id = design_ == Design::Paw ? IDI_PAW : IDI_APP;
	const UINT dpi = GetDpiForWindow(hwnd_);
	HICON big = static_cast<HICON>(LoadImageW(inst_, MAKEINTRESOURCEW(id), IMAGE_ICON,
	                                          GetSystemMetricsForDpi(SM_CXICON, dpi),
	                                          GetSystemMetricsForDpi(SM_CYICON, dpi), 0));
	HICON icon_s = static_cast<HICON>(LoadImageW(inst_, MAKEINTRESOURCEW(id), IMAGE_ICON,
	                                            GetSystemMetricsForDpi(SM_CXSMICON, dpi),
	                                            GetSystemMetricsForDpi(SM_CYSMICON, dpi), 0));
	if (HICON old = reinterpret_cast<HICON>(SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big))))
		DestroyIcon(old);
	if (HICON old = reinterpret_cast<HICON>(SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon_s))))
		DestroyIcon(old);
}

// ---- layout

void App::layout()
{
	Layout &L = lay_;
	const float W = static_cast<float>(gfx_.width()), H = static_cast<float>(gfx_.height());
	const Theme &th = theme(design_);
	L.scale = GetDpiForWindow(hwnd_) / 96.0f;
	// the text grows with a tall window (14 to 22 pixels at 96 DPI), or is the size set in config.ini
	const float base = font_px_ > 0 ? static_cast<float>(font_px_) : std::clamp(H / L.scale / 60.0f, 14.0f, 22.0f);
	L.font = std::round(base * L.scale);
	L.font_s = std::round(base * 0.82f * L.scale);
	L.row = std::round(L.font * 1.8f);
	L.bar = {0, 0, W, L.row};
	const float log_h = log_open_ ? std::max(L.row * 8, std::round(H * 0.28f)) : L.row;
	L.log = {0, H - log_h, W, H};
	const float top = L.row, bottom = std::max(top + 1, H - log_h);
	const float ch = bottom - top;
	const float coord = std::round(L.font * 1.2f);
	const float pad = std::round(6 * L.scale);
	const float want_panel = panel_hidden_ ? 0 : std::max(300 * L.scale, panel_w_ * L.scale);
	L.panel_below = !panel_hidden_ && (W - want_panel < ch * 0.7f) && H > W;
	float area_w = W, area_h = ch;
	if (!panel_hidden_) {
		if (L.panel_below) area_h = std::round(ch * 0.62f);
		else area_w = std::max(120.0f, W - want_panel);
	}
	const float avail = std::max(40.0f, std::min(area_w - coord - 2 * pad, area_h - coord - 2 * pad));
	int cell = std::max(4, static_cast<int>(avail / 8));
	int lw = 1, fw = 2;
	for (; cell > 4; --cell) {
		lw = std::max(1, static_cast<int>(std::lround(cell / 44.0)));
		fw = std::max(lw * 2, static_cast<int>(std::lround(cell * th.frame_ratio)));
		if (8 * cell + 9 * lw + 2 * fw <= avail) break;
	}
	L.cell = cell;
	L.line = lw;
	L.frame = fw;
	L.board_px = 8 * cell + 9 * lw + 2 * fw;
	const float block = L.board_px + coord;  // the board with the coordinates
	if (panel_hidden_) {
		L.board_area = {0, top, W, bottom};
		L.panel = {W, top, W, bottom};
	} else if (!L.panel_below) {
		const float used = std::round(block + 2 * pad);
		L.board_area = {0, top, used, bottom};
		L.panel = {used, top, W, bottom};
	} else {
		const float used = std::round(block + 2 * pad);
		L.board_area = {0, top, W, top + used};
		L.panel = {0, top + used, W, bottom};
	}
	const float aw = L.board_area.right - L.board_area.left, ah = L.board_area.bottom - L.board_area.top;
	L.bx = static_cast<int>(L.board_area.left + coord + std::floor((aw - block) / 2));
	L.by = static_cast<int>(L.board_area.top + coord + std::floor((ah - block) / 2));
	L.panel_split = {L.panel.left - 4 * L.scale, top, L.panel.left + 4 * L.scale, bottom};

	// the panel
	const D2D1_RECT_F P{L.panel.left + pad, L.panel.top + pad, L.panel.right - pad, L.panel.bottom - pad};
	L.tabs = {P.left, P.top, P.right, P.top + L.row};
	L.range = {P.left, L.tabs.bottom + 4 * L.scale, P.right, L.tabs.bottom + 4 * L.scale + L.row};
	const float rest_top = L.range.bottom + 4 * L.scale;
	const float rest_h = std::max(0.0f, P.bottom - rest_top);
	const float split_h = std::round(7 * L.scale);
	float gh = std::round(rest_h * graph_ratio_);
	gh = std::clamp(gh, std::min(L.row * 3, rest_h * 0.5f), std::max(L.row * 3, rest_h - L.row * 4));
	L.table = {P.left, rest_top, P.right, P.bottom - gh - split_h};
	L.splitter = {P.left, L.table.bottom, P.right, L.table.bottom + split_h};
	L.graph = {P.left, L.splitter.bottom, P.right, P.bottom};

	// the range row: "label  X [ 2 ] ---o---  Y [ 4 ] ---o---"
	IDWriteTextFormat *f = gfx_.text(L.font, false);
	const float gap = std::round(8 * L.scale);
	float x = L.range.left + gfx_.text_width(lang::t("range.title"), f) + gap * 1.5f;
	const float box_w = std::round(L.font * 2.8f);
	const float lx = gfx_.text_width(lang::t("range.x"), f), ly = gfx_.text_width(lang::t("range.y"), f);
	const float free = L.range.right - x - lx - ly - 2 * box_w - 6 * gap;
	const float track_w = free >= 60 * L.scale ? std::min(160 * L.scale, free / 2) : 0;
	const float bt = L.range.top + 3 * L.scale, bb = L.range.bottom - 3 * L.scale;
	x += lx + gap * 0.5f;
	L.fx.box = {x, bt, x + box_w, bb};
	L.fx.track = {x + box_w + gap, bt, x + box_w + gap + track_w, bb};
	x = L.fx.track.right + gap * 2 + ly + gap * 0.5f;
	L.fy.box = {x, bt, x + box_w, bb};
	L.fy.track = {x + box_w + gap, bt, x + box_w + gap + track_w, bb};
}

// ---- drawing

double App::now_s() const
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0_).count();
}

void App::paint()
{
	measure::Frame mf(true);
	compute_line();
	prepare_glows();
	if (!gfx_.begin_frame()) return;
	const Theme &th = theme(design_);
	gfx_.dc()->Clear(th.bg);
	draw_board(*this);
	draw_panel(*this);
	draw_log(*this);
	draw_bar(*this);
	gfx_.end_frame();
	measure::first_frame();
}

void App::paint_animation()
{
	const std::vector<int> cells = animated_cells();
	if (cells.empty() || !active_ || IsIconic(hwnd_)) {
		update_animation_timer();
		return;
	}
	measure::Frame mf(false);
	if (!gfx_.begin_frame()) return;
	std::vector<D2D1_RECT_F> dirty;
	dirty.reserve(cells.size());
	const double t = now_s();
	for (const int sq : cells) {
		const D2D1_RECT_F r = lay_.cell_rect(sq);
		gfx_.dc()->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_ALIASED);
		draw_cell(*this, sq, t);
		gfx_.dc()->PopAxisAlignedClip();
		dirty.push_back(r);
	}
	gfx_.end_frame(&dirty);
}

void App::update_animation_timer()
{
	const bool want = effects_ == Effects::On && active_ && hwnd_ && !IsIconic(hwnd_) && !animated_cells().empty();
	if (want == animating_) return;
	animating_ = want;
	if (want) SetTimer(hwnd_, kTimerAnim, 1000 / anim_fps_, nullptr);
	else KillTimer(hwnd_, kTimerAnim);
}

CellView App::cell_view(int sq) const
{
	CellView v;
	if (editing_) {  // a position being set up: its discs only
		if ((edit_pos_.black >> sq) & 1) v.stone = 1;
		else if ((edit_pos_.white >> sq) & 1) v.stone = 2;
		return v;
	}
	const Position &p = game_.now();
	if ((p.black >> sq) & 1) v.stone = 1;
	else if ((p.white >> sq) & 1) v.stone = 2;
	// the last move (a pass belongs to the move before)
	for (int i = game_.cursor() - 1; i >= 0; --i) {
		if (game_.moves()[i] == kPass) continue;
		v.last = game_.moves()[i] == sq;
		break;
	}
	if (v.stone && move_numbers_) v.number = move_number_at(sq);
	if (!show_eval_ || v.stone) return v;
	const PosInfo *info = cache_.peek(p);

	// the expected line of the move under the mouse, from the reply on: numbers on the squares
	if (info && hover_sq_ != kNoMove && hover_sq_ != sq) {
		if (const MoveEval *h = info->find(hover_sq_); h && h->pv.size() > 1) {
			int side = p.side ^ 1, num = 0;
			for (size_t k = 1; k < h->pv.size() && num < 12; ++k) {
				if (h->pv[k] == kPass) {
					side ^= 1;
					continue;
				}
				++num;
				if (h->pv[k] == sq) {
					v.pv_num = num;
					v.pv_side = side;
					break;
				}
				side ^= 1;
			}
		}
	}
	if (!p.can_play(sq)) return v;
	v.selected = sq == selected_sq_;
	const MoveEval *m = info ? info->find(sq) : nullptr;
	if (!m) {
		v.pending = eng_[0].alive || eng_[1].alive;
		return v;
	}
	// the move played next in the game takes the value that came back from the next position
	const int cur = game_.cursor();
	const LineEval *le = (line_.size() == static_cast<size_t>(game_.size()) + 1 && cur < game_.size()) ? &line_[cur] : nullptr;
	const int played_sq = cur < game_.size() ? game_.moves()[cur] : kNoMove;
	auto value_of = [&](const MoveEval &o, int *x) {
		if (le && o.sq == played_sq && le->played_improved) {
			*x = le->played;
			return true;
		}
		return shown_value(o, use_book_, x);
	};
	v.has_value = value_of(*m, &v.value);
	v.stage = (use_book_ && m->book_row && m->stage > 0) ? m->stage : 0;
	if (v.has_value && v.stage == 0) {
		v.level = shown_level(*m, use_book_);
		v.exact = move_exact(*m, p);
		if (le && sq == played_sq && le->played_improved) {
			v.improved = true;
			v.exact = le->played_exact;
		}
	}
	v.pending = !v.has_value && (eng_[1].alive || (use_book_ && eng_[0].alive && !info->book_done));
	if (cell_info_ != CellInfo::None && v.has_value) v.info = cell_info_text(*m, cell_info_, use_book_);
	int best = -999;
	for (const auto &o : info->moves) {
		int x;
		if (value_of(o, &x)) best = std::max(best, x);
	}
	if (v.has_value) {
		v.best = v.value == best;
		v.good = !v.best && v.value >= best - 2;
	}
	// the fixed record, when the game has followed it up to here
	if (fixed_on_ && fixed_start_ == game_.start() && cur < static_cast<int>(fixed_.size()) && fixed_[cur] == sq &&
	    cur <= game_.size() && std::equal(fixed_.begin(), fixed_.begin() + cur, game_.moves().begin())) {
		v.fixed_mark = cur >= game_.size() || game_.moves()[cur] != sq;
		v.fixed_bad = v.has_value && v.value < best;
	}
	return v;
}

std::vector<int> App::animated_cells() const
{
	std::vector<int> out;
	if (effects_ != Effects::On || !show_eval_) return out;
	u64 m = game_.now().moves();
	while (m) {
		const int sq = std::countr_zero(m);
		m &= m - 1;
		const CellView v = cell_view(sq);
		if ((v.has_value && v.stage >= 3) || v.pending) out.push_back(sq);
	}
	return out;
}

void App::log(const std::wstring &line)
{
	log_.push_back(line);
	while (log_.size() > 500) log_.pop_front();
	status_ = line;
	InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::set_status(const std::wstring &s)
{
	if (s == status_) return;
	status_ = s;
	invalidate();
}

}  // namespace ng

namespace ng {

void App::compute_line()
{
	line_ = backward_line(game_, cache_, use_book_, backward_);
}

}  // namespace ng
