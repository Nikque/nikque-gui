#include "form.h"

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
};

bool read_fields(HWND hwnd, FormState &st)
{
	for (auto &f : *st.fields) {
		if (!f.control) continue;
		if (f.kind == FormField::Number || f.kind == FormField::Text) {
			const int n = GetWindowTextLengthW(f.control);
			std::wstring s(n, L'\0');
			GetWindowTextW(f.control, s.data(), n + 1);
			if (f.kind == FormField::Number) {
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
	case WM_COMMAND:
		if (!st) break;
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
	}
	return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool run_form(HWND owner, const std::wstring &title, std::vector<FormField> &fields, const std::wstring &ok,
              const std::wstring &cancel, const std::wstring &bad_number)
{
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
	const UINT dpi = GetDpiForWindow(owner);
	auto px = [&](int v) { return MulDiv(v, static_cast<int>(dpi), 96); };
	HFONT font = CreateFontW(-px(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Yu Gothic UI");
	HFONT bold = CreateFontW(-px(14), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Yu Gothic UI");

	const int label_w = px(230), field_w = px(150), note_w = px(260), row = px(30), pad = px(14);
	const int width = pad * 2 + label_w + field_w + px(10) + note_w;
	const int height = pad * 2 + row * static_cast<int>(fields.size()) + px(46);
	RECT wr{0, 0, width, height};
	const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
	AdjustWindowRectExForDpi(&wr, style, FALSE, WS_EX_DLGMODALFRAME, dpi);
	RECT orc;
	GetWindowRect(owner, &orc);
	const int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
	const int x = orc.left + ((orc.right - orc.left) - ww) / 2, y = orc.top + ((orc.bottom - orc.top) - wh) / 3;

	FormState st{&fields, &bad_number};
	HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"NikqueGuiForm", title.c_str(), style, x, y, ww, wh, owner,
	                            nullptr, inst, nullptr);
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
	auto child = [&](const wchar_t *cls, const std::wstring &text, DWORD s, int cx, int cy, int w, int h, int id = 0) {
		HWND c = CreateWindowExW(0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | s, cx, cy, w, h, hwnd,
		                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
		SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
		return c;
	};
	int cy = pad;
	HWND first = nullptr;
	for (auto &f : fields) {
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
		default:
			break;
		}
		if (!first) first = f.control;
		if (!f.note.empty()) child(L"STATIC", f.note, SS_LEFT, nx, cy + px(4), note_w, h);
		cy += row;
	}
	const int bw = px(100), bh = px(30);
	child(L"BUTTON", ok, WS_TABSTOP | BS_DEFPUSHBUTTON, width - pad - bw * 2 - px(10), cy + px(8), bw, bh, kIdOk);
	child(L"BUTTON", cancel, WS_TABSTOP, width - pad - bw, cy + px(8), bw, bh, kIdCancel);

	EnableWindow(owner, FALSE);
	ShowWindow(hwnd, SW_SHOW);
	if (first) SetFocus(first);
	MSG m;
	while (!st.done && GetMessageW(&m, nullptr, 0, 0) > 0) {
		if (m.message == WM_KEYDOWN && m.wParam == VK_ESCAPE) {
			st.done = true;
			continue;
		}
		if (!IsDialogMessageW(hwnd, &m)) {
			TranslateMessage(&m);
			DispatchMessageW(&m);
		}
	}
	EnableWindow(owner, TRUE);
	DestroyWindow(hwnd);
	SetForegroundWindow(owner);
	DeleteObject(font);
	DeleteObject(bold);
	for (auto &f : fields) f.control = nullptr;
	return st.accepted;
}

}  // namespace ng
