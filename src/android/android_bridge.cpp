// src/android/android_bridge.h: the Java of the screen and the queue of the app.
#include "android_bridge.h"

#include <android/log.h>

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <deque>
#include <mutex>

#include "../app.h"

namespace ng {

namespace {

JavaVM *g_vm = nullptr;
jclass g_draw = nullptr, g_bridge = nullptr;

std::mutex g_mutex;
std::condition_variable g_wake;
std::deque<AndroidEvent> g_events;

AndroidState g_state;

[[noreturn]] void fail(const char *what, const char *name)
{
	__android_log_print(ANDROID_LOG_FATAL, "nikque-gui", "%s: %s", what, name);
	std::abort();
}

jmethodID method(jclass c, const char *name, const char *signature)
{
	JNIEnv *e = jni();
	const jmethodID m = e->GetStaticMethodID(c, name, signature);
	if (!m) fail("no Java function", name);
	return m;
}

}  // namespace

AndroidState &android_state() { return g_state; }

void push_event(const AndroidEvent &e)
{
	{
		std::lock_guard lock(g_mutex);
		// (the moves of a finger not yet taken are one move)
		if (e.kind == kEvTouchMove && !g_events.empty() && g_events.back().kind == kEvTouchMove) g_events.back() = e;
		else g_events.push_back(e);
	}
	g_wake.notify_one();
}

bool pop_event(AndroidEvent *e, long long wait_ms)
{
	std::unique_lock lock(g_mutex);
	if (g_events.empty() && wait_ms > 0) g_wake.wait_for(lock, std::chrono::milliseconds(wait_ms), [] { return !g_events.empty(); });
	if (g_events.empty()) return false;
	*e = g_events.front();
	g_events.pop_front();
	return true;
}

JNIEnv *jni()
{
	JNIEnv *e = nullptr;
	if (!g_vm || g_vm->GetEnv(reinterpret_cast<void **>(&e), JNI_VERSION_1_6) != JNI_OK || !e) fail("no Java on this thread", "jni");
	return e;
}

jclass draw_class() { return g_draw; }
jclass bridge_class() { return g_bridge; }
jmethodID draw_method(const char *name, const char *signature) { return method(g_draw, name, signature); }
jmethodID bridge_method(const char *name, const char *signature) { return method(g_bridge, name, signature); }

// (Java keeps text as UTF-16; wchar_t is a code point here)
jstring to_java(std::wstring_view s)
{
	std::u16string u;
	u.reserve(s.size());
	for (const wchar_t w : s) {
		char32_t c = static_cast<char32_t>(w);
		if (c >= 0x10000 && c <= 0x10ffff) {
			c -= 0x10000;
			u += static_cast<char16_t>(0xd800 + (c >> 10));
			u += static_cast<char16_t>(0xdc00 + (c & 0x3ff));
		} else {
			u += static_cast<char16_t>(c > 0xffff || (c >= 0xd800 && c < 0xe000) ? 0xfffd : c);
		}
	}
	return jni()->NewString(reinterpret_cast<const jchar *>(u.data()), static_cast<jsize>(u.size()));
}

std::wstring from_java(jstring s)
{
	std::wstring w;
	if (!s) return w;
	JNIEnv *e = jni();
	const jsize n = e->GetStringLength(s);
	const jchar *p = e->GetStringChars(s, nullptr);
	if (!p) return w;
	w.reserve(static_cast<size_t>(n));
	for (jsize i = 0; i < n; ++i) {
		char32_t c = p[i];
		if (c >= 0xd800 && c < 0xdc00 && i + 1 < n && p[i + 1] >= 0xdc00 && p[i + 1] < 0xe000) {
			c = 0x10000 + ((c - 0xd800) << 10) + (p[i + 1] - 0xdc00);
			++i;
		}
		w += static_cast<wchar_t>(c);
	}
	e->ReleaseStringChars(s, p);
	return w;
}

}  // namespace ng

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *)
{
	ng::g_vm = vm;
	JNIEnv *e = nullptr;
	if (vm->GetEnv(reinterpret_cast<void **>(&e), JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
	// (the classes of the app are found from the thread that loads the library; kept for the other threads)
	const jclass d = e->FindClass("io/github/nikque/gui/Draw"), b = e->FindClass("io/github/nikque/gui/Bridge");
	if (!d || !b) return JNI_ERR;
	ng::g_draw = static_cast<jclass>(e->NewGlobalRef(d));
	ng::g_bridge = static_cast<jclass>(e->NewGlobalRef(b));
	return JNI_VERSION_1_6;
}

JNIEXPORT jint JNICALL Java_io_github_nikque_gui_Bridge_run(JNIEnv *, jclass, jstring app_dir, jstring temp_dir, jfloat density)
{
	// (a write to an engine that has ended fails; it does not end the program)
	std::signal(SIGPIPE, SIG_IGN);
	ng::AndroidState &s = ng::android_state();
	s.app_dir = ng::from_java(app_dir);
	s.temp_dir = ng::from_java(temp_dir);
	s.density = density;
	int r;
	{
		ng::App app;
		r = app.run(nullptr, 0);
	}
	return r;
}

JNIEXPORT void JNICALL Java_io_github_nikque_gui_Bridge_event(JNIEnv *, jclass, jint kind, jint a, jint b, jint c)
{
	ng::push_event(ng::AndroidEvent{kind, a, b, c});
}

JNIEXPORT jobjectArray JNICALL Java_io_github_nikque_gui_Bridge_menu(JNIEnv *e, jclass)
{
	const std::vector<std::wstring> lines = ng::menu_lines(nullptr);
	const jclass str = e->FindClass("java/lang/String");
	const jobjectArray out = e->NewObjectArray(static_cast<jsize>(lines.size()), str, nullptr);
	for (size_t i = 0; i < lines.size(); ++i) {
		const jstring s = ng::to_java(lines[i]);
		e->SetObjectArrayElement(out, static_cast<jsize>(i), s);
		e->DeleteLocalRef(s);
	}
	return out;
}

}  // extern "C"
