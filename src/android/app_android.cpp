// The parts of App that are Android's own: the loop of the thread of the app (the events of the screen, the messages
// posted to the window, the timers, the drawing), as WndProc of src/win/app_win.cpp hands them to the App. A finger is
// the mouse (android/.../MainActivity.java). This version has no text box of the moves (they are pasted with the
// button of their row) and no file dialogs (the commands that ask for a file are not in its menus).
#include "../app.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <map>

#include "../lang.h"
#include "android_bridge.h"

namespace ng {

// (the buttons of the screen send these commands: MainActivity.java)
static_assert(cmd::Undo == 1001 && cmd::Redo == 1002 && cmd::ToStart == 1003 && cmd::ToEnd == 1004);

namespace {

using Clock = std::chrono::steady_clock;

struct Timer {
	std::chrono::milliseconds every;
	Clock::time_point next;
};

std::map<UINT_PTR, Timer> g_timers;
int g_window = 0;  // (the address of it is the window of the App)

void handle(App &a, const AndroidEvent &e)
{
	AndroidState &s = android_state();
	const int x = static_cast<int>(e.a), y = static_cast<int>(e.b);
	switch (e.kind) {
	case kEvResize:
		s.width = x;
		s.height = y;
		s.surface = true;
		a.gfx_.resize();
		a.gfx_.clear_shapes();
		a.layout();
		a.update_animation_timer();
		a.invalidate();
		break;
	case kEvSurface:
		s.surface = e.a != 0;
		a.update_animation_timer();
		if (s.surface) a.invalidate();
		break;
	case kEvTouchDown:
		a.on_mouse_move(x, y);
		a.on_mouse_down(x, y, false, e.c != 0);
		break;
	case kEvTouchMove: a.on_mouse_move(x, y); break;
	case kEvTouchUp:
		a.on_mouse_up(x, y);
		a.on_mouse_move(-1, -1);  // (the finger has left the screen: nothing is under it)
		break;
	case kEvRightClick:
		a.on_mouse_move(x, y);
		a.on_mouse_down(x, y, true, false);
		a.on_mouse_move(-1, -1);
		break;
	case kEvWheel: a.on_wheel(x, y, static_cast<int>(e.c)); break;
	case kEvCommand: a.on_command(x); break;
	case kEvKey:
		s.ctrl = (e.b & 1) != 0;
		s.shift = (e.b & 2) != 0;
		a.on_key(static_cast<WPARAM>(e.a));
		if (!s.ctrl && e.a >= '0' && e.a <= '9') a.on_char(static_cast<wchar_t>(e.a));
		s.ctrl = s.shift = false;
		break;
	case kEvActive:
		s.active = e.a != 0;
		a.active_ = s.active;
		a.update_animation_timer();
		// (the system may end the app while it is behind: the settings are kept now)
		if (!s.active) a.save_config();
		else a.invalidate();
		break;
	case kEvClose: a.close_window(); break;
	case kEvMessage:
		switch (static_cast<UINT>(e.a)) {
		case WM_TASK: a.task_ended(); break;
		case WM_SCRIPT: a.pump_script(); break;
		case WM_REDUCE: a.reduce_counted(); break;
		case WM_BOOK_PEEK: a.book_peek_answers(); break;
		case WM_EVAL_WARN: a.eval_mismatch_box(); break;
		case WM_ENGINE: a.on_engine(static_cast<WPARAM>(e.b), e.c); break;
		case WM_CLOSE: a.close_window(); break;
		default: break;
		}
		break;
	default: break;
	}
}

void fire_timers(App &a)
{
	const auto now = Clock::now();
	std::vector<UINT_PTR> due;
	for (auto &[id, t] : g_timers) {
		if (t.next > now) continue;
		t.next = now + t.every;
		due.push_back(id);
	}
	for (const UINT_PTR id : due) {
		if (!g_timers.count(id)) continue;  // (killed by a timer before it)
		if (id == kTimerAnim) a.paint_animation();
		else if (id == kTimerWatch) a.watch_engines();
		else if (id == kTimerScript) a.pump_script();
		else if (id == kTimerSave) a.watch_save();
		else play_timer(a, id);
	}
}

long long wait_ms()
{
	long long ms = 1000;
	const auto now = Clock::now();
	for (const auto &[id, t] : g_timers) {
		const long long left = std::chrono::duration_cast<std::chrono::milliseconds>(t.next - now).count();
		ms = std::min(ms, std::max(0LL, left));
	}
	return ms;
}

}  // namespace

int App::run(HINSTANCE, int)
{
	AndroidState &s = android_state();
	g_timers.clear();
	prepare();
	hwnd_ = &g_window;
	if (!gfx_.init(hwnd_)) return 1;
	create_record_box();
	build_menu();

	// the view has its size before the engines start and the startup script runs (as on Windows); the app closed before
	// that ends without starting them
	while (s.width <= 0 && !s.quit) {
		AndroidEvent e;
		if (!pop_event(&e, 1000)) continue;
		if (e.kind == kEvResize || e.kind == kEvSurface || e.kind == kEvActive || e.kind == kEvClose ||
		    (e.kind == kEvMessage && static_cast<UINT>(e.a) == WM_CLOSE))
			handle(*this, e);
	}
	if (!s.quit) {
		layout();
		started();
	}

	while (!s.quit) {
		AndroidEvent e;
		// (without a surface the drawing waits for it: the thread sleeps until an event or a timer)
		if (pop_event(&e, s.needs_paint && s.surface ? 0 : wait_ms())) {
			handle(*this, e);
			while (!s.quit && pop_event(&e, 0)) handle(*this, e);
		}
		if (s.quit) break;
		fire_timers(*this);
		if (s.needs_paint && s.surface && !s.quit) {
			s.needs_paint = false;
			gfx_.resize();  // (the picture of the window again, when it could not be made before)
			paint();
		}
	}

	// the temporary folder of this run (empty books, files given to the engines)
	std::error_code ec;
	std::filesystem::remove_all(narrow(temp_dir_), ec);
	g_timers.clear();
	hwnd_ = nullptr;
	return book_save_failed_ ? 1 : 0;
}

App::~App() = default;

void App::set_window_icon() {}  // (the icon of the app is in its package)

void App::set_timer(UINT_PTR id, UINT ms)
{
	const auto every = std::chrono::milliseconds(std::max<UINT>(1, ms));
	g_timers[id] = Timer{every, Clock::now() + every};
}

void App::kill_timer(UINT_PTR id) { g_timers.erase(id); }

void App::post(UINT msg) { post_message(hwnd_, msg); }

bool App::window_enabled() const { return android_state().modal == 0; }

// (no file dialogs in this version: the commands that ask for a file are not in the menus)
std::wstring App::pick_open(const wchar_t *, const std::wstring &) { return {}; }

std::wstring App::pick_save(const wchar_t *, const wchar_t *, const std::wstring &, const std::wstring &) { return {}; }

// ---- the text box of the row of the moves: none (the row shows the moves; its buttons copy and paste them)

void App::create_record_box() {}
void App::place_record_box() {}
void App::sync_record_box(bool) {}
void App::leave_record_box() { record_resume_ = false; }
void App::record_focus_lost(HWND) {}
bool App::record_resumes() { return false; }
void App::record_key(WPARAM) {}
LRESULT App::record_color(HDC) { return 0; }

}  // namespace ng
