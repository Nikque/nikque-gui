// The text box of the row of the moves on Windows: an EDIT control (src/app_record.cpp has what is shared).
#include <cmath>

#include "app.h"
#include "lang.h"

#include <commctrl.h>

namespace ng {

namespace {

COLORREF color_ref(const Color &c)
{
	auto b = [](float v) { return static_cast<BYTE>(std::lround(v * 255)); };
	return RGB(b(c.r), b(c.g), b(c.b));
}

LRESULT CALLBACK edit_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	App *app = reinterpret_cast<App *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	switch (msg) {
	case WM_PASTE:  // Ctrl+V and the menu of the right button
		app->record_take(narrow(app->clipboard_text()));
		return 0;
	case WM_KEYDOWN:
		if (wp == VK_RETURN || wp == VK_ESCAPE) {
			app->record_key(wp);
			return 0;
		}
		break;
	case WM_CHAR:
		if (wp == L'\r' || wp == VK_ESCAPE) return 0;  // (the box would beep)
		break;
	case WM_SETFOCUS:
		app->record_resume_ = false;
		break;
	case WM_KILLFOCUS: {
		const LRESULT r = CallWindowProcW(app->record_edit_proc_, hwnd, msg, wp, lp);
		app->record_focus_lost(reinterpret_cast<HWND>(wp));
		return r;
	}
	}
	return CallWindowProcW(app->record_edit_proc_, hwnd, msg, wp, lp);
}

}  // namespace

// the last font and brush of the text box (the ones before them are deleted when they are made again)
App::~App()
{
	if (record_font_) DeleteObject(record_font_);
	if (record_brush_) DeleteObject(record_brush_);
}

void App::create_record_box()
{
	record_edit_ = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd_, nullptr, inst_, nullptr);
	if (!record_edit_) return;
	SetWindowLongPtrW(record_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	record_edit_proc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(record_edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(edit_proc)));
}

void App::place_record_box()
{
	if (!record_edit_) return;
	const Layout &L = lay_;
	const RectF &T = L.record_text;
	if (T.right <= T.left) {
		// (the keys in another window meanwhile: what was typed stays in the box, which would not follow the game when
		// shown again; it is taken back, and the keys do not come back to it)
		leave_record_box();
		if (SendMessageW(record_edit_, EM_GETMODIFY, 0, 0)) sync_record_box(true);
		ShowWindow(record_edit_, SW_HIDE);
		return;
	}
	const int px = static_cast<int>(L.font_s);
	if (px != record_font_px_) {
		const HFONT old = record_font_;
		record_font_ = CreateFontW(-px, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		                           CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		record_font_px_ = px;
		SendMessageW(record_edit_, WM_SETFONT, reinterpret_cast<WPARAM>(record_font_), FALSE);
		if (old) DeleteObject(old);
	}
	const int pad = static_cast<int>(std::lround(6 * L.scale));
	const int h = static_cast<int>(std::lround(px * 1.4f));
	const int y = static_cast<int>(std::lround((T.top + T.bottom - h) / 2));
	SetWindowPos(record_edit_, HWND_TOP, static_cast<int>(T.left) + pad, y, static_cast<int>(T.right - T.left) - 2 * pad, h,
	             SWP_NOACTIVATE | SWP_SHOWWINDOW);
	SendMessageW(record_edit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(lang::t("record.hint").c_str()));
	InvalidateRect(record_edit_, nullptr, TRUE);  // (the design may have changed)
}

void App::sync_record_box(bool force)
{
	if (!record_edit_) return;
	if (editing_) leave_record_box();
	EnableWindow(record_edit_, !editing_);
	if (!force && SendMessageW(record_edit_, EM_GETMODIFY, 0, 0)) return;
	std::wstring text = record_text();
	if (!force && text == record_shown_) return;
	SetWindowTextW(record_edit_, text.c_str());
	SendMessageW(record_edit_, EM_SETMODIFY, FALSE, 0);
	record_shown_ = std::move(text);
	// the last moves are the ones in sight
	const WPARAM end = record_shown_.size();
	SendMessageW(record_edit_, EM_SETSEL, end, end);
	SendMessageW(record_edit_, EM_SCROLLCARET, 0, 0);
}

void App::leave_record_box()
{
	record_resume_ = false;
	if (record_edit_ && GetFocus() == record_edit_) SetFocus(hwnd_);
}

// To the window itself (a click on the board, Esc, Enter): what was typed is taken back. To another window (another
// program by Alt+Tab, a message box or a small window of this one: `to` is null or not in this window), it stays, and
// the keys come back to the box when the window has them again, as a dialog gives them back to its field.
void App::record_focus_lost(HWND to)
{
	if (!to || (to != hwnd_ && !IsChild(hwnd_, to))) {
		record_resume_ = true;
		return;
	}
	sync_record_box(true);
}

bool App::record_resumes()
{
	const bool r = record_resume_;
	record_resume_ = false;
	return r && record_edit_ && (GetWindowLongPtrW(record_edit_, GWL_STYLE) & WS_VISIBLE) && IsWindowEnabled(record_edit_);
}

void App::record_key(WPARAM vk)
{
	if (vk == VK_RETURN && SendMessageW(record_edit_, EM_GETMODIFY, 0, 0)) {
		const int n = GetWindowTextLengthW(record_edit_);
		std::wstring s(static_cast<size_t>(n) + 1, L'\0');
		GetWindowTextW(record_edit_, s.data(), n + 1);
		s.resize(static_cast<size_t>(n));
		record_take(narrow(s));
		return;
	}
	sync_record_box(true);
	leave_record_box();
}

LRESULT App::record_color(HDC dc)
{
	const Theme &th = theme(design_);
	const COLORREF bg = color_ref(th.panel2);
	if (!record_brush_ || record_brush_color_ != bg) {
		if (record_brush_) DeleteObject(record_brush_);
		record_brush_ = CreateSolidBrush(bg);
		record_brush_color_ = bg;
	}
	SetTextColor(dc, color_ref(th.text));
	SetBkColor(dc, bg);
	return reinterpret_cast<LRESULT>(record_brush_);
}

}  // namespace ng
