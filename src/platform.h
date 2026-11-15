// What the shared code asks of the system: files, the clipboard, messages to the window from any thread, the keys
// held, and a few more. Windows: src/win/platform_win.cpp; macOS: src/mac/platform_mac.mm.
#pragma once

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include "mac/win_types.h"
#endif

namespace ng {

#if defined(_WIN32)
using NativeWindow = HWND;
using MenuRef = HMENU;
#else
using NativeWindow = void *;  // the view of the window (an NSView)
using MenuRef = void *;       // an NSMenu
#endif

// a name in a folder: `rel` may hold folders separated by '\' (made '/' on macOS)
std::wstring path_join(const std::wstring &dir, std::wstring_view rel);
// the name is a full one (a drive or "\\" on Windows, '/' on macOS)
bool is_full_path(std::wstring_view p);

// a file opened with a mode of fopen ("rb", "wb", "a"; null: it cannot be opened)
FILE *open_file(const std::wstring &path, const wchar_t *mode);
bool delete_file(const std::wstring &path);
// moves a file over another (`copy_allowed`: to another drive by copying)
bool move_file(const std::wstring &from, const std::wstring &to, bool copy_allowed = false);
// copies a file over another
bool copy_file(const std::wstring &from, const std::wstring &to);
bool make_dir(const std::wstring &path);  // false when it is not made (as when it is there)
bool path_exists(const std::wstring &path);  // a file or a folder
bool dir_exists(const std::wstring &path);
bool file_exists(const std::wstring &p);  // a file (not a folder)
std::optional<std::uint64_t> file_size(const std::wstring &path);
// a place in a file over 2 GB (fseek and ftell)
int seek64(FILE *f, long long at, int whence);
long long tell64(FILE *f);
// text in a code page (65001 UTF-8, 932 Shift_JIS); nothing when it is not text of that code page
std::optional<std::wstring> decode_text(std::string_view bytes, unsigned code_page);

// a file read at any place; others may read it, write it, rename it or delete it meanwhile
class RandomFile {
public:
	RandomFile() = default;
	RandomFile(const RandomFile &) = delete;
	RandomFile &operator=(const RandomFile &) = delete;
	~RandomFile() { close(); }
	bool open(const std::wstring &path);
	void close();
	bool is_open() const noexcept;
	std::optional<std::uint64_t> size() const;
	// reads up to `size` bytes at `at` (`got`: how many; 0 at the end); false: it could not be read
	bool read(std::uint64_t at, unsigned char *out, size_t size, size_t *got);

private:
#if defined(_WIN32)
	HANDLE h_ = INVALID_HANDLE_VALUE;
#else
	int fd_ = -1;
#endif
};

// the same file by name (the case of the letters ignored on Windows)
bool same_file(const std::wstring &a, const std::wstring &b);

// the program file, the folder for temporary files (with its separator at the end), the number of this process
std::wstring exe_path();
std::wstring temp_folder();
unsigned process_id();
// the names of the engine programs this computer runs, the fastest first (Windows: by the instructions of the CPU)
std::vector<std::wstring> engine_exe_names();
// the local time of `t`
inline void local_tm(std::time_t t, std::tm *out)
{
#if defined(_WIN32)
	localtime_s(out, &t);
#else
	localtime_r(&t, out);
#endif
}
// the logical processors of the computer
int cpu_count();
void sleep_ms(unsigned ms);

// a message to the window (WM_APP + n), from any thread; it is handled by the window when it gets to it
void post_message(NativeWindow w, unsigned msg, std::uintptr_t wp = 0, std::intptr_t lp = 0);
// the key is held now (VK_CONTROL, VK_SHIFT, VK_MENU)
bool key_held(int vk);

// the clipboard as text (false: it could not be set; empty: no text)
bool set_clipboard_text(NativeWindow w, const std::wstring &text);
std::wstring clipboard_text(NativeWindow w);

// a message box (MB_... flags; it answers IDOK, IDYES, ...)
int message_box(NativeWindow w, const std::wstring &text, const std::wstring &title, unsigned flags);

// the size of a point of the screen in pixels of the drawing (Windows: the DPI / 96; macOS: 1, the drawing is in points)
float window_scale(NativeWindow w);
bool window_minimized(NativeWindow w);
// the window is drawn again soon (App::paint)
void invalidate_window(NativeWindow w);
void destroy_window(NativeWindow w);
// where the window is when it is not maximized or minimized, and whether it is maximized (false: not known)
bool window_placement(NativeWindow w, int *x, int *y, int *width, int *height, bool *maximized);
// the title of the window
void set_window_title(NativeWindow w, const std::wstring &title);
// the mouse leaving the window comes as a move to (-1, -1) (WM_MOUSELEAVE on Windows)
void track_mouse_leave(NativeWindow w);

// Menus: the bar of the window and the small menus in it. An item is a command (id, its words, after a tab the keys
// that do it too), a small menu or a line between items. A command of the menus comes to App::on_command.
MenuRef menu_new(bool bar = false);
void menu_item(MenuRef m, int id, const std::wstring &text, bool grayed = false);
void menu_sub(MenuRef parent, MenuRef m, const std::wstring &text);
void menu_sep(MenuRef m);
// the bar of the window (the one before is destroyed); null when it has none
void menu_set_bar(NativeWindow w, MenuRef bar);
MenuRef menu_bar(NativeWindow w);
void menu_check(MenuRef m, int id, bool on);
// one item of the commands first..last checked as a choice
void menu_radio(MenuRef m, int first, int last, int on);
void menu_enable(MenuRef m, int id, bool on);
// a small menu as a whole
void menu_enable_sub(MenuRef parent, MenuRef child, bool on);
void menu_rename(MenuRef m, int id, const std::wstring &text);
// shows a small menu at (x, y) of the window and returns the command chosen (0: none); the menu is destroyed after
int popup_menu(NativeWindow w, int x, int y, MenuRef m);

// opens a text file in the text editor of the system
void open_text_file(NativeWindow w, const std::wstring &path);

}  // namespace ng
