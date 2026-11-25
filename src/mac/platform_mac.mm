// src/platform.h on macOS (Cocoa). The window is an NgView (src/mac/app_mac.mm); NativeWindow is that view.
#import <Cocoa/Cocoa.h>

#include "../platform.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <system_error>

#include "../ini.h"
#include "../menu_text.h"
#include "mac_view.h"

namespace {

int g_modal = 0;

}  // namespace

void ng_modal_begin() { ++g_modal; }
void ng_modal_end() { --g_modal; }
bool ng_modal_open() { return g_modal > 0; }

namespace ng {

namespace {

std::string u8(const std::wstring &w) { return narrow(w); }

NSString *ns(const std::wstring &w) { return [NSString stringWithUTF8String:narrow(w).c_str()] ?: @""; }

std::wstring from_ns(NSString *s) { return s ? widen(std::string_view(s.UTF8String ?: "")) : std::wstring(); }

}  // namespace

// ---- names and files

std::wstring path_join(const std::wstring &dir, std::wstring_view rel)
{
	std::wstring r(rel);
	for (auto &c : r)
		if (c == L'\\') c = L'/';
	if (!dir.empty() && dir.back() == L'/') return dir + r;
	return dir + L"/" + r;
}

bool is_full_path(std::wstring_view p) { return !p.empty() && p[0] == L'/'; }

FILE *open_file(const std::wstring &path, const wchar_t *mode)
{
	std::string m;
	for (const wchar_t *p = mode; *p; ++p) m += static_cast<char>(*p);
	return std::fopen(u8(path).c_str(), m.c_str());
}

bool delete_file(const std::wstring &path) { return ::unlink(u8(path).c_str()) == 0; }

bool move_file(const std::wstring &from, const std::wstring &to, bool copy_allowed)
{
	const std::string a = u8(from), b = u8(to);
	if (::rename(a.c_str(), b.c_str()) == 0) return true;
	if (errno != EXDEV || !copy_allowed) return false;
	// another file system: copied next to `to` first, and the copy renamed over it (`to` is never left half written).
	// false when `from` cannot be taken away after: `to` then holds the copy, and `from` stays too.
	const std::string part = b + ".part" + std::to_string(::getpid());
	std::error_code ec;
	std::filesystem::copy_file(a, part, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec || ::rename(part.c_str(), b.c_str()) != 0) {
		::unlink(part.c_str());
		return false;
	}
	return ::unlink(a.c_str()) == 0;
}

bool copy_file(const std::wstring &from, const std::wstring &to)
{
	std::error_code ec;
	std::filesystem::copy_file(u8(from), u8(to), std::filesystem::copy_options::overwrite_existing, ec);
	return !ec;
}

bool make_dir(const std::wstring &path) { return ::mkdir(u8(path).c_str(), 0755) == 0; }

bool path_exists(const std::wstring &path)
{
	struct stat st;
	return ::stat(u8(path).c_str(), &st) == 0;
}

bool dir_exists(const std::wstring &path)
{
	struct stat st;
	return ::stat(u8(path).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool file_exists(const std::wstring &path)
{
	struct stat st;
	return ::stat(u8(path).c_str(), &st) == 0 && !S_ISDIR(st.st_mode);
}

std::optional<std::uint64_t> file_size(const std::wstring &path)
{
	struct stat st;
	if (::stat(u8(path).c_str(), &st) != 0) return std::nullopt;
	return static_cast<std::uint64_t>(st.st_size);
}

int seek64(FILE *f, long long at, int whence) { return ::fseeko(f, static_cast<off_t>(at), whence); }

long long tell64(FILE *f) { return static_cast<long long>(::ftello(f)); }

std::optional<std::wstring> decode_text(std::string_view bytes, unsigned code_page)
{
	if (code_page == 65001) {
		// strict UTF-8: no byte that does not make a letter
		const std::wstring w = widen(bytes);
		if (w.find(static_cast<wchar_t>(0xfffd)) != std::wstring::npos && narrow(w) != bytes) return std::nullopt;
		return w;
	}
	const CFStringEncoding enc = code_page == 932 ? kCFStringEncodingDOSJapanese
	                                              : CFStringConvertWindowsCodepageToEncoding(code_page);
	if (enc == kCFStringEncodingInvalidId) return std::nullopt;
	CFStringRef s = CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8 *>(bytes.data()),
	                                        static_cast<CFIndex>(bytes.size()), enc, false);
	if (!s) return std::nullopt;
	std::wstring w = from_ns((__bridge NSString *)s);
	CFRelease(s);
	return w;
}

bool RandomFile::open(const std::wstring &path)
{
	close();
	fd_ = ::open(u8(path).c_str(), O_RDONLY);
	return fd_ >= 0;
}

void RandomFile::close()
{
	if (fd_ >= 0) ::close(fd_);
	fd_ = -1;
}

bool RandomFile::is_open() const noexcept { return fd_ >= 0; }

std::optional<std::uint64_t> RandomFile::size() const
{
	struct stat st;
	if (fd_ < 0 || ::fstat(fd_, &st) != 0) return std::nullopt;
	return static_cast<std::uint64_t>(st.st_size);
}

bool RandomFile::read(std::uint64_t at, unsigned char *out, size_t size, size_t *got)
{
	size_t done = 0;
	while (done < size) {
		const ssize_t n = ::pread(fd_, out + done, size - done, static_cast<off_t>(at + done));
		if (n < 0) {
			if (errno == EINTR) continue;
			return false;
		}
		if (n == 0) break;
		done += static_cast<size_t>(n);
	}
	*got = done;
	return true;
}

bool same_file(const std::wstring &a, const std::wstring &b)
{
	std::error_code ec;
	if (std::filesystem::equivalent(u8(a), u8(b), ec) && !ec) return true;
	return [ns(a) caseInsensitiveCompare:ns(b)] == NSOrderedSame;
}

// ---- the program

// The folder of the program is the one that holds the app (as on Windows the exe is next to its engines, its
// language files and config.ini): ".../nikque GUI.app" is given as the program, so its folder is that folder.
std::wstring exe_path() { return from_ns(NSBundle.mainBundle.bundlePath); }

std::wstring temp_folder()
{
	std::wstring t = from_ns(NSTemporaryDirectory());
	if (t.empty() || t.back() != L'/') t += L'/';
	return t;
}

unsigned process_id() { return static_cast<unsigned>(::getpid()); }

std::vector<std::wstring> engine_exe_names()
{
#if defined(__aarch64__)
	return {L"mEdax-arm64", L"mEdax-x64-modern"};
#else
	// (the program for Intel on a Mac with Apple silicon, under Rosetta: the engine for Apple silicon first)
	int arm = 0;
	size_t len = sizeof arm;
	if (::sysctlbyname("hw.optional.arm64", &arm, &len, nullptr, 0) == 0 && arm) return {L"mEdax-arm64", L"mEdax-x64-modern"};
	return {L"mEdax-x64-modern"};
#endif
}

int cpu_count()
{
	const long n = ::sysconf(_SC_NPROCESSORS_ONLN);
	return n > 0 ? static_cast<int>(n) : 1;
}

void sleep_ms(unsigned ms) { ::usleep(ms * 1000); }

// ---- the window

void post_message(NativeWindow w, unsigned msg, std::uintptr_t wp, std::intptr_t lp)
{
	if (!w) return;
	NgView *view = (__bridge NgView *)w;
	__weak NgView *weak = view;
	dispatch_async(dispatch_get_main_queue(), ^{
		NgView *v = weak;
		if (v) [v handleMessage:msg wParam:wp lParam:lp];
	});
}

bool key_held(int vk)
{
	const NSEventModifierFlags f = NSEvent.modifierFlags;
	switch (vk) {
	case VK_CONTROL: return (f & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) != 0;
	case VK_SHIFT: return (f & NSEventModifierFlagShift) != 0;
	case VK_MENU: return (f & NSEventModifierFlagOption) != 0;
	default: return false;
	}
}

bool set_clipboard_text(NativeWindow, const std::wstring &text)
{
	NSPasteboard *pb = NSPasteboard.generalPasteboard;
	[pb clearContents];
	return [pb setString:ns(text) forType:NSPasteboardTypeString];
}

std::wstring clipboard_text(NativeWindow)
{
	return from_ns([NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString]);
}

int message_box(NativeWindow w, const std::wstring &text, const std::wstring &title, unsigned flags)
{
	NSAlert *a = [[NSAlert alloc] init];
	a.messageText = ns(title);
	a.informativeText = ns(text);
	const unsigned icon = flags & 0xf0;
	a.alertStyle = icon == MB_ICONERROR || icon == MB_ICONWARNING ? NSAlertStyleWarning : NSAlertStyleInformational;
	const unsigned kind = flags & 0xf;
	std::vector<int> answers;
	auto button = [&](NSString *label, int answer) {
		[a addButtonWithTitle:label];
		answers.push_back(answer);
	};
	// (the words of the buttons are the system's own, as on Windows)
	switch (kind) {
	case MB_OKCANCEL:
		button(@"OK", IDOK);
		button(NSLocalizedString(@"Cancel", nil), IDCANCEL);
		break;
	case MB_YESNO:
		button(NSLocalizedString(@"Yes", nil), IDYES);
		button(NSLocalizedString(@"No", nil), IDNO);
		break;
	case MB_YESNOCANCEL:
		button(NSLocalizedString(@"Yes", nil), IDYES);
		button(NSLocalizedString(@"No", nil), IDNO);
		button(NSLocalizedString(@"Cancel", nil), IDCANCEL);
		break;
	default: button(@"OK", IDOK); break;
	}
	// the default button (MB_DEFBUTTON2: the second)
	const size_t def = (flags & 0xf00) == MB_DEFBUTTON2 ? 1 : (flags & 0xf00) == MB_DEFBUTTON3 ? 2 : 0;
	if (def > 0 && def < answers.size()) {
		a.buttons[0].keyEquivalent = @"";
		a.buttons[def].keyEquivalent = @"\r";
	}
	if (answers.size() > 1) a.buttons[answers.size() - 1].keyEquivalent = def == answers.size() - 1 ? @"\r" : @"\033";
	ng_modal_begin();
	const NSModalResponse r = [a runModal];
	ng_modal_end();
	const long i = r - NSAlertFirstButtonReturn;
	if (i >= 0 && i < static_cast<long>(answers.size())) return answers[static_cast<size_t>(i)];
	return kind == MB_OK ? IDOK : IDCANCEL;
}

float window_scale(NativeWindow) { return 1.0f; }

bool window_minimized(NativeWindow w)
{
	if (!w) return false;
	NgView *v = (__bridge NgView *)w;
	return v.window.isMiniaturized;
}

void invalidate_window(NativeWindow w)
{
	if (!w) return;
	NgView *v = (__bridge NgView *)w;
	v.needsPaint = YES;
	v.needsDisplay = YES;
}

void destroy_window(NativeWindow w)
{
	if (!w) return;
	NgView *v = (__bridge NgView *)w;
	[v closeForGood];
}

bool window_placement(NativeWindow w, int *x, int *y, int *width, int *height, bool *maximized)
{
	if (!w) return false;
	NgView *v = (__bridge NgView *)w;
	NSWindow *win = v.window;
	if (!win) return false;
	// the frame of the window, its top left from the top left of the main screen (as on Windows)
	const NSRect f = win.frame;
	const NSRect screen = NSScreen.screens.firstObject.frame;
	*x = static_cast<int>(f.origin.x);
	*y = static_cast<int>(NSMaxY(screen) - NSMaxY(f));
	*width = static_cast<int>(f.size.width);
	*height = static_cast<int>(f.size.height);
	*maximized = win.isZoomed;
	return true;
}

void set_window_title(NativeWindow w, const std::wstring &title)
{
	if (!w) return;
	NgView *v = (__bridge NgView *)w;
	v.window.title = ns(title);
}

void track_mouse_leave(NativeWindow) {}  // (the view has a tracking area: mouseExited)

void open_text_file(NativeWindow, const std::wstring &path)
{
	NSURL *url = [NSURL fileURLWithPath:ns(path)];
	NSWorkspace *ws = NSWorkspace.sharedWorkspace;
	NSURL *editor = [ws URLForApplicationWithBundleIdentifier:@"com.apple.TextEdit"];
	if (!editor) {
		[ws openURL:url];
		return;
	}
	[ws openURLs:@[url] withApplicationAtURL:editor configuration:[NSWorkspaceOpenConfiguration configuration] completionHandler:nil];
}

// ---- menus: an item keeps its command in its tag; its action goes to the view (NgView -menuCommand:)

MenuRef menu_new(bool)
{
	NSMenu *m = [[NSMenu alloc] initWithTitle:@""];
	m.autoenablesItems = NO;
	return (__bridge_retained void *)m;
}

void menu_item(MenuRef m, int id, const std::wstring &text, bool grayed)
{
	NSMenu *menu = (__bridge NSMenu *)m;
	// (the keys after a tab are for the eye only: the view takes the keys itself)
	std::wstring label = text, keys;
	if (const size_t tab = text.find(L'\t'); tab != std::wstring::npos) {
		label = text.substr(0, tab);
		keys = text.substr(tab + 1);
	}
	NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:ns(label) action:@selector(menuCommand:) keyEquivalent:@""];
	item.tag = id;
	item.enabled = !grayed;
	if (!keys.empty()) item.toolTip = ns(keys);
	[menu addItem:item];
}

void menu_sub(MenuRef parent, MenuRef m, const std::wstring &text)
{
	NSMenu *p = (__bridge NSMenu *)parent;
	NSMenu *sub = (__bridge_transfer NSMenu *)m;  // (the parent keeps it now)
	sub.title = ns(menu_words(text));
	NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:ns(menu_words(text)) action:nil keyEquivalent:@""];
	item.submenu = sub;
	[p addItem:item];
}

void menu_sep(MenuRef m) { [(__bridge NSMenu *)m addItem:[NSMenuItem separatorItem]]; }

void menu_set_bar(NativeWindow w, MenuRef bar)
{
	NSMenu *b = (__bridge_transfer NSMenu *)bar;
	if (w) [(__bridge NgView *)w setMenuBar:b];
}

MenuRef menu_bar(NativeWindow w)
{
	if (!w) return nullptr;
	return (__bridge void *)((__bridge NgView *)w).menuBar;
}

namespace {

NSMenuItem *find_item(NSMenu *m, int id)
{
	for (NSMenuItem *i in m.itemArray) {
		if (i.submenu) {
			if (NSMenuItem *f = find_item(i.submenu, id)) return f;
		} else if (i.tag == id && i.action == @selector(menuCommand:)) {
			return i;
		}
	}
	return nil;
}

}  // namespace

void menu_check(MenuRef m, int id, bool on)
{
	if (NSMenuItem *i = find_item((__bridge NSMenu *)m, id)) i.state = on ? NSControlStateValueOn : NSControlStateValueOff;
}

void menu_radio(MenuRef m, int first, int last, int on)
{
	for (int id = first; id <= last; ++id) menu_check(m, id, id == on);
}

void menu_enable(MenuRef m, int id, bool on)
{
	if (NSMenuItem *i = find_item((__bridge NSMenu *)m, id)) i.enabled = on;
}

void menu_enable_sub(MenuRef parent, MenuRef child, bool on)
{
	if (!parent || !child) return;
	NSMenu *c = (__bridge NSMenu *)child;
	for (NSMenuItem *i in ((__bridge NSMenu *)parent).itemArray)
		if (i.submenu == c) i.enabled = on;
}

void menu_rename(MenuRef m, int id, const std::wstring &text)
{
	if (NSMenuItem *i = find_item((__bridge NSMenu *)m, id)) i.title = ns(text);
}

int popup_menu(NativeWindow w, int x, int y, MenuRef m)
{
	NSMenu *menu = (__bridge_transfer NSMenu *)m;
	if (!w) return 0;
	NgView *v = (__bridge NgView *)w;
	return [v popUpMenu:menu atX:x y:y];
}

}  // namespace ng
