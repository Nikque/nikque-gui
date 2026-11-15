// The text box of the row of the moves on Linux: a GtkEntry over the drawing area (src/app_record.cpp has what is
// shared). Enter takes what was typed, Esc takes it back; a paste loads at once.
#include <gtk/gtk.h>

#include "../app.h"

#include <cmath>
#include <cstdio>

#include "../lang.h"
#include "linux_view.h"

namespace ng {

namespace {

bool g_modified = false;  // typed in since it was given the moves
bool g_setting = false;   // the text is being set by the app (not typed)
GtkCssProvider *g_css = nullptr;
std::string g_css_text;

GtkWidget *box_of(const App &a) { return static_cast<GtkWidget *>(a.record_edit_); }

App *app_of(gpointer data) { return static_cast<App *>(data); }

void on_activate(GtkEntry *, gpointer data) { app_of(data)->record_key(VK_RETURN); }

gboolean key_pressed(GtkWidget *, GdkEventKey *e, gpointer data)
{
	if (e->keyval != GDK_KEY_Escape) return FALSE;
	app_of(data)->record_key(VK_ESCAPE);
	return TRUE;
}

void on_changed(GtkEditable *, gpointer)
{
	if (!g_setting) g_modified = true;
}

void on_paste(GtkEntry *e, gpointer data)
{
	g_signal_stop_emission_by_name(e, "paste-clipboard");
	App *a = app_of(data);
	a->record_take(narrow(a->clipboard_text()));
}

gboolean on_focus_in(GtkWidget *, GdkEventFocus *, gpointer data)
{
	app_of(data)->record_resume_ = false;
	return FALSE;
}

gboolean on_focus_out(GtkWidget *, GdkEventFocus *, gpointer data)
{
	// to the board of this window: taken back; to another window or program: it stays, and the keys come back to it
	App *a = app_of(data);
	LinuxWindow *lw = linux_window();
	const bool to_board = lw && lw->window && gtk_window_has_toplevel_focus(GTK_WINDOW(lw->window));
	a->record_focus_lost(to_board ? a->hwnd_ : nullptr);
	return FALSE;
}

std::string css_color(const Color &c)
{
	char b[16];
	std::snprintf(b, sizeof b, "#%02x%02x%02x", static_cast<int>(std::lround(c.r * 255)), static_cast<int>(std::lround(c.g * 255)),
	              static_cast<int>(std::lround(c.b * 255)));
	return b;
}

void set_text(GtkWidget *e, const std::string &text)
{
	g_setting = true;
	gtk_entry_set_text(GTK_ENTRY(e), text.c_str());
	g_setting = false;
}

}  // namespace

App::~App() = default;  // (the text box goes with the window)

void App::create_record_box()
{
	LinuxWindow *lw = linux_window();
	if (!hwnd_ || !lw) return;
	GtkWidget *e = gtk_entry_new();
	gtk_entry_set_has_frame(GTK_ENTRY(e), FALSE);
	gtk_widget_set_no_show_all(e, TRUE);
	gtk_overlay_add_overlay(GTK_OVERLAY(lw->overlay), e);
	g_signal_connect(e, "activate", G_CALLBACK(on_activate), this);
	g_signal_connect(e, "key-press-event", G_CALLBACK(key_pressed), this);
	g_signal_connect(e, "changed", G_CALLBACK(on_changed), this);
	g_signal_connect(e, "paste-clipboard", G_CALLBACK(on_paste), this);
	g_signal_connect(e, "focus-in-event", G_CALLBACK(on_focus_in), this);
	g_signal_connect(e, "focus-out-event", G_CALLBACK(on_focus_out), this);
	g_css = gtk_css_provider_new();
	gtk_style_context_add_provider(gtk_widget_get_style_context(e), GTK_STYLE_PROVIDER(g_css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_css_text.clear();
	g_modified = false;
	record_font_px_ = 0;
	lw->record = e;
	record_edit_ = e;
}

void App::place_record_box()
{
	GtkWidget *e = box_of(*this);
	LinuxWindow *lw = linux_window();
	if (!e || !lw) return;
	const Layout &L = lay_;
	const RectF &T = L.record_text;
	if (T.right <= T.left) {
		// (the keys in another window meanwhile: what was typed is taken back when the box is shown again)
		leave_record_box();
		if (g_modified) sync_record_box(true);
		gtk_widget_hide(e);
		return;
	}
	const int px = static_cast<int>(L.font_s);
	record_font_px_ = px;
	const Theme &th = theme(design_);
	const std::string css = "entry { font-size: " + std::to_string(px) + "px; min-height: 0; padding: 0 2px; border: none; " +
	                        "border-radius: 0; box-shadow: none; background-image: none; background-color: " + css_color(th.panel2) +
	                        "; color: " + css_color(th.text) + "; caret-color: " + css_color(th.text) + "; }";
	if (css != g_css_text) {
		gtk_css_provider_load_from_data(g_css, css.c_str(), -1, nullptr);
		g_css_text = css;
	}
	gtk_entry_set_placeholder_text(GTK_ENTRY(e), narrow(lang::t("record.hint")).c_str());
	const float pad = std::round(6 * L.scale);
	const float h = std::round(px * 1.4f);
	const float y = std::round((T.top + T.bottom - h) / 2);
	const GdkRectangle at{static_cast<int>(T.left + pad), static_cast<int>(y), static_cast<int>(T.right - T.left - 2 * pad),
	                      static_cast<int>(h)};
	if (at.x != lw->record_at.x || at.y != lw->record_at.y || at.width != lw->record_at.width || at.height != lw->record_at.height) {
		lw->record_at = at;
		gtk_widget_queue_resize(e);
	}
	gtk_widget_show(e);
}

void App::sync_record_box(bool force)
{
	GtkWidget *e = box_of(*this);
	if (!e) return;
	if (editing_) leave_record_box();
	gtk_widget_set_sensitive(e, !editing_);
	if (!force && g_modified) return;
	std::wstring text = record_text();
	if (!force && text == record_shown_) return;
	set_text(e, narrow(text));
	g_modified = false;
	record_shown_ = std::move(text);
	// the last moves are the ones in sight
	gtk_editable_set_position(GTK_EDITABLE(e), -1);
}

void App::leave_record_box()
{
	record_resume_ = false;
	GtkWidget *e = box_of(*this);
	LinuxWindow *lw = linux_window();
	if (e && lw && gtk_widget_has_focus(e)) gtk_widget_grab_focus(lw->area);
}

void App::record_focus_lost(HWND to)
{
	if (!to) {
		record_resume_ = true;
		return;
	}
	sync_record_box(true);
}

bool App::record_resumes()
{
	const bool r = record_resume_;
	record_resume_ = false;
	GtkWidget *e = box_of(*this);
	return r && e && gtk_widget_get_visible(e) && gtk_widget_get_sensitive(e);
}

void App::record_key(WPARAM vk)
{
	GtkWidget *e = box_of(*this);
	if (e && vk == VK_RETURN && g_modified) {
		record_take(std::string(gtk_entry_get_text(GTK_ENTRY(e))));
		return;
	}
	sync_record_box(true);
	leave_record_box();
}

LRESULT App::record_color(HDC) { return 0; }  // (the colors are set in place_record_box)

}  // namespace ng
