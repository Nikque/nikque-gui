// src/platform.h on Linux (GTK 3): what is not in src/posix. The window is a LinuxWindow (src/linux/app_linux.cpp);
// NativeWindow is its drawing area.
#include <gtk/gtk.h>

#include "../platform.h"

#include <iconv.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstring>

#include "../ini.h"
#include "../menu_text.h"
#include "../posix/posix.h"
#include "linux_view.h"

namespace ng {

namespace {

LinuxWindow *g_window = nullptr;

std::wstring from_utf8(const char *s) { return s ? widen(std::string_view(s)) : std::wstring(); }

}  // namespace

LinuxWindow *linux_window() { return g_window; }
void set_linux_window(LinuxWindow *w) { g_window = w; }

void modal_begin()
{
	if (g_window) ++g_window->modal;
}
void modal_end()
{
	if (g_window) --g_window->modal;
}
bool modal_open() { return g_window && g_window->modal > 0; }

GtkWindow *dialog_parent() { return g_window && g_window->window ? GTK_WINDOW(g_window->window) : nullptr; }

std::string gtk_text(const std::wstring &w) { return narrow(w); }

// ---- text of other code pages (the files of games in Shift_JIS)

std::optional<std::wstring> decode_codepage(std::string_view bytes, unsigned code_page)
{
	const std::string from = "CP" + std::to_string(code_page);
	const iconv_t cd = iconv_open("UTF-8", from.c_str());
	if (cd == reinterpret_cast<iconv_t>(-1)) return std::nullopt;
	std::string out(bytes.size() * 4 + 16, '\0');
	char *in = const_cast<char *>(bytes.data());
	size_t in_left = bytes.size();
	char *o = out.data();
	size_t out_left = out.size();
	const size_t r = iconv(cd, &in, &in_left, &o, &out_left);
	iconv_close(cd);
	if (r == static_cast<size_t>(-1) || in_left != 0) return std::nullopt;
	out.resize(out.size() - out_left);
	return widen(out);
}

// ---- the program

std::wstring exe_path()
{
	char b[PATH_MAX];
	const ssize_t n = ::readlink("/proc/self/exe", b, sizeof b - 1);
	if (n <= 0) return L"./nikque-gui";
	return widen(std::string_view(b, static_cast<size_t>(n)));
}

std::wstring temp_folder()
{
	std::wstring t = from_utf8(g_get_tmp_dir());
	if (t.empty() || t.back() != L'/') t += L'/';
	return t;
}

// the engines of the instructions this CPU has, the fastest first (the names of the Linux programs of the engines)
std::vector<std::wstring> engine_exe_names()
{
	std::vector<std::wstring> names;
#if defined(__x86_64__)
	__builtin_cpu_init();
	const bool v3 = __builtin_cpu_supports("avx2") && __builtin_cpu_supports("bmi") && __builtin_cpu_supports("bmi2") &&
	                __builtin_cpu_supports("fma") && __builtin_cpu_supports("popcnt");
	const bool v4 = v3 && __builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw") &&
	                __builtin_cpu_supports("avx512cd") && __builtin_cpu_supports("avx512dq") && __builtin_cpu_supports("avx512vl");
	if (v4) names.push_back(L"lEdax-x86-64-v4");
	if (v3) names.push_back(L"lEdax-x86-64-v3");
#endif
	names.push_back(L"lEdax-x86-64");
	return names;
}

// ---- the window

namespace {

struct Posted {
	unsigned msg;
	std::uintptr_t wp;
	std::intptr_t lp;
};

gboolean deliver(gpointer data)
{
	Posted *p = static_cast<Posted *>(data);
	LinuxWindow *lw = g_window;
	if (lw && lw->message) lw->message(p->msg, p->wp, p->lp);
	delete p;
	return G_SOURCE_REMOVE;
}

}  // namespace

// (from any thread: the main loop of GLib takes it on the thread of the window, in the order they were posted)
void post_message(NativeWindow w, unsigned msg, std::uintptr_t wp, std::intptr_t lp)
{
	if (!w) return;
	g_idle_add_full(G_PRIORITY_DEFAULT, deliver, new Posted{msg, wp, lp}, nullptr);
}

bool key_held(int vk)
{
	GdkDisplay *d = gdk_display_get_default();
	if (!d) return false;
	const guint f = gdk_keymap_get_modifier_state(gdk_keymap_get_for_display(d));
	switch (vk) {
	case VK_CONTROL: return (f & GDK_CONTROL_MASK) != 0;
	case VK_SHIFT: return (f & GDK_SHIFT_MASK) != 0;
	case VK_MENU: return (f & GDK_MOD1_MASK) != 0;
	default: return false;
	}
}

bool set_clipboard_text(NativeWindow, const std::wstring &text)
{
	GtkClipboard *c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	if (!c) return false;
	const std::string s = narrow(text);
	gtk_clipboard_set_text(c, s.c_str(), static_cast<gint>(s.size()));
	gtk_clipboard_store(c);  // (kept after the program has ended, where a clipboard manager runs)
	return true;
}

std::wstring clipboard_text(NativeWindow)
{
	GtkClipboard *c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	if (!c) return {};
	gchar *s = gtk_clipboard_wait_for_text(c);
	std::wstring w = from_utf8(s);
	g_free(s);
	return w;
}

int message_box(NativeWindow, const std::wstring &text, const std::wstring &title, unsigned flags)
{
	const unsigned icon = flags & 0xf0, kind = flags & 0xf;
	const GtkMessageType type = icon == MB_ICONERROR     ? GTK_MESSAGE_ERROR
	                            : icon == MB_ICONWARNING ? GTK_MESSAGE_WARNING
	                            : icon == MB_ICONQUESTION ? GTK_MESSAGE_QUESTION
	                                                      : GTK_MESSAGE_INFO;
	GtkWidget *d = gtk_message_dialog_new(dialog_parent(), static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
	                                      type, GTK_BUTTONS_NONE, "%s", narrow(text).c_str());
	gtk_window_set_title(GTK_WINDOW(d), narrow(title).c_str());
	// (the words of the buttons are the system's own, as on Windows)
	const char *ok = g_dgettext("gtk30", "_OK"), *cancel = g_dgettext("gtk30", "_Cancel");
	const char *yes = g_dgettext("gtk30", "_Yes"), *no = g_dgettext("gtk30", "_No");
	std::vector<int> answers;
	auto button = [&](const char *label, int answer) {
		gtk_dialog_add_button(GTK_DIALOG(d), label, answer);
		answers.push_back(answer);
	};
	switch (kind) {
	case MB_OKCANCEL:
		button(ok, IDOK);
		button(cancel, IDCANCEL);
		break;
	case MB_YESNO:
		button(yes, IDYES);
		button(no, IDNO);
		break;
	case MB_YESNOCANCEL:
		button(yes, IDYES);
		button(no, IDNO);
		button(cancel, IDCANCEL);
		break;
	default: button(ok, IDOK); break;
	}
	const size_t def = (flags & 0xf00) == MB_DEFBUTTON2 ? 1 : (flags & 0xf00) == MB_DEFBUTTON3 ? 2 : 0;
	gtk_dialog_set_default_response(GTK_DIALOG(d), answers[def < answers.size() ? def : 0]);
	modal_begin();
	const gint r = gtk_dialog_run(GTK_DIALOG(d));
	modal_end();
	gtk_widget_destroy(d);
	for (const int a : answers)
		if (a == r) return a;
	// (closed without a button: as Esc on Windows)
	return kind == MB_OK ? IDOK : kind == MB_YESNO ? IDNO : IDCANCEL;
}

// (the drawing is in the pixels of GTK, which are whole multiples of the pixels of a dense screen)
float window_scale(NativeWindow) { return 1.0f; }

bool window_minimized(NativeWindow w) { return w && g_window && g_window->minimized; }

void invalidate_window(NativeWindow w)
{
	if (!w || !g_window) return;
	g_window->needs_paint = true;
	gtk_widget_queue_draw(g_window->area);
}

void destroy_window(NativeWindow w)
{
	if (!w || !g_window) return;
	LinuxWindow *lw = g_window;
	for (auto &[id, source] : lw->timers) g_source_remove(source);
	lw->timers.clear();
	gtk_widget_hide(lw->window);
	if (gtk_main_level() > 0) gtk_main_quit();
}

bool window_placement(NativeWindow w, int *x, int *y, int *width, int *height, bool *maximized)
{
	if (!w || !g_window || g_window->w <= 0) return false;
	*x = g_window->x;
	*y = g_window->y;
	*width = g_window->w;
	*height = g_window->h;
	*maximized = g_window->maximized;
	return true;
}

void set_window_title(NativeWindow w, const std::wstring &title)
{
	if (w && g_window) gtk_window_set_title(GTK_WINDOW(g_window->window), narrow(title).c_str());
}

void track_mouse_leave(NativeWindow) {}  // (the drawing area gets the event of the mouse leaving it)

void open_text_file(NativeWindow, const std::wstring &path)
{
	gchar *uri = g_filename_to_uri(narrow(path).c_str(), nullptr, nullptr);
	if (!uri) return;
	g_app_info_launch_default_for_uri(uri, nullptr, nullptr);
	g_free(uri);
}

// ---- menus: a GtkMenuBar and GtkMenus. An item of a command shows a mark (checked), its words and the keys that do it
// too; its command is kept in the item.

namespace {

const char kId[] = "ng-id", kMark[] = "ng-mark", kLabel[] = "ng-label";

gboolean run_command(gpointer data)
{
	LinuxWindow *lw = g_window;
	if (lw && lw->command) lw->command(GPOINTER_TO_INT(data));
	return G_SOURCE_REMOVE;
}

void item_activated(GtkMenuItem *item, gpointer) { menu_command(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), kId))); }

GtkWidget *find_item(GtkWidget *menu, int id)
{
	GtkWidget *found = nullptr;
	GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
	for (GList *i = items; i && !found; i = i->next) {
		GtkWidget *item = GTK_WIDGET(i->data);
		if (!GTK_IS_MENU_ITEM(item)) continue;
		if (GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item))) found = find_item(sub, id);
		else if (g_object_get_data(G_OBJECT(item), kLabel) && GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), kId)) == id)
			found = item;
	}
	g_list_free(items);
	return found;
}

void split_keys(const std::wstring &text, std::string *label, std::string *keys)
{
	const size_t tab = text.find(L'\t');
	*label = narrow(tab == std::wstring::npos ? text : text.substr(0, tab));
	*keys = tab == std::wstring::npos ? std::string() : narrow(text.substr(tab + 1));
}

}  // namespace

// (after the menu has closed: a command may open a window of its own)
void menu_command(int id)
{
	LinuxWindow *lw = g_window;
	if (!lw) return;
	if (lw->popup_open) lw->popup_choice = id;
	else g_idle_add(run_command, GINT_TO_POINTER(id));
}

MenuRef menu_new(bool bar) { return bar ? gtk_menu_bar_new() : gtk_menu_new(); }

void menu_item(MenuRef m, int id, const std::wstring &text, bool grayed)
{
	std::string label, keys;
	split_keys(text, &label, &keys);
	GtkWidget *item = gtk_menu_item_new();
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *mark = gtk_label_new("");
	gtk_label_set_width_chars(GTK_LABEL(mark), 2);
	GtkWidget *words = gtk_label_new(label.c_str());
	gtk_label_set_xalign(GTK_LABEL(words), 0);
	gtk_box_pack_start(GTK_BOX(row), mark, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row), words, TRUE, TRUE, 0);
	if (!keys.empty()) {
		GtkWidget *k = gtk_label_new(keys.c_str());
		gtk_widget_set_opacity(k, 0.6);
		gtk_widget_set_margin_start(k, 18);
		gtk_box_pack_end(GTK_BOX(row), k, FALSE, FALSE, 0);
	}
	gtk_container_add(GTK_CONTAINER(item), row);
	g_object_set_data(G_OBJECT(item), kId, GINT_TO_POINTER(id));
	g_object_set_data(G_OBJECT(item), kMark, mark);
	g_object_set_data(G_OBJECT(item), kLabel, words);
	gtk_widget_set_sensitive(item, !grayed);
	g_signal_connect(item, "activate", G_CALLBACK(item_activated), nullptr);
	gtk_menu_shell_append(GTK_MENU_SHELL(static_cast<GtkWidget *>(m)), item);
}

void menu_sub(MenuRef parent, MenuRef m, const std::wstring &text)
{
	GtkWidget *item = gtk_menu_item_new_with_label(narrow(menu_words(text)).c_str());
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), static_cast<GtkWidget *>(m));
	gtk_menu_shell_append(GTK_MENU_SHELL(static_cast<GtkWidget *>(parent)), item);
}

void menu_sep(MenuRef m) { gtk_menu_shell_append(GTK_MENU_SHELL(static_cast<GtkWidget *>(m)), gtk_separator_menu_item_new()); }

void menu_set_bar(NativeWindow w, MenuRef bar)
{
	LinuxWindow *lw = g_window;
	GtkWidget *b = static_cast<GtkWidget *>(bar);
	if (!w || !lw) {
		if (b) gtk_widget_destroy(b);
		return;
	}
	if (lw->menu_bar) gtk_widget_destroy(lw->menu_bar);
	lw->menu_bar = b;
	if (!b) return;
	gtk_box_pack_start(GTK_BOX(lw->box), b, FALSE, FALSE, 0);
	gtk_box_reorder_child(GTK_BOX(lw->box), b, 0);
	gtk_widget_show_all(b);
}

MenuRef menu_bar(NativeWindow w) { return w && g_window ? g_window->menu_bar : nullptr; }

void menu_check(MenuRef m, int id, bool on)
{
	if (!m) return;
	if (GtkWidget *item = find_item(static_cast<GtkWidget *>(m), id))
		gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(item), kMark)), on ? "✓" : "");
}

void menu_radio(MenuRef m, int first, int last, int on)
{
	for (int id = first; id <= last; ++id) menu_check(m, id, id == on);
}

void menu_enable(MenuRef m, int id, bool on)
{
	if (!m) return;
	if (GtkWidget *item = find_item(static_cast<GtkWidget *>(m), id)) gtk_widget_set_sensitive(item, on);
}

void menu_enable_sub(MenuRef parent, MenuRef child, bool on)
{
	if (!parent || !child) return;
	GList *items = gtk_container_get_children(GTK_CONTAINER(static_cast<GtkWidget *>(parent)));
	for (GList *i = items; i; i = i->next) {
		GtkWidget *item = GTK_WIDGET(i->data);
		if (GTK_IS_MENU_ITEM(item) && gtk_menu_item_get_submenu(GTK_MENU_ITEM(item)) == static_cast<GtkWidget *>(child))
			gtk_widget_set_sensitive(item, on);
	}
	g_list_free(items);
}

void menu_rename(MenuRef m, int id, const std::wstring &text)
{
	if (!m) return;
	std::string label, keys;
	split_keys(text, &label, &keys);
	if (GtkWidget *item = find_item(static_cast<GtkWidget *>(m), id))
		gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(item), kLabel)), label.c_str());
}

namespace {

void popup_closed(GtkMenuShell *, gpointer)
{
	LinuxWindow *lw = g_window;
	if (lw && lw->popup_loop && g_main_loop_is_running(lw->popup_loop)) g_main_loop_quit(lw->popup_loop);
}

}  // namespace

int popup_menu(NativeWindow w, int x, int y, MenuRef m)
{
	GtkWidget *menu = static_cast<GtkWidget *>(m);
	LinuxWindow *lw = g_window;
	if (!w || !lw || !gtk_widget_get_realized(lw->area)) {
		if (menu) gtk_widget_destroy(menu);
		return 0;
	}
	g_object_ref_sink(menu);
	gtk_menu_attach_to_widget(GTK_MENU(menu), lw->area, nullptr);
	gtk_widget_show_all(menu);
	g_signal_connect(menu, "deactivate", G_CALLBACK(popup_closed), nullptr);
	lw->popup_open = true;
	lw->popup_choice = 0;
	lw->popup_loop = g_main_loop_new(nullptr, FALSE);
	const GdkRectangle at{x, y, 1, 1};
	gtk_menu_popup_at_rect(GTK_MENU(menu), gtk_widget_get_window(lw->area), &at, GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST,
	                       nullptr);
	// (the menu closes with a choice, a click elsewhere or Esc; the choice comes before the loop ends)
	if (gtk_widget_get_mapped(menu)) g_main_loop_run(lw->popup_loop);
	g_main_loop_unref(lw->popup_loop);
	lw->popup_loop = nullptr;
	lw->popup_open = false;
	gtk_menu_detach(GTK_MENU(menu));
	gtk_widget_destroy(menu);
	g_object_unref(menu);
	return lw->popup_choice;
}

}  // namespace ng
