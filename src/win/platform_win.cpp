// src/platform.h on Windows.
#include "../platform.h"

#include <intrin.h>
#include <shellapi.h>

#include <algorithm>
#include <iterator>

namespace ng {

std::wstring path_join(const std::wstring &dir, std::wstring_view rel) { return dir + L"\\" + std::wstring(rel); }

bool is_full_path(std::wstring_view w) { return w.size() >= 2 && (w[1] == L':' || (w[0] == L'\\' && w[1] == L'\\')); }

FILE *open_file(const std::wstring &path, const wchar_t *mode)
{
	FILE *f = nullptr;
	if (_wfopen_s(&f, path.c_str(), mode) != 0) return nullptr;
	return f;
}

bool delete_file(const std::wstring &path) { return DeleteFileW(path.c_str()) != 0; }

bool move_file(const std::wstring &from, const std::wstring &to, bool copy_allowed)
{
	return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | (copy_allowed ? MOVEFILE_COPY_ALLOWED : 0)) != 0;
}

bool copy_file(const std::wstring &from, const std::wstring &to) { return CopyFileW(from.c_str(), to.c_str(), FALSE) != 0; }

bool make_dir(const std::wstring &path) { return CreateDirectoryW(path.c_str(), nullptr) != 0; }

bool path_exists(const std::wstring &path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

bool dir_exists(const std::wstring &p)
{
	const DWORD a = GetFileAttributesW(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool file_exists(const std::wstring &p)
{
	const DWORD a = GetFileAttributesW(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::optional<std::uint64_t> file_size(const std::wstring &path)
{
	WIN32_FILE_ATTRIBUTE_DATA attr{};
	if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attr)) return std::nullopt;
	return std::uint64_t{attr.nFileSizeHigh} << 32 | attr.nFileSizeLow;
}

bool same_file(const std::wstring &a, const std::wstring &b)
{
	return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

int seek64(FILE *f, long long at, int whence) { return _fseeki64(f, at, whence); }

long long tell64(FILE *f) { return _ftelli64(f); }

std::optional<std::wstring> decode_text(std::string_view bytes, unsigned code_page)
{
	const int len = static_cast<int>(bytes.size());
	const int n = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, bytes.data(), len, nullptr, 0);
	if (n <= 0) return std::nullopt;
	std::wstring w(n, L'\0');
	MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, bytes.data(), len, w.data(), n);
	return w;
}

bool RandomFile::open(const std::wstring &path)
{
	close();
	// (shared with the engine, which reads the file and may give it another name; but while this handle is open no file
	// can be put in its place, as the engine saves its book: the app closes it before that)
	h_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
	                 FILE_FLAG_RANDOM_ACCESS, nullptr);
	return h_ != INVALID_HANDLE_VALUE;
}

void RandomFile::close()
{
	if (h_ != INVALID_HANDLE_VALUE) CloseHandle(h_);
	h_ = INVALID_HANDLE_VALUE;
}

bool RandomFile::is_open() const noexcept { return h_ != INVALID_HANDLE_VALUE; }

std::optional<std::uint64_t> RandomFile::size() const
{
	LARGE_INTEGER size{};
	if (!GetFileSizeEx(h_, &size)) return std::nullopt;
	return static_cast<std::uint64_t>(size.QuadPart);
}

bool RandomFile::read(std::uint64_t at, unsigned char *out, size_t size, size_t *got)
{
	OVERLAPPED o{};
	o.Offset = static_cast<DWORD>(at);
	o.OffsetHigh = static_cast<DWORD>(at >> 32);
	DWORD n = 0;
	if (!ReadFile(h_, out, static_cast<DWORD>(size), &n, &o)) {
		if (GetLastError() != ERROR_HANDLE_EOF) return false;
		n = 0;
	}
	*got = n;
	return true;
}

std::wstring exe_path()
{
	wchar_t path[MAX_PATH * 4];
	GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
	return path;
}

std::wstring temp_folder()
{
	wchar_t tmp[MAX_PATH];
	GetTempPathW(MAX_PATH, tmp);
	return tmp;
}

unsigned process_id() { return GetCurrentProcessId(); }

namespace {

// 4: AVX-512 (x86-64-v4), 3: AVX2 (x86-64-v3), 1: other
int cpu_level()
{
	int r[4];
	__cpuid(r, 0);
	const int max_leaf = r[0];
	__cpuid(r, 1);
	const bool osxsave = (r[2] >> 27) & 1, avx = (r[2] >> 28) & 1, fma = (r[2] >> 12) & 1, movbe = (r[2] >> 22) & 1;
	const bool popcnt = (r[2] >> 23) & 1, f16c = (r[2] >> 29) & 1;
	if (!osxsave || !avx || max_leaf < 7) return 1;
	const unsigned long long xcr0 = _xgetbv(0);
	if ((xcr0 & 6) != 6) return 1;
	__cpuidex(r, 7, 0);
	const unsigned b = static_cast<unsigned>(r[1]);
	const bool avx2 = (b >> 5) & 1, bmi1 = (b >> 3) & 1, bmi2 = (b >> 8) & 1;
	int e[4];
	__cpuid(e, static_cast<int>(0x80000001));
	const bool lzcnt = (e[2] >> 5) & 1;
	if (!(avx2 && bmi1 && bmi2 && fma && movbe && popcnt && f16c && lzcnt)) return 1;
	const bool avx512 = ((b >> 16) & 1) && ((b >> 17) & 1) && ((b >> 28) & 1) && ((b >> 30) & 1) && ((b >> 31) & 1);
	if (avx512 && (xcr0 & 0xe6) == 0xe6) return 4;
	return 3;
}

}  // namespace

std::vector<std::wstring> engine_exe_names()
{
	const int lv = cpu_level();
	std::vector<std::wstring> names;
	if (lv >= 4) names.push_back(L"wEdax-x86-64-v4.exe");
	if (lv >= 3) names.push_back(L"wEdax-x86-64-v3.exe");
	names.push_back(L"wEdax-x86-64.exe");
	return names;
}

int cpu_count() { return static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)); }

void sleep_ms(unsigned ms) { Sleep(ms); }

void post_message(NativeWindow w, unsigned msg, std::uintptr_t wp, std::intptr_t lp) { PostMessageW(w, msg, wp, lp); }

bool key_held(int vk) { return GetKeyState(vk) < 0; }

namespace {

// other programs hold the clipboard for a moment, right after it changed for one
bool open_clipboard(HWND hwnd)
{
	for (int i = 0; i < 20; ++i) {
		if (OpenClipboard(hwnd)) return true;
		Sleep(10);
	}
	return false;
}

}  // namespace

bool set_clipboard_text(NativeWindow w, const std::wstring &text)
{
	if (!open_clipboard(w)) return false;
	EmptyClipboard();
	bool ok = false;
	if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t))) {
		if (wchar_t *p = static_cast<wchar_t *>(GlobalLock(h))) {
			std::copy_n(text.c_str(), text.size() + 1, p);
			GlobalUnlock(h);
			ok = SetClipboardData(CF_UNICODETEXT, h) != nullptr;
		}
		if (!ok) GlobalFree(h);  // (the clipboard owns it only when it took it)
	}
	CloseClipboard();
	return ok;
}

std::wstring clipboard_text(NativeWindow w)
{
	std::wstring text;
	if (open_clipboard(w)) {
		if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
			if (const wchar_t *p = static_cast<const wchar_t *>(GlobalLock(h))) text = p;
			GlobalUnlock(h);
		}
		CloseClipboard();
	}
	return text;
}

int message_box(NativeWindow w, const std::wstring &text, const std::wstring &title, unsigned flags)
{
	return MessageBoxW(w, text.c_str(), title.c_str(), flags);
}

float window_scale(NativeWindow w) { return GetDpiForWindow(w) / 96.0f; }

bool window_minimized(NativeWindow w) { return IsIconic(w) != 0; }

void invalidate_window(NativeWindow w) { InvalidateRect(w, nullptr, FALSE); }

void destroy_window(NativeWindow w) { DestroyWindow(w); }

bool window_placement(NativeWindow w, int *x, int *y, int *width, int *height, bool *maximized)
{
	WINDOWPLACEMENT wp{sizeof wp};
	if (!GetWindowPlacement(w, &wp)) return false;
	const RECT &r = wp.rcNormalPosition;
	*x = r.left;
	*y = r.top;
	*width = r.right - r.left;
	*height = r.bottom - r.top;
	*maximized = wp.showCmd == SW_SHOWMAXIMIZED;
	return true;
}

void set_window_title(NativeWindow w, const std::wstring &title) { SetWindowTextW(w, title.c_str()); }

void track_mouse_leave(NativeWindow w)
{
	TRACKMOUSEEVENT tme{sizeof tme, TME_LEAVE, w, 0};
	TrackMouseEvent(&tme);
}

MenuRef menu_new(bool bar) { return bar ? CreateMenu() : CreatePopupMenu(); }

void menu_item(MenuRef m, int id, const std::wstring &text, bool grayed)
{
	AppendMenuW(m, grayed ? MF_STRING | MF_GRAYED : MF_STRING, id, text.c_str());
}

void menu_sub(MenuRef parent, MenuRef m, const std::wstring &text)
{
	AppendMenuW(parent, MF_POPUP, reinterpret_cast<UINT_PTR>(m), text.c_str());
}

void menu_sep(MenuRef m) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }

void menu_set_bar(NativeWindow w, MenuRef bar)
{
	HMENU old = GetMenu(w);
	SetMenu(w, bar);
	if (old) DestroyMenu(old);
}

MenuRef menu_bar(NativeWindow w) { return GetMenu(w); }

void menu_check(MenuRef m, int id, bool on) { CheckMenuItem(m, id, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED)); }

void menu_radio(MenuRef m, int first, int last, int on) { CheckMenuRadioItem(m, first, last, on, MF_BYCOMMAND); }

void menu_enable(MenuRef m, int id, bool on) { EnableMenuItem(m, id, MF_BYCOMMAND | (on ? MF_ENABLED : MF_GRAYED)); }

void menu_enable_sub(MenuRef parent, MenuRef child, bool on)
{
	if (!parent || !child) return;
	for (int i = 0, n = GetMenuItemCount(parent); i < n; ++i)
		if (GetSubMenu(parent, i) == child) EnableMenuItem(parent, static_cast<UINT>(i), MF_BYPOSITION | (on ? MF_ENABLED : MF_GRAYED));
}

void menu_rename(MenuRef m, int id, const std::wstring &text)
{
	ModifyMenuW(m, id, MF_BYCOMMAND | MF_STRING, id, text.c_str());
}

int popup_menu(NativeWindow w, int x, int y, MenuRef m)
{
	POINT pt{x, y};
	ClientToScreen(w, &pt);
	const int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, w, nullptr);
	DestroyMenu(m);
	return id;
}

void open_text_file(NativeWindow w, const std::wstring &path)
{
	ShellExecuteW(w, L"open", L"notepad.exe", (L"\"" + path + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
}

}  // namespace ng
