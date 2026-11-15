// Linux (GTK 3): the main window and what its parts share. The App draws into a picture kept by Gfx
// (src/linux/gfx_linux.cpp); the drawing area puts that picture on the screen, and hands the events of the mouse, the
// keys, the menus and the timers to the App (src/linux/app_linux.cpp). NativeWindow is the drawing area.
#pragma once

#include <gtk/gtk.h>

#include <cstdint>
#include <map>
#include <string>

namespace ng {

class App;

struct LinuxWindow {
	App *app = nullptr;
	// a message posted to the window and a command of the menus, handed to the App (set by src/linux/app_linux.cpp:
	// src/linux/platform_linux.cpp is also in the tests, which have no App)
	void (*message)(unsigned msg, std::uintptr_t wp, std::intptr_t lp) = nullptr;
	void (*command)(int id) = nullptr;
	GtkWidget *window = nullptr;   // the GtkWindow
	GtkWidget *box = nullptr;      // the menu bar above the view
	GtkWidget *menu_bar = nullptr;
	GtkWidget *overlay = nullptr;  // the drawing area, the text box of the moves over it
	GtkWidget *area = nullptr;
	GtkWidget *record = nullptr;   // the text box of the moves (a GtkEntry)
	GdkRectangle record_at{0, 0, 0, 0};
	bool needs_paint = true;       // the App draws the whole window when the view is drawn next (App::paint)
	std::map<std::uintptr_t, guint> timers;
	int modal = 0;                 // modal windows open over the main window
	bool popup_open = false;       // a small menu is open: its command is returned, not run
	int popup_choice = 0;
	GMainLoop *popup_loop = nullptr;
	// where the window is when it is not maximized (kept for config.ini)
	int x = 0, y = 0, w = 0, h = 0;
	bool maximized = false, minimized = false;
	// the click before (a double click is told by its time and place)
	guint32 click_time = 0;
	double click_x = 0, click_y = 0;
	double wheel = 0;              // the fine scrolling not yet given
};

// the window of the program (null before it is made and after it has gone)
LinuxWindow *linux_window();
void set_linux_window(LinuxWindow *w);

// a modal window is open over the main window (App::window_enabled is false meanwhile)
void modal_begin();
void modal_end();
bool modal_open();
// the window a dialog is shown over
GtkWindow *dialog_parent();

std::string gtk_text(const std::wstring &w);  // UTF-8
// a command of a menu: run by the App, or kept as the choice of the small menu that is open
void menu_command(int id);

}  // namespace ng
