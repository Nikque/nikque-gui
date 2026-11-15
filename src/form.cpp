// The fields of a small window of settings (run_form is the system's own: src/win/form_win.cpp, src/mac/form_mac.mm).
#include "form.h"

namespace ng {

FormField number_field(const std::wstring &label, int value, int lo, int hi, const std::wstring &note)
{
	FormField f;
	f.kind = FormField::Number;
	f.label = label;
	f.text = std::to_wstring(value);
	f.lo = lo;
	f.hi = hi;
	f.note = note;
	return f;
}

FormField slider_field(const std::wstring &label, int value, int lo, int hi, const std::wstring &note)
{
	FormField f = number_field(label, value, lo, hi, note);
	f.kind = FormField::Slider;
	return f;
}

FormField text_field(const std::wstring &label, const std::wstring &text, const std::wstring &note)
{
	FormField f;
	f.kind = FormField::Text;
	f.label = label;
	f.text = text;
	f.note = note;
	return f;
}

FormField choice_field(const std::wstring &label, std::vector<std::wstring> choices, int choice, const std::wstring &note)
{
	FormField f;
	f.kind = FormField::Choice;
	f.label = label;
	f.choices = std::move(choices);
	f.choice = choice;
	f.note = note;
	return f;
}

FormField check_field(const std::wstring &label, bool checked, const std::wstring &note)
{
	FormField f;
	f.kind = FormField::Check;
	f.label = label;
	f.checked = checked;
	f.note = note;
	return f;
}

}  // namespace ng
