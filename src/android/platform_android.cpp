// src/platform.h on Android: what is not in src/posix. The menus are kept here as lists (the screen shows them as
// lists in windows: Bridge.showMenu); the items of what this version does not have on Android (the learning of the
// book, the game lists, the scripts, the commands that ask for a file) are left out.
#include "../platform.h"

#include <algorithm>
#include <mutex>

#include "../app.h"
#include "../ini.h"
#include "../menu_text.h"
#include "../posix/posix.h"
#include "android_bridge.h"

namespace ng {

namespace {

// the local references made by a call go at its end (the thread of the app never goes back to Java)
struct LocalFrame {
	LocalFrame() { jni()->PushLocalFrame(32); }
	~LocalFrame() { jni()->PopLocalFrame(nullptr); }
};

struct Menu;

struct MenuItem {
	int id = 0;
	std::wstring text;
	bool checked = false, grayed = false, sep = false;
	Menu *sub = nullptr;
};

struct Menu {
	std::vector<MenuItem> items;
};

std::mutex g_menu_mutex;  // (the screen reads the menus from its thread)
Menu *g_bar = nullptr;
std::vector<Menu *> g_made, g_before;  // the menus made since the bar was set, and those of the bar before

Menu *M(MenuRef m) { return static_cast<Menu *>(m); }

// the commands this version does not show on Android
bool left_out(int id)
{
	using namespace cmd;
	if (id >= LearnDeviate && id <= LearnJobLast) return true;  // the learning and the keeping of the book, the command box
	if (id >= LearnStop && id <= QueueClear) return true;
	if (id >= RecentBook0 && id < RecentBook0 + 8) return true;
	if (id >= BaseConvert && id <= BaseTofen) return true;
	if (id >= ListOpen && id <= TaskCancel) return true;
	switch (id) {
	case OpenBook:
	case ExportMissing:
	case ExportBest:
	case ExportDeviate:
	case ToolSolve:
	case Screenshot:
	case SaveBoardImage:
	case RunScript:
	case StopScript:
	case SaveLog:
	case OpenConfig: return true;
	default: return false;
	}
}

MenuItem *find_item(Menu *m, int id)
{
	for (auto &i : m->items) {
		if (i.sub) {
			if (MenuItem *f = find_item(i.sub, id)) return f;
		} else if (!i.sep && i.id == id) {
			return &i;
		}
	}
	return nullptr;
}

bool has_command(const Menu *m)
{
	for (const auto &i : m->items)
		if (i.sub ? has_command(i.sub) : (!i.sep && i.id != 0)) return true;
	return false;
}

void add_lines(const Menu *m, int depth, std::vector<std::wstring> *out)
{
	bool sep_before = true;  // (no line at the start, nor two in a row, where items were left out)
	for (const auto &i : m->items) {
		if (i.sep) {
			if (!sep_before) out->push_back(std::to_wstring(depth) + L"\t0\t8\t");
			sep_before = true;
			continue;
		}
		sep_before = false;
		const int flags = (i.checked ? 1 : 0) | (i.grayed ? 2 : 0) | (i.sub ? 4 : 0);
		out->push_back(std::to_wstring(depth) + L"\t" + std::to_wstring(i.id) + L"\t" + std::to_wstring(flags) + L"\t" + i.text);
		if (i.sub) add_lines(i.sub, depth + 1, out);
	}
}

void collect(Menu *m, std::vector<Menu *> *out)
{
	out->push_back(m);
	for (auto &i : m->items)
		if (i.sub) collect(i.sub, out);
}

jobjectArray to_java_array(const std::vector<std::wstring> &lines)
{
	JNIEnv *e = jni();
	const jobjectArray a = e->NewObjectArray(static_cast<jsize>(lines.size()), e->FindClass("java/lang/String"), nullptr);
	for (size_t i = 0; i < lines.size(); ++i) {
		const jstring s = to_java(lines[i]);
		e->SetObjectArrayElement(a, static_cast<jsize>(i), s);
		e->DeleteLocalRef(s);
	}
	return a;
}

}  // namespace

std::vector<std::wstring> menu_lines(void *menu)
{
	std::lock_guard lock(g_menu_mutex);
	std::vector<std::wstring> out;
	const Menu *m = menu ? M(menu) : g_bar;
	if (m) add_lines(m, 0, &out);
	return out;
}

// ---- text of other code pages (the files of games in Shift_JIS)

std::optional<std::wstring> decode_codepage(std::string_view bytes, unsigned code_page)
{
	LocalFrame frame;
	JNIEnv *e = jni();
	static const jmethodID m = bridge_method("decode", "([BLjava/lang/String;)Ljava/lang/String;");
	const jbyteArray b = e->NewByteArray(static_cast<jsize>(bytes.size()));
	if (!b) return std::nullopt;
	e->SetByteArrayRegion(b, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<const jbyte *>(bytes.data()));
	const jstring name = to_java(code_page == 932 ? std::wstring(L"windows-31j") : L"windows-" + std::to_wstring(code_page));
	const jstring s = static_cast<jstring>(e->CallStaticObjectMethod(bridge_class(), m, b, name));
	if (!s) return std::nullopt;
	return from_java(s);
}

// ---- the program

std::wstring exe_path() { return android_state().app_dir; }

std::wstring temp_folder() { return android_state().temp_dir; }

// (a link in the folder of the engine to its program among the native libraries of the app: MainActivity)
std::vector<std::wstring> engine_exe_names() { return {L"aEdax"}; }

// ---- the window

void post_message(NativeWindow w, unsigned msg, std::uintptr_t wp, std::intptr_t lp)
{
	if (!w) return;
	push_event(AndroidEvent{kEvMessage, static_cast<std::intptr_t>(msg), static_cast<std::intptr_t>(wp), lp});
}

bool key_held(int vk)
{
	const AndroidState &s = android_state();
	return vk == VK_CONTROL ? s.ctrl : vk == VK_SHIFT ? s.shift : false;
}

bool set_clipboard_text(NativeWindow, const std::wstring &text)
{
	LocalFrame frame;
	static const jmethodID m = bridge_method("clipboardSet", "(Ljava/lang/String;)Z");
	return jni()->CallStaticBooleanMethod(bridge_class(), m, to_java(text)) != JNI_FALSE;
}

std::wstring clipboard_text(NativeWindow)
{
	LocalFrame frame;
	static const jmethodID m = bridge_method("clipboardGet", "()Ljava/lang/String;");
	return from_java(static_cast<jstring>(jni()->CallStaticObjectMethod(bridge_class(), m)));
}

int message_box(NativeWindow, const std::wstring &text, const std::wstring &title, unsigned flags)
{
	LocalFrame frame;
	static const jmethodID m = bridge_method("messageBox", "(Ljava/lang/String;Ljava/lang/String;I)I");
	AndroidState &s = android_state();
	++s.modal;
	const int r = jni()->CallStaticIntMethod(bridge_class(), m, to_java(text), to_java(title), static_cast<jint>(flags & 0xf));
	--s.modal;
	return r;
}

// (the drawing is in the pixels of the screen; the sizes of the layout are in its points)
float window_scale(NativeWindow) { return android_state().density; }

bool window_minimized(NativeWindow)
{
	const AndroidState &s = android_state();
	return !s.active || !s.surface;
}

void invalidate_window(NativeWindow w)
{
	if (w) android_state().needs_paint = true;
}

void destroy_window(NativeWindow w)
{
	if (w) android_state().quit = true;
}

bool window_placement(NativeWindow, int *, int *, int *, int *, bool *) { return false; }

void set_window_title(NativeWindow, const std::wstring &) {}

void track_mouse_leave(NativeWindow) {}

void open_text_file(NativeWindow, const std::wstring &) {}

// ---- menus

MenuRef menu_new(bool)
{
	std::lock_guard lock(g_menu_mutex);
	Menu *m = new Menu;
	g_made.push_back(m);
	return m;
}

void menu_item(MenuRef m, int id, const std::wstring &text, bool grayed)
{
	if (left_out(id)) return;
	std::lock_guard lock(g_menu_mutex);
	MenuItem i;
	i.id = id;
	// (the keys after a tab are not shown: the screen has no keyboard)
	i.text = text.substr(0, text.find(L'\t'));
	i.grayed = grayed;
	M(m)->items.push_back(std::move(i));
}

void menu_sub(MenuRef parent, MenuRef m, const std::wstring &text)
{
	std::lock_guard lock(g_menu_mutex);
	if (!has_command(M(m))) return;  // (all its items were left out: it stays kept, not shown)
	MenuItem i;
	i.text = menu_words(text);
	i.sub = M(m);
	M(parent)->items.push_back(std::move(i));
}

void menu_sep(MenuRef m)
{
	std::lock_guard lock(g_menu_mutex);
	MenuItem i;
	i.sep = true;
	M(m)->items.push_back(std::move(i));
}

void menu_set_bar(NativeWindow, MenuRef bar)
{
	std::lock_guard lock(g_menu_mutex);
	// (the menus of the bar before go; the App keeps some of the new ones by their address until the next bar)
	for (Menu *m : g_before) delete m;
	g_before = std::move(g_made);
	g_made.clear();
	g_bar = M(bar);
}

MenuRef menu_bar(NativeWindow) { return g_bar; }

void menu_check(MenuRef m, int id, bool on)
{
	if (!m) return;
	std::lock_guard lock(g_menu_mutex);
	if (MenuItem *i = find_item(M(m), id)) i->checked = on;
}

void menu_radio(MenuRef m, int first, int last, int on)
{
	for (int id = first; id <= last; ++id) menu_check(m, id, id == on);
}

void menu_enable(MenuRef m, int id, bool on)
{
	if (!m) return;
	std::lock_guard lock(g_menu_mutex);
	if (MenuItem *i = find_item(M(m), id)) i->grayed = !on;
}

void menu_enable_sub(MenuRef parent, MenuRef child, bool on)
{
	if (!parent || !child) return;
	std::lock_guard lock(g_menu_mutex);
	for (auto &i : M(parent)->items)
		if (i.sub == M(child)) i.grayed = !on;
}

void menu_rename(MenuRef m, int id, const std::wstring &text)
{
	if (!m) return;
	std::lock_guard lock(g_menu_mutex);
	if (MenuItem *i = find_item(M(m), id)) i->text = text.substr(0, text.find(L'\t'));
}

int popup_menu(NativeWindow w, int, int, MenuRef m)
{
	if (!m) return 0;
	const std::vector<std::wstring> lines = menu_lines(m);
	{
		// (the menu and its small menus go)
		std::lock_guard lock(g_menu_mutex);
		std::vector<Menu *> all;
		collect(M(m), &all);
		for (Menu *x : all) {
			g_made.erase(std::remove(g_made.begin(), g_made.end(), x), g_made.end());
			delete x;
		}
	}
	if (!w || lines.empty()) return 0;
	LocalFrame frame;
	static const jmethodID f = bridge_method("popup", "([Ljava/lang/String;)I");
	AndroidState &s = android_state();
	++s.modal;
	const int r = jni()->CallStaticIntMethod(bridge_class(), f, to_java_array(lines));
	--s.modal;
	return r;
}

}  // namespace ng
