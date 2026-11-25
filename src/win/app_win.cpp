// The parts of App that are Windows' own: the window and its messages, the timers, the icon, the file dialogs.
#include "../app.h"

#include <shellapi.h>
#include <windowsx.h>

#include <filesystem>

#include "lang.h"
#include "measure.h"
#include "resource.h"

namespace ng {

namespace {

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

}  // namespace

int App::run(HINSTANCE inst, int show)
{
	inst_ = inst;
	prepare();

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
	CreateWindowExW(0, wc.lpszClassName, lang::t("app.title").c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT,
	                CW_USEDEFAULT, w, h, nullptr, nullptr, inst, this);
	if (!hwnd_) return 1;
	if (x != CW_USEDEFAULT && y != CW_USEDEFAULT) {
		RECT r{x, y, x + w, y + h};
		if (MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) SetWindowPos(hwnd_, nullptr, x, y, w, h, SWP_NOZORDER);
	}
	if (!gfx_.init(hwnd_)) {
		message(lang::t("err.d2d"), MB_ICONERROR);
		return 1;
	}
	DragAcceptFiles(hwnd_, TRUE);
	create_record_box();
	build_menu();
	set_window_icon();
	gfx_.resize();
	layout();
	ShowWindow(hwnd_, ini_.get_bool("window", "max", false) ? SW_SHOWMAXIMIZED : show);
	UpdateWindow(hwnd_);
	started();

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
		else if (wp == kTimerSave) watch_save();
		else play_timer(*this, wp);
		return 0;
	case WM_TASK:
		task_ended();
		return 0;
	case WM_SCRIPT:
		pump_script();
		return 0;
	case WM_REDUCE:
		reduce_counted();
		return 0;
	case WM_BOOK_PEEK:
		book_peek_answers();
		return 0;
	case WM_EVAL_WARN:
		eval_mismatch_box();
		return 0;
	case WM_ACTIVATEAPP:
		active_ = wp != 0;
		update_animation_timer();
		return 0;
	case WM_SETFOCUS:  // (given by the activation of the window too)
		if (record_resumes()) SetFocus(record_edit_);
		break;
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
	case WM_CAPTURECHANGED:  // the mouse taken by another window (a box, Alt+Tab): no button-up comes, the drag ends here
		if (reinterpret_cast<HWND>(lp) != hwnd) on_mouse_up(-1, -1);
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
			LPCWSTR c = IDC_ARROW;
			if (drag_ == Drag::Panel || (!panel_hidden_ && !lay_.panel_below && inside(lay_.panel_split, p))) c = IDC_SIZEWE;
			else if (drag_ == Drag::Graph || (!panel_hidden_ && inside(lay_.splitter, p))) c = IDC_SIZENS;
			else if (!panel_hidden_ && (inside(lay_.fx.box, p) || inside(lay_.fy.box, p) || inside(lay_.record_text, p))) c = IDC_IBEAM;
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
		if (!lp) on_command(LOWORD(wp));  // (not the notes of the text box)
		return 0;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:  // (the text box while it is disabled)
		return record_color(reinterpret_cast<HDC>(wp));
	case WM_ENGINE:
		on_engine(wp, lp != 0);
		return 0;
	case WM_CLOSE:
		close_window();
		return 0;
	case WM_DESTROY: {
		// the temporary folder of this run (empty books, files given to the engines)
		std::error_code ec;
		std::filesystem::remove_all(temp_dir_, ec);
		PostQuitMessage(0);
		return 0;
	}
	}
	return DefWindowProcW(hwnd, msg, wp, lp);
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

std::wstring App::pick_open(const wchar_t *patterns_or_filter, const std::wstring &title)
{
	wchar_t file[MAX_PATH * 4] = L"";
	OPENFILENAMEW of{sizeof of};
	of.hwndOwner = hwnd_;
	of.lpstrFilter = patterns_or_filter;
	of.lpstrFile = file;
	of.nMaxFile = static_cast<DWORD>(std::size(file));
	if (!title.empty()) of.lpstrTitle = title.c_str();
	of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	return GetOpenFileNameW(&of) ? std::wstring(file) : std::wstring();
}

std::wstring App::pick_save(const wchar_t *patterns_or_filter, const wchar_t *ext, const std::wstring &name,
                            const std::wstring &title)
{
	wchar_t file[MAX_PATH * 4] = L"";
	wcsncpy_s(file, name.c_str(), _TRUNCATE);
	OPENFILENAMEW of{sizeof of};
	of.hwndOwner = hwnd_;
	of.lpstrFilter = patterns_or_filter;
	of.lpstrFile = file;
	of.nMaxFile = static_cast<DWORD>(std::size(file));
	of.lpstrDefExt = ext;
	if (!title.empty()) of.lpstrTitle = title.c_str();
	of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	return GetSaveFileNameW(&of) ? std::wstring(file) : std::wstring();
}


void App::set_timer(UINT_PTR id, UINT ms) { SetTimer(hwnd_, id, ms, nullptr); }

void App::kill_timer(UINT_PTR id) { KillTimer(hwnd_, id); }

void App::post(UINT msg) { PostMessageW(hwnd_, msg, 0, 0); }

bool App::window_enabled() const { return IsWindowEnabled(hwnd_) != 0; }

}  // namespace ng
