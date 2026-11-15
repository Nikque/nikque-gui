// Android: between the C++ of the GUI and the Java of the screen (android/app/src/main/java/io/github/nikque/gui).
// The App runs on a thread of its own (Bridge.run): it takes the events of the screen, the messages posted to the
// window and its timers from one queue, draws with the calls of Draw, and asks the screen for the windows that wait
// for an answer. NativeWindow is a token (the App has one window).
#pragma once

#include <jni.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ng {

// the kinds of Bridge.event (as in Bridge.java)
enum : int {
	kEvResize = 1,
	kEvTouchDown,
	kEvTouchMove,
	kEvTouchUp,
	kEvRightClick,
	kEvWheel,
	kEvCommand,
	kEvKey,
	kEvActive,
	kEvClose,
	kEvSurface,
	kEvMessage = 100,  // a message posted to the window (post_message)
};

struct AndroidEvent {
	int kind = 0;
	std::intptr_t a = 0, b = 0, c = 0;
};

// the queue of the app: an event from any thread; the next one for the thread of the app, waited for up to `wait_ms`
// (false: none came)
void push_event(const AndroidEvent &e);
bool pop_event(AndroidEvent *e, long long wait_ms);

struct AndroidState {
	std::wstring app_dir;   // the program as the App sees it (a name in the folder of the files of the app)
	std::wstring temp_dir;  // with '/' at its end
	float density = 1;
	int width = 0, height = 0;   // the view, in pixels
	bool surface = false;        // the view has a surface to draw on
	bool active = true;          // the screen is in front
	bool needs_paint = false;    // the App draws the whole window soon (invalidate_window)
	bool quit = false;           // the window has gone (destroy_window)
	int modal = 0;               // windows that wait for an answer are open
	bool ctrl = false, shift = false;  // held with the key being handled
};
AndroidState &android_state();

// the Java of this thread (the thread of the app, or one that calls a native function)
JNIEnv *jni();
// the classes Draw and Bridge and a static function of one of them (kept; the program ends when there is none)
jclass draw_class();
jclass bridge_class();
jmethodID draw_method(const char *name, const char *signature);
jmethodID bridge_method(const char *name, const char *signature);

// a text for Java and back (the caller deletes the local reference of to_java)
jstring to_java(std::wstring_view s);
std::wstring from_java(jstring s);

// the lines of a menu for the screen ("<depth>\t<command>\t<flags>\t<words>"); null: the bar of the window
std::vector<std::wstring> menu_lines(void *menu);

}  // namespace ng
