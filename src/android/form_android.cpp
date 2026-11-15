// run_form on Android: a window of the screen with the fields in a column (Bridge.form) and OK / Cancel; the thread of
// the app waits for it. A number out of its range is said in a box and the window comes again with what was typed.
#include "../form.h"

#include <cwchar>

#include "../ini.h"
#include "../lang.h"
#include "android_bridge.h"

namespace ng {

namespace {

struct LocalFrame {
	LocalFrame() { jni()->PushLocalFrame(256); }
	~LocalFrame() { jni()->PopLocalFrame(nullptr); }
};

jobjectArray strings(JNIEnv *e, const std::vector<std::wstring> &v)
{
	const jobjectArray a = e->NewObjectArray(static_cast<jsize>(v.size()), e->FindClass("java/lang/String"), nullptr);
	for (size_t i = 0; i < v.size(); ++i) {
		const jstring s = to_java(v[i]);
		e->SetObjectArrayElement(a, static_cast<jsize>(i), s);
		e->DeleteLocalRef(s);
	}
	return a;
}

}  // namespace

bool run_form(NativeWindow, const std::wstring &title, std::vector<FormField> &fields)
{
	const std::wstring &ok = lang::t("form.ok"), &cancel = lang::t("form.cancel"), &bad_number = lang::t("form.bad_number");
	static const jmethodID form = bridge_method(
		"form", "(Ljava/lang/String;[I[Ljava/lang/String;[Ljava/lang/String;[I[I[Ljava/lang/String;[I[Z[Ljava/lang/String;"
		        "Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
	const jsize n = static_cast<jsize>(fields.size());
	for (;;) {
		std::wstring bad;
		{
			LocalFrame frame;
			JNIEnv *e = jni();
			std::vector<jint> kinds, lo, hi, choice;
			std::vector<jboolean> checked;
			std::vector<std::wstring> labels, texts, choices, notes;
			for (const FormField &f : fields) {
				// (the kinds as the screen knows them: Bridge.form)
				kinds.push_back(f.kind == FormField::Number ? 0
				                : f.kind == FormField::Text ? 1
				                : f.kind == FormField::Choice ? 2
				                : f.kind == FormField::Check ? 3
				                : f.kind == FormField::Heading ? 4
				                                               : 5);
				lo.push_back(f.lo);
				hi.push_back(f.hi);
				choice.push_back(f.choice);
				checked.push_back(f.checked ? JNI_TRUE : JNI_FALSE);
				labels.push_back(f.label);
				texts.push_back(f.text);
				notes.push_back(f.note);
				std::wstring all;
				for (size_t k = 0; k < f.choices.size(); ++k) all += (k ? L"\n" : L"") + f.choices[k];
				choices.push_back(all);
			}
			const jintArray jkinds = e->NewIntArray(n), jlo = e->NewIntArray(n), jhi = e->NewIntArray(n), jchoice = e->NewIntArray(n);
			const jbooleanArray jchecked = e->NewBooleanArray(n);
			e->SetIntArrayRegion(jkinds, 0, n, kinds.data());
			e->SetIntArrayRegion(jlo, 0, n, lo.data());
			e->SetIntArrayRegion(jhi, 0, n, hi.data());
			e->SetIntArrayRegion(jchoice, 0, n, choice.data());
			e->SetBooleanArrayRegion(jchecked, 0, n, checked.data());
			AndroidState &s = android_state();
			++s.modal;
			const jobjectArray r = static_cast<jobjectArray>(
				e->CallStaticObjectMethod(bridge_class(), form, to_java(title), jkinds, strings(e, labels), strings(e, texts), jlo, jhi,
			                              strings(e, choices), jchoice, jchecked, strings(e, notes), to_java(ok), to_java(cancel)));
			--s.modal;
			if (!r) return false;
			// what was in the controls (kept when the window comes again)
			for (jsize i = 0; i < n; ++i) {
				FormField &f = fields[static_cast<size_t>(i)];
				const std::wstring v = from_java(static_cast<jstring>(e->GetObjectArrayElement(r, i)));
				switch (f.kind) {
				case FormField::Number:
				case FormField::Text:
				case FormField::Slider: {
					f.text = v;
					if (f.kind == FormField::Text || !bad.empty()) break;
					wchar_t *end = nullptr;
					const long x = std::wcstol(f.text.c_str(), &end, 10);
					if (f.text.empty() || *end || x < f.lo || x > f.hi)
						bad = bad_number + L"\n" + f.label + L": " + std::to_wstring(f.lo) + L" .. " + std::to_wstring(f.hi);
					break;
				}
				case FormField::Choice: f.choice = static_cast<int>(std::wcstol(v.c_str(), nullptr, 10)); break;
				case FormField::Check: f.checked = v == L"1"; break;
				case FormField::Heading: break;
				}
			}
		}
		if (bad.empty()) return true;
		message_box(nullptr, bad, title, MB_ICONWARNING);
	}
}

}  // namespace ng
