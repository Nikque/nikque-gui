// The row of the moves as text, right above the graph: a text box with the moves up to the position shown, and the
// buttons to copy and to paste. Pasted text loads at once, typed text with Enter; Esc or leaving the box takes it back.
#include <cmath>

#include "app.h"
#include "lang.h"

#include <commctrl.h>

namespace ng {

namespace {

COLORREF color_ref(const D2D1_COLOR_F &c)
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
	case WM_KILLFOCUS: {
		const LRESULT r = CallWindowProcW(app->record_edit_proc_, hwnd, msg, wp, lp);
		app->sync_record_box(true);
		return r;
	}
	}
	return CallWindowProcW(app->record_edit_proc_, hwnd, msg, wp, lp);
}

}  // namespace

std::wstring App::record_text() const
{
	std::wstring s = widen(game_.to_text(true));
	if (upper_moves_)
		for (auto &c : s)
			if (c >= L'a' && c <= L'z') c = static_cast<wchar_t>(c - L'a' + L'A');
	return s;
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
	const D2D1_RECT_F &T = L.record_text;
	if (T.right <= T.left) {
		if (GetFocus() == record_edit_) SetFocus(hwnd_);
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
	if (editing_ && GetFocus() == record_edit_) SetFocus(hwnd_);
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

bool App::record_take(const std::string &u)
{
	if (editing_ || !paste_text(u)) return false;
	sync_record_box(true);
	if (record_edit_ && GetFocus() == record_edit_) SetFocus(hwnd_);
	return true;
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
	if (GetFocus() == record_edit_) SetFocus(hwnd_);
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
