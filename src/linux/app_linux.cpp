// The parts of App that are Linux' own (GTK 3): the window and its drawing area, the events (as WndProc of
// src/win/app_win.cpp hands them to the App), the timers, the icon, the file dialogs.
#include <gtk/gtk.h>

#include "../app.h"

#include <cmath>
#include <filesystem>

#include "../lang.h"
#include "linux_view.h"

namespace ng {

namespace {

LinuxWindow g_lw;

App &app() { return *g_lw.app; }

// the virtual key of Windows for a key of GDK (0: none)
int virtual_key(guint keyval)
{
	switch (keyval) {
	case GDK_KEY_Left:
	case GDK_KEY_KP_Left: return VK_LEFT;
	case GDK_KEY_Right:
	case GDK_KEY_KP_Right: return VK_RIGHT;
	case GDK_KEY_Up:
	case GDK_KEY_KP_Up: return VK_UP;
	case GDK_KEY_Down:
	case GDK_KEY_KP_Down: return VK_DOWN;
	case GDK_KEY_Home:
	case GDK_KEY_KP_Home: return VK_HOME;
	case GDK_KEY_End:
	case GDK_KEY_KP_End: return VK_END;
	case GDK_KEY_Page_Up:
	case GDK_KEY_KP_Page_Up: return VK_PRIOR;
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Page_Down: return VK_NEXT;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter: return VK_RETURN;
	case GDK_KEY_Escape: return VK_ESCAPE;
	case GDK_KEY_BackSpace: return VK_BACK;
	case GDK_KEY_Delete:
	case GDK_KEY_KP_Delete: return VK_DELETE;
	case GDK_KEY_Tab:
	case GDK_KEY_ISO_Left_Tab: return VK_TAB;
	case GDK_KEY_space: return VK_SPACE;
	case GDK_KEY_Insert:
	case GDK_KEY_KP_Insert: return VK_INSERT;
	default: break;
	}
	if (keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F12) return VK_F1 + static_cast<int>(keyval - GDK_KEY_F1);
	if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9) return '0' + static_cast<int>(keyval - GDK_KEY_KP_0);
	const guint up = gdk_keyval_to_upper(keyval);
	if ((up >= 'A' && up <= 'Z') || (up >= '0' && up <= '9')) return static_cast<int>(up);
	return 0;
}

void handle_message(unsigned msg, std::uintptr_t wp, std::intptr_t lp)
{
	App &a = app();
	switch (msg) {
	case WM_TASK: a.task_ended(); break;
	case WM_SCRIPT: a.pump_script(); break;
	case WM_REDUCE: a.reduce_counted(); break;
	case WM_BOOK_PEEK: a.book_peek_answers(); break;
	case WM_EVAL_WARN: a.eval_mismatch_box(); break;
	case WM_ENGINE: a.on_engine(wp, lp != 0); break;
	case WM_CLOSE: a.close_window(); break;
	default: break;
	}
}

void handle_command(int id) { app().on_command(id); }

// ---- the drawing area

gboolean on_draw(GtkWidget *, cairo_t *cr, gpointer)
{
	App &a = app();
	if (g_lw.needs_paint) {
		g_lw.needs_paint = false;
		a.gfx_.set_in_view_draw(true);
		a.paint();
		a.gfx_.set_in_view_draw(false);
	}
	if (auto *s = static_cast<cairo_surface_t *>(a.gfx_.copy_frame_image())) {
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_surface(cr, s, 0, 0);
		cairo_paint(cr);
	}
	return FALSE;
}

void on_size(GtkWidget *, GdkRectangle *, gpointer)
{
	App &a = app();
	if (!a.gfx_.ready()) return;
	a.gfx_.resize();
	a.layout();
	a.update_animation_timer();
	a.invalidate();
}

// (the window moved to a screen of another density)
void on_scale(GObject *, GParamSpec *, gpointer)
{
	App &a = app();
	if (!a.gfx_.ready()) return;
	a.gfx_.resize();
	a.gfx_.clear_shapes();
	a.layout();
	a.invalidate();
}

void update_cursor(double x, double y)
{
	App &a = app();
	const POINT p{static_cast<LONG>(x), static_cast<LONG>(y)};
	const char *name = "default";
	if (a.drag_ == App::Drag::Panel || (!a.panel_hidden_ && !a.lay_.panel_below && inside(a.lay_.panel_split, p))) name = "col-resize";
	else if (a.drag_ == App::Drag::Graph || (!a.panel_hidden_ && inside(a.lay_.splitter, p))) name = "row-resize";
	else if (!a.panel_hidden_ && (inside(a.lay_.fx.box, p) || inside(a.lay_.fy.box, p) || inside(a.lay_.record_text, p))) name = "text";
	static const char *shown = nullptr;
	if (name == shown) return;
	shown = name;
	GdkWindow *w = gtk_widget_get_window(g_lw.area);
	if (!w) return;
	GdkCursor *c = gdk_cursor_new_from_name(gdk_window_get_display(w), name);
	gdk_window_set_cursor(w, c);
	if (c) g_object_unref(c);
}

gboolean on_button_press(GtkWidget *area, GdkEventButton *e, gpointer)
{
	// (the second press of a double click comes as a press too, then as GDK_2BUTTON_PRESS: told here by its time and place)
	if (e->type != GDK_BUTTON_PRESS) return TRUE;
	gtk_widget_grab_focus(area);
	const int x = static_cast<int>(e->x), y = static_cast<int>(e->y);
	if (e->button == 3) {
		app().on_mouse_down(x, y, true, false);
		return TRUE;
	}
	if (e->button != 1) return FALSE;
	gint dbl_ms = 400, dbl_px = 5;
	g_object_get(gtk_settings_get_default(), "gtk-double-click-time", &dbl_ms, "gtk-double-click-distance", &dbl_px, nullptr);
	const bool dbl = g_lw.click_time != 0 && e->time - g_lw.click_time <= static_cast<guint32>(dbl_ms) &&
	                 std::fabs(e->x - g_lw.click_x) <= dbl_px && std::fabs(e->y - g_lw.click_y) <= dbl_px;
	// (a third press starts again, as the messages of Windows)
	g_lw.click_time = dbl ? 0 : e->time;
	g_lw.click_x = e->x;
	g_lw.click_y = e->y;
	app().on_mouse_down(x, y, false, dbl);
	return TRUE;
}

gboolean on_button_release(GtkWidget *, GdkEventButton *e, gpointer)
{
	if (e->button != 1) return FALSE;
	app().on_mouse_up(static_cast<int>(e->x), static_cast<int>(e->y));
	return TRUE;
}

gboolean on_motion(GtkWidget *, GdkEventMotion *e, gpointer)
{
	app().on_mouse_move(static_cast<int>(e->x), static_cast<int>(e->y));
	update_cursor(e->x, e->y);
	return TRUE;
}

gboolean on_leave(GtkWidget *, GdkEventCrossing *e, gpointer)
{
	if (e->mode == GDK_CROSSING_NORMAL) app().on_mouse_move(-1, -1);
	return FALSE;
}

gboolean on_scroll(GtkWidget *, GdkEventScroll *e, gpointer)
{
	const int x = static_cast<int>(e->x), y = static_cast<int>(e->y);
	switch (e->direction) {
	case GDK_SCROLL_UP: app().on_wheel(x, y, 120); break;
	case GDK_SCROLL_DOWN: app().on_wheel(x, y, -120); break;
	case GDK_SCROLL_SMOOTH: {
		// a notch of a wheel is a step; the fine scrolling of a touchpad is gathered into steps
		g_lw.wheel -= e->delta_y;
		while (std::fabs(g_lw.wheel) >= 1) {
			const int step = g_lw.wheel > 0 ? 1 : -1;
			g_lw.wheel -= step;
			app().on_wheel(x, y, step * 120);
		}
		if (e->is_stop) g_lw.wheel = 0;
		break;
	}
	default: return FALSE;
	}
	return TRUE;
}

gboolean key_pressed(GtkWidget *, GdkEventKey *e, gpointer)
{
	const int vk = virtual_key(e->keyval);
	const bool alt = (e->state & GDK_MOD1_MASK) != 0, ctrl = (e->state & GDK_CONTROL_MASK) != 0;
	// (with Alt only the arrows are keys of the app, as on Windows)
	if (alt && vk != VK_LEFT && vk != VK_RIGHT && vk != VK_UP && vk != VK_DOWN) return FALSE;
	if (vk) app().on_key(static_cast<WPARAM>(vk));
	// the letters typed (as WM_CHAR), not with Control or Alt
	if (!ctrl && !alt) {
		const guint32 u = gdk_keyval_to_unicode(e->keyval);
		if (u >= 0x20 && u != 0x7f) app().on_char(static_cast<wchar_t>(u));
	}
	return vk != 0;
}

// ---- files dropped on the window: a book

void on_drop(GtkWidget *, GdkDragContext *ctx, gint, gint, GtkSelectionData *data, guint, guint time, gpointer)
{
	std::wstring file;
	if (gchar **uris = gtk_selection_data_get_uris(data)) {
		if (uris[0]) {
			if (gchar *name = g_filename_from_uri(uris[0], nullptr, nullptr)) {
				file = widen(name);
				g_free(name);
			}
		}
		g_strfreev(uris);
	}
	gtk_drag_finish(ctx, !file.empty(), FALSE, time);
	if (!file.empty()) app().switch_book(file);
}

// ---- the window

gboolean on_delete(GtkWidget *, GdkEvent *, gpointer)
{
	app().close_window();
	return TRUE;
}

gboolean on_configure(GtkWidget *w, GdkEventConfigure *, gpointer)
{
	if (!g_lw.maximized && !g_lw.minimized) {
		gtk_window_get_position(GTK_WINDOW(w), &g_lw.x, &g_lw.y);
		gtk_window_get_size(GTK_WINDOW(w), &g_lw.w, &g_lw.h);
	}
	return FALSE;
}

gboolean on_window_state(GtkWidget *, GdkEventWindowState *e, gpointer)
{
	const bool was_min = g_lw.minimized;
	g_lw.maximized = (e->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
	g_lw.minimized = (e->new_window_state & GDK_WINDOW_STATE_ICONIFIED) != 0;
	if (was_min != g_lw.minimized) {
		app().update_animation_timer();
		if (!g_lw.minimized) app().invalidate();
	}
	return FALSE;
}

gboolean on_focus_in(GtkWidget *, GdkEventFocus *, gpointer)
{
	App &a = app();
	a.active_ = true;
	a.update_animation_timer();
	if (a.record_resumes() && g_lw.record) gtk_widget_grab_focus(g_lw.record);
	return FALSE;
}

gboolean on_focus_out(GtkWidget *, GdkEventFocus *, gpointer)
{
	app().active_ = false;
	app().update_animation_timer();
	return FALSE;
}

gboolean on_timer(gpointer data)
{
	const std::uintptr_t id = static_cast<std::uintptr_t>(GPOINTER_TO_SIZE(data));
	App &a = app();
	if (id == kTimerAnim) a.paint_animation();
	else if (id == kTimerWatch) a.watch_engines();
	else if (id == kTimerScript) a.pump_script();
	else if (id == kTimerSave) a.watch_save();
	else play_timer(a, id);
	return G_SOURCE_CONTINUE;
}

gboolean record_position(GtkOverlay *, GtkWidget *child, GdkRectangle *at, gpointer)
{
	if (child != g_lw.record) return FALSE;
	*at = g_lw.record_at;
	return TRUE;
}

// the icon of the window, drawn as the icons of Windows are (src/win/icon_draw.cpp)
GdkPixbuf *draw_icon(int px, bool paw)
{
	cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, px, px);
	cairo_t *g = cairo_create(sf);
	const double s = px, r = s * 0.2, pi = 3.14159265358979;
	auto hex = [&](unsigned v) { cairo_set_source_rgb(g, ((v >> 16) & 255) / 255.0, ((v >> 8) & 255) / 255.0, (v & 255) / 255.0); };
	auto rounded = [&](double x0, double y0, double x1, double y1) {
		cairo_new_sub_path(g);
		cairo_arc(g, x1 - r, y0 + r, r, -pi / 2, 0);
		cairo_arc(g, x1 - r, y1 - r, r, 0, pi / 2);
		cairo_arc(g, x0 + r, y1 - r, r, pi / 2, pi);
		cairo_arc(g, x0 + r, y0 + r, r, pi, 3 * pi / 2);
		cairo_close_path(g);
	};
	auto oval = [&](double cx, double cy, double rx, double ry) {
		cairo_save(g);
		cairo_translate(g, cx, cy);
		cairo_scale(g, rx, ry);
		cairo_new_sub_path(g);
		cairo_arc(g, 0, 0, 1, 0, 2 * pi);
		cairo_restore(g);
	};
	if (!paw) {
		// a blue board square with a black and a white disc
		hex(0x0684c7);
		rounded(0, 0, s, s);
		cairo_fill(g);
		const double d = s * 0.27;
		cairo_set_line_width(g, s / 48.0 + 0.5);
		hex(0xf1f1f1);
		oval(s * 0.63, s * 0.63, d, d);
		cairo_fill(g);
		hex(0x9f9fa9);
		oval(s * 0.63, s * 0.63, d, d);
		cairo_stroke(g);
		hex(0x101113);
		oval(s * 0.37, s * 0.37, d, d);
		cairo_fill(g);
		hex(0x52525c);
		oval(s * 0.37, s * 0.37, d, d);
		cairo_stroke(g);
	} else {
		// a pink paw on milk yellow
		hex(0xfdf3d9);
		rounded(0, 0, s, s);
		cairo_fill(g);
		hex(0xfccee8);
		cairo_set_line_width(g, s * 0.06);
		rounded(s * 0.03, s * 0.03, s * 0.97, s * 0.97);
		cairo_stroke(g);
		hex(0xfb64b6);
		oval(s * 0.5, s * 0.64, s * 0.23, s * 0.19);
		cairo_fill(g);
		const double toes[4][2] = {{0.22, 0.42}, {0.39, 0.25}, {0.61, 0.25}, {0.78, 0.42}};
		for (const auto &t : toes) {
			oval(s * t[0], s * t[1], s * 0.09, s * 0.105);
			cairo_fill(g);
		}
	}
	cairo_destroy(g);
	GdkPixbuf *pb = gdk_pixbuf_get_from_surface(sf, 0, 0, px, px);
	cairo_surface_destroy(sf);
	return pb;
}

// the filters of a file dialog from the filter of Windows: pairs of words and patterns ("*.a;*.b"), each ended by
// a zero, the list by another
void add_filters(GtkFileChooser *fc, const wchar_t *filter)
{
	if (!filter) return;
	for (const wchar_t *p = filter; *p;) {
		const std::wstring words = p;
		p += words.size() + 1;
		if (!*p) break;
		const std::wstring patterns = p;
		p += patterns.size() + 1;
		GtkFileFilter *f = gtk_file_filter_new();
		gtk_file_filter_set_name(f, narrow(words).c_str());
		size_t at = 0;
		while (at <= patterns.size()) {
			size_t end = patterns.find(L';', at);
			if (end == std::wstring::npos) end = patterns.size();
			std::wstring one = patterns.substr(at, end - at);
			if (one == L"*.*") one = L"*";
			if (!one.empty()) gtk_file_filter_add_pattern(f, narrow(one).c_str());
			at = end + 1;
		}
		gtk_file_chooser_add_filter(fc, f);
	}
}

std::wstring run_chooser(GtkWidget *d)
{
	modal_begin();
	const gint r = gtk_dialog_run(GTK_DIALOG(d));
	modal_end();
	std::wstring path;
	if (r == GTK_RESPONSE_ACCEPT) {
		if (gchar *name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d))) {
			path = widen(name);
			g_free(name);
		}
	}
	gtk_widget_destroy(d);
	return path;
}

}  // namespace

int App::run(HINSTANCE, int)
{
	prepare();

	g_lw = LinuxWindow{};
	g_lw.app = this;
	g_lw.message = handle_message;
	g_lw.command = handle_command;
	set_linux_window(&g_lw);

	const int w = ini_.get_int("window", "w", 1280, 400, 16000);
	const int h = ini_.get_int("window", "h", 820, 300, 16000);
	const int x = ini_.get_int("window", "x", -32001, -32000, 32000);
	const int y = ini_.get_int("window", "y", -32001, -32000, 32000);

	GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_lw.window = win;
	gtk_window_set_title(GTK_WINDOW(win), narrow(lang::t("app.title")).c_str());
	gtk_window_set_default_size(GTK_WINDOW(win), w, h);
	// the place kept in config.ini, when it is on a screen (none kept: the value is at the end of its range)
	if (x > -32000 && y > -32000) {
		bool on_screen = false;
		if (GdkDisplay *d = gdk_display_get_default()) {
			const GdkRectangle at{x, y, w, h};
			for (int i = 0; i < gdk_display_get_n_monitors(d); ++i) {
				GdkRectangle m;
				gdk_monitor_get_geometry(gdk_display_get_monitor(d, i), &m);
				if (gdk_rectangle_intersect(&at, &m, nullptr)) on_screen = true;
			}
		}
		if (on_screen) gtk_window_move(GTK_WINDOW(win), x, y);
	}
	g_lw.box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(win), g_lw.box);
	g_lw.overlay = gtk_overlay_new();
	gtk_box_pack_end(GTK_BOX(g_lw.box), g_lw.overlay, TRUE, TRUE, 0);
	GtkWidget *area = gtk_drawing_area_new();
	g_lw.area = area;
	gtk_widget_set_size_request(area, 480, 320);
	gtk_widget_set_can_focus(area, TRUE);
	gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK |
	                                GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_KEY_PRESS_MASK);
	gtk_container_add(GTK_CONTAINER(g_lw.overlay), area);
	g_signal_connect(g_lw.overlay, "get-child-position", G_CALLBACK(record_position), nullptr);
	g_signal_connect(area, "draw", G_CALLBACK(on_draw), nullptr);
	g_signal_connect(area, "size-allocate", G_CALLBACK(on_size), nullptr);
	g_signal_connect(area, "notify::scale-factor", G_CALLBACK(on_scale), nullptr);
	g_signal_connect(area, "button-press-event", G_CALLBACK(on_button_press), nullptr);
	g_signal_connect(area, "button-release-event", G_CALLBACK(on_button_release), nullptr);
	g_signal_connect(area, "motion-notify-event", G_CALLBACK(on_motion), nullptr);
	g_signal_connect(area, "leave-notify-event", G_CALLBACK(on_leave), nullptr);
	g_signal_connect(area, "scroll-event", G_CALLBACK(on_scroll), nullptr);
	g_signal_connect(area, "key-press-event", G_CALLBACK(key_pressed), nullptr);
	gtk_drag_dest_set(area, GTK_DEST_DEFAULT_ALL, nullptr, 0, GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets(area);
	g_signal_connect(area, "drag-data-received", G_CALLBACK(on_drop), nullptr);
	g_signal_connect(win, "delete-event", G_CALLBACK(on_delete), nullptr);
	g_signal_connect(win, "configure-event", G_CALLBACK(on_configure), nullptr);
	g_signal_connect(win, "window-state-event", G_CALLBACK(on_window_state), nullptr);
	g_signal_connect(win, "focus-in-event", G_CALLBACK(on_focus_in), nullptr);
	g_signal_connect(win, "focus-out-event", G_CALLBACK(on_focus_out), nullptr);
	hwnd_ = area;

	if (!gfx_.init(hwnd_)) {
		message(lang::t("err.d2d"), MB_ICONERROR);
		return 1;
	}
	create_record_box();
	build_menu();
	set_window_icon();
	if (ini_.get_bool("window", "max", false)) gtk_window_maximize(GTK_WINDOW(win));
	gtk_widget_show_all(win);
	gtk_widget_grab_focus(area);
	// the window has its size before the engines start and the startup script runs (as on Windows)
	while (gtk_events_pending()) gtk_main_iteration();
	gfx_.resize();
	layout();
	started();
	gtk_main();

	// the temporary folder of this run (empty books, files given to the engines)
	std::error_code ec;
	std::filesystem::remove_all(narrow(temp_dir_), ec);
	hwnd_ = nullptr;
	record_edit_ = nullptr;
	set_linux_window(nullptr);
	gtk_widget_destroy(win);
	g_lw = LinuxWindow{};
	return 0;
}

void App::set_window_icon()
{
	if (!g_lw.window) return;
	GList *icons = nullptr;
	for (const int px : {16, 32, 48, 64, 128, 256}) icons = g_list_append(icons, draw_icon(px, design_ == Design::Paw));
	gtk_window_set_icon_list(GTK_WINDOW(g_lw.window), icons);
	g_list_free_full(icons, g_object_unref);
}

void App::set_timer(UINT_PTR id, UINT ms)
{
	if (!hwnd_) return;
	kill_timer(id);
	g_lw.timers[id] = g_timeout_add(ms, on_timer, GSIZE_TO_POINTER(static_cast<gsize>(id)));
}

void App::kill_timer(UINT_PTR id)
{
	auto it = g_lw.timers.find(id);
	if (it == g_lw.timers.end()) return;
	g_source_remove(it->second);
	g_lw.timers.erase(it);
}

void App::post(UINT msg) { post_message(hwnd_, msg); }

bool App::window_enabled() const { return !modal_open(); }

std::wstring App::pick_open(const wchar_t *filter, const std::wstring &title)
{
	GtkWidget *d = gtk_file_chooser_dialog_new(narrow(title.empty() ? lang::t("app.title") : title).c_str(), dialog_parent(),
	                                           GTK_FILE_CHOOSER_ACTION_OPEN, g_dgettext("gtk30", "_Cancel"), GTK_RESPONSE_CANCEL,
	                                           g_dgettext("gtk30", "_Open"), GTK_RESPONSE_ACCEPT, nullptr);
	add_filters(GTK_FILE_CHOOSER(d), filter);
	return run_chooser(d);
}

std::wstring App::pick_save(const wchar_t *filter, const wchar_t *ext, const std::wstring &name, const std::wstring &title)
{
	GtkWidget *d = gtk_file_chooser_dialog_new(narrow(title.empty() ? lang::t("app.title") : title).c_str(), dialog_parent(),
	                                           GTK_FILE_CHOOSER_ACTION_SAVE, g_dgettext("gtk30", "_Cancel"), GTK_RESPONSE_CANCEL,
	                                           g_dgettext("gtk30", "_Save"), GTK_RESPONSE_ACCEPT, nullptr);
	GtkFileChooser *fc = GTK_FILE_CHOOSER(d);
	gtk_file_chooser_set_do_overwrite_confirmation(fc, TRUE);
	add_filters(fc, filter);
	const size_t slash = name.find_last_of(L'/');
	if (slash != std::wstring::npos) {
		gtk_file_chooser_set_current_folder(fc, narrow(name.substr(0, slash)).c_str());
		gtk_file_chooser_set_current_name(fc, narrow(name.substr(slash + 1)).c_str());
	} else if (!name.empty()) {
		gtk_file_chooser_set_current_name(fc, narrow(name).c_str());
	}
	std::wstring path = run_chooser(d);
	// (a name without an extension gets the one of the kind of file)
	if (!path.empty() && ext && *ext) {
		const size_t s = path.find_last_of(L'/'), dot = path.find_last_of(L'.');
		if (dot == std::wstring::npos || (s != std::wstring::npos && dot < s)) path += L"." + std::wstring(ext);
	}
	return path;
}

}  // namespace ng
