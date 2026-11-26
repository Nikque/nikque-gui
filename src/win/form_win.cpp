// run_form on Windows: a window of Win32 controls made from code (src/form.cpp has the fields).
#include "form.h"

#include "lang.h"

#include <commctrl.h>

#include <algorithm>
#include <cwchar>

namespace ng {

namespace {

constexpr int kIdOk = 1, kIdCancel = 2;

struct FormState {
	std::vector<FormField> *fields;
	const std::wstring *bad_number;
	bool done = false, accepted = false;
	int content_h = 0;  // the height of the fields and buttons when the form is lower than them (it scrolls), else 0
	int pos = 0;        // how far it is scrolled down
	int line = 1;       // a step of the scroll bar's arrows and of the wheel
};

// the form scrolled so that the part from `to` (of its content) is at its top
void scroll_form(HWND hwnd, FormState &st, int to)
{
	RECT rc;
	GetClientRect(hwnd, &rc);
	to = std::clamp(to, 0, std::max(0, st.content_h - static_cast<int>(rc.bottom)));
	if (to == st.pos) return;
	ScrollWindowEx(hwnd, 0, st.pos - to, nullptr, nullptr, nullptr, nullptr, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
	st.pos = to;
	SCROLLINFO si{sizeof si, SIF_POS};
	si.nPos = to;
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

// a control given the focus (Tab, a number refused) is scrolled into the form
void show_control(HWND hwnd, FormState &st, HWND c)
{
	RECT r, rc;
	GetWindowRect(c, &r);
	MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT *>(&r), 2);
	GetClientRect(hwnd, &rc);
	if (r.top < 0) scroll_form(hwnd, st, st.pos + r.top - st.line / 2);
	else if (r.bottom > rc.bottom) scroll_form(hwnd, st, st.pos + r.bottom - rc.bottom + st.line / 2);
}

bool read_fields(HWND hwnd, FormState &st)
{
	for (auto &f : *st.fields) {
		if (!f.control) continue;
		if (f.kind == FormField::Number || f.kind == FormField::Text || f.kind == FormField::Slider) {
			const int n = GetWindowTextLengthW(f.control);
			std::wstring s(n, L'\0');
			GetWindowTextW(f.control, s.data(), n + 1);
			if (f.kind != FormField::Text) {
				wchar_t *end = nullptr;
				const long v = std::wcstol(s.c_str(), &end, 10);
				if (s.empty() || *end || v < f.lo || v > f.hi) {
					wchar_t msg[512];
					swprintf(msg, 512, L"%s\n%s: %d .. %d", st.bad_number->c_str(), f.label.c_str(), f.lo, f.hi);
					MessageBoxW(hwnd, msg, f.label.c_str(), MB_ICONWARNING);
					SetFocus(f.control);
					return false;
				}
			}
			f.text = s;
		} else if (f.kind == FormField::Choice) {
			f.choice = static_cast<int>(SendMessageW(f.control, CB_GETCURSEL, 0, 0));
		} else if (f.kind == FormField::Check) {
			f.checked = SendMessageW(f.control, BM_GETCHECK, 0, 0) == BST_CHECKED;
		}
	}
	return true;
}

LRESULT CALLBACK form_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	auto *st = reinterpret_cast<FormState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	switch (msg) {
	case WM_HSCROLL:  // a slider moved: its number follows
		if (!st) break;
		for (auto &f : *st->fields)
			if (f.slider && reinterpret_cast<HWND>(lp) == f.slider) {
				const int v = static_cast<int>(SendMessageW(f.slider, TBM_GETPOS, 0, 0));
				SetWindowTextW(f.control, std::to_wstring(v).c_str());
			}
		return 0;
	case WM_COMMAND:
		if (!st) break;
		if (HIWORD(wp) == EN_CHANGE) {  // a number typed: its slider follows
			for (auto &f : *st->fields)
				if (f.slider && reinterpret_cast<HWND>(lp) == f.control) {
					wchar_t b[16] = {};
					GetWindowTextW(f.control, b, 16);
					wchar_t *end = nullptr;
					const long v = std::wcstol(b, &end, 10);
					if (*b && !*end && v >= f.lo && v <= f.hi) SendMessageW(f.slider, TBM_SETPOS, TRUE, v);
				}
			return 0;
		}
		if (LOWORD(wp) == kIdOk) {
			if (read_fields(hwnd, *st)) {
				st->accepted = true;
				st->done = true;
			}
			return 0;
		}
		if (LOWORD(wp) == kIdCancel) {
			st->done = true;
			return 0;
		}
		break;
	case WM_CLOSE:
		if (st) st->done = true;
		return 0;
	case WM_VSCROLL: {  // the scroll bar of a form higher than the screen
		if (!st || !st->content_h || lp) break;
		RECT rc;
		GetClientRect(hwnd, &rc);
		int to = st->pos;
		switch (LOWORD(wp)) {
		case SB_LINEUP: to -= st->line; break;
		case SB_LINEDOWN: to += st->line; break;
		case SB_PAGEUP: to -= rc.bottom; break;
		case SB_PAGEDOWN: to += rc.bottom; break;
		case SB_TOP: to = 0; break;
		case SB_BOTTOM: to = st->content_h; break;
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION: {
			SCROLLINFO si{sizeof si, SIF_TRACKPOS};
			GetScrollInfo(hwnd, SB_VERT, &si);
			to = si.nTrackPos;
			break;
		}
		default: break;
		}
		scroll_form(hwnd, *st, to);
		return 0;
	}
	case WM_MOUSEWHEEL:
		if (!st || !st->content_h) break;
		scroll_form(hwnd, *st, st->pos - GET_WHEEL_DELTA_WPARAM(wp) * 3 * st->line / WHEEL_DELTA);
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool run_form(HWND owner, const std::wstring &title, std::vector<FormField> &fields)
{
	const std::wstring &ok = lang::t("form.ok"), &cancel = lang::t("form.cancel"), &bad_number = lang::t("form.bad_number");
	static bool registered = false;
	HINSTANCE inst = GetModuleHandleW(nullptr);
	if (!registered) {
		WNDCLASSEXW wc{sizeof wc};
		wc.lpfnWndProc = form_proc;
		wc.hInstance = inst;
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
		wc.lpszClassName = L"NikqueGuiForm";
		RegisterClassExW(&wc);
		registered = true;
	}
	{
		INITCOMMONCONTROLSEX icc{sizeof icc, ICC_BAR_CLASSES};
		InitCommonControlsEx(&icc);  // (the sliders)
	}
	const UINT dpi = GetDpiForWindow(owner);
	auto px = [&](int v) { return MulDiv(v, static_cast<int>(dpi), 96); };
	HFONT font = CreateFontW(-px(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, lang::ui_font());
	HFONT bold = CreateFontW(-px(14), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, lang::ui_font());

	MONITORINFO mi{sizeof mi};
	const bool screen = GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &mi) != 0;
	const RECT work = mi.rcWork;
	RECT frame{};
	AdjustWindowRectExForDpi(&frame, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VSCROLL, FALSE, WS_EX_DLGMODALFRAME, dpi);
	const int nominal = px(14) * 2 + px(230) + px(150) + px(10) + px(360);
	const int width = screen ? std::clamp<int>(work.right - work.left - (frame.right - frame.left), 1, nominal)
	                         : nominal;
	const int row = px(30), pad = std::min(px(14), width / 20);
	const int columns = std::max(3, width - pad * 2 - px(10));
	const int label_w = width == nominal ? px(230) : std::min(px(230), columns * 3 / 10);
	const int field_w = std::min(px(150), columns * 3 / 10);
	const int note_w = columns - label_w - field_w;
	// a long note is folded in its column: its row is as high as the folded text
	std::vector<int> rows;
	int rows_h = 0;
	{
		HDC sdc = GetDC(nullptr);
		HGDIOBJ old = SelectObject(sdc, font);
		for (const auto &f : fields) {
			int r = f.kind == FormField::Slider ? row + px(8) : row;
			if (f.kind == FormField::Slider && !f.note.empty()) {
				RECT rc{0, 0, field_w + px(10) + note_w, 0};
				DrawTextW(sdc, f.note.c_str(), -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
				r += static_cast<int>(rc.bottom) + px(4);
			} else if (f.kind != FormField::Heading && !f.note.empty()) {
				RECT rc{0, 0, note_w, 0};
				DrawTextW(sdc, f.note.c_str(), -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
				r = std::max(row, static_cast<int>(rc.bottom) + px(10));
			}
			rows.push_back(r);
			rows_h += r;
		}
		SelectObject(sdc, old);
		ReleaseDC(nullptr, sdc);
	}
	const int height = pad * 2 + rows_h + px(46);
	RECT wr{0, 0, width, height};
	DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
	AdjustWindowRectExForDpi(&wr, style, FALSE, WS_EX_DLGMODALFRAME, dpi);
	RECT orc;
	GetWindowRect(owner, &orc);
	int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
	// on the screen of the main window: a form higher than it is as high as the screen and scrolls (the fields and the
	// buttons are reached with its scroll bar, the wheel and Tab)
	const bool scrolls = screen && wh > work.bottom - work.top;
	if (scrolls) {
		style |= WS_VSCROLL;
		ww += GetSystemMetricsForDpi(SM_CXVSCROLL, dpi);
		wh = work.bottom - work.top;
	}
	int x = orc.left + ((orc.right - orc.left) - ww) / 2, y = orc.top + ((orc.bottom - orc.top) - wh) / 3;
	if (screen) {
		x = std::max<int>(work.left, std::min<int>(x, work.right - ww));
		y = std::max<int>(work.top, std::min<int>(y, work.bottom - wh));
	}

	FormState st{&fields, &bad_number};
	HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"NikqueGuiForm", title.c_str(), style, x, y, ww, wh, owner,
	                            nullptr, inst, nullptr);
	if (!hwnd) {  // (not made: the main window is left as it was, and the form is as cancelled)
		DeleteObject(font);
		DeleteObject(bold);
		return false;
	}
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
	if (scrolls) {
		st.content_h = height;
		st.line = row;
		RECT rc;
		GetClientRect(hwnd, &rc);
		SCROLLINFO si{sizeof si, SIF_RANGE | SIF_PAGE | SIF_POS};
		si.nMax = height - 1;
		si.nPage = static_cast<UINT>(rc.bottom);
		SetScrollInfo(hwnd, SB_VERT, &si, FALSE);
	}
	bool child_failed = false;
	auto child = [&](const wchar_t *cls, const std::wstring &text, DWORD s, int cx, int cy, int w, int h, int id = 0) {
		HWND c = CreateWindowExW(0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | s, cx, cy, w, h, hwnd,
		                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
		child_failed = child_failed || !c;
		SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
		return c;
	};
	int cy = pad;
	HWND first = nullptr;
	size_t fi = 0;
	for (auto &f : fields) {
		const int this_row = rows[fi++];
		const int lx = pad, fx = pad + label_w, nx = fx + field_w + px(10);
		const int h = row - px(6);
		if (f.kind == FormField::Heading) {
			HWND c = child(L"STATIC", f.label, SS_LEFT, lx, cy + px(4), width - pad * 2, h);
			SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(bold), FALSE);
			cy += row;
			continue;
		}
		child(L"STATIC", f.label, SS_LEFT, lx, cy + px(4), label_w - px(8), h);
		switch (f.kind) {
		case FormField::Number:
		case FormField::Text:
			f.control = child(L"EDIT", f.text, WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, fx, cy, field_w, h);
			break;
		case FormField::Choice:
			f.control = child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, fx, cy, field_w, px(200));
			for (const auto &c : f.choices) SendMessageW(f.control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(c.c_str()));
			SendMessageW(f.control, CB_SETCURSEL, std::clamp(f.choice, 0, std::max(0, static_cast<int>(f.choices.size()) - 1)), 0);
			break;
		case FormField::Check:
			f.control = child(L"BUTTON", L"", WS_TABSTOP | BS_AUTOCHECKBOX, fx, cy, field_w, h);
			SendMessageW(f.control, BM_SETCHECK, f.checked ? BST_CHECKED : BST_UNCHECKED, 0);
			break;
		case FormField::Slider: {
			// the number, and the slider over the rest of the row (the note goes under it)
			const int nw = std::max(1, std::min(px(56), field_w / 2));
			f.control = child(L"EDIT", f.text, WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, fx, cy, nw, h);
			f.slider = child(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS | TBS_BOTTOM, fx + nw + px(6), cy,
			                 field_w - nw - px(6) + px(10) + note_w, h + px(6));
			SendMessageW(f.slider, TBM_SETRANGEMIN, FALSE, f.lo);
			SendMessageW(f.slider, TBM_SETRANGEMAX, FALSE, f.hi);
			SendMessageW(f.slider, TBM_SETTICFREQ, 5, 0);
			SendMessageW(f.slider, TBM_SETPAGESIZE, 0, 5);
			SendMessageW(f.slider, TBM_SETPOS, TRUE, std::wcstol(f.text.c_str(), nullptr, 10));
			break;
		}
		default:
			break;
		}
		if (!first) first = f.control;
		if (!f.note.empty() && f.kind == FormField::Slider)
			child(L"STATIC", f.note, SS_LEFT | SS_NOPREFIX, fx, cy + row + px(8), field_w + px(10) + note_w, this_row - row - px(8));
		else if (!f.note.empty()) child(L"STATIC", f.note, SS_LEFT | SS_NOPREFIX, nx, cy + px(4), note_w, this_row - px(6));
		cy += this_row;
	}
	const int bw = std::max(1, std::min(px(100), (width - 2 * pad - px(10)) / 2)), bh = px(30);
	child(L"BUTTON", ok, WS_TABSTOP | BS_DEFPUSHBUTTON, width - pad - bw * 2 - px(10), cy + px(8), bw, bh, kIdOk);
	child(L"BUTTON", cancel, WS_TABSTOP, width - pad - bw, cy + px(8), bw, bh, kIdCancel);
	if (child_failed) {
		DestroyWindow(hwnd);
		DeleteObject(font);
		DeleteObject(bold);
		for (auto &f : fields) f.control = f.slider = nullptr;
		return false;
	}

	EnableWindow(owner, FALSE);
	ShowWindow(hwnd, SW_SHOW);
	if (first) SetFocus(first);
	MSG m{};
	// the end of the program asked while the form is open (the main window closed by "quit" of a script, which goes
	// on in this loop): WM_QUIT is posted again after the form, for the loop under it
	bool quit = false;
	HWND focused = nullptr;
	while (!st.done) {
		const BOOL got = GetMessageW(&m, nullptr, 0, 0);
		if (got <= 0) {
			quit = got == 0;  // (-1: no message, as with an error)
			break;
		}
		if (m.message == WM_KEYDOWN && m.wParam == VK_ESCAPE) {
			st.done = true;
			continue;
		}
		if (!IsDialogMessageW(hwnd, &m)) {
			TranslateMessage(&m);
			DispatchMessageW(&m);
		}
		if (st.content_h) {
			HWND f = GetFocus();
			if (f != focused) {
				focused = f;
				if (f && IsChild(hwnd, f)) show_control(hwnd, st, f);
			}
		}
	}
	EnableWindow(owner, TRUE);
	DestroyWindow(hwnd);
	SetForegroundWindow(owner);
	DeleteObject(font);
	DeleteObject(bold);
	for (auto &f : fields) f.control = f.slider = nullptr;
	if (quit) PostQuitMessage(static_cast<int>(m.wParam));
	return st.accepted;
}

}  // namespace ng
