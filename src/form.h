// A small modal window of settings: a column of labels with a number, a text, a choice, a check box or a number
// with a slider, and OK / Cancel. Built from code (no dialog resources), sized for the DPI of the owner.
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace ng {

struct FormField {
	enum Kind { Number, Text, Choice, Check, Heading, Slider } kind = Number;
	std::wstring label;
	std::wstring text;                  // Number, Text and Slider (in and out)
	int lo = 0, hi = 0;                 // Number and Slider: the range
	std::vector<std::wstring> choices;  // Choice
	int choice = 0;                     // Choice (in and out)
	bool checked = false;               // Check (in and out)
	std::wstring note;                  // a hint on the right
	HWND control = nullptr;             // set while the window is open
	HWND slider = nullptr;              // Slider: the slider beside the number
};

// a field of a number from lo to hi (the note: a hint on the right)
FormField number_field(const std::wstring &label, int value, int lo, int hi, const std::wstring &note = {});
// a number from lo to hi with a slider beside it
FormField slider_field(const std::wstring &label, int value, int lo, int hi, const std::wstring &note = {});
// a text
FormField text_field(const std::wstring &label, const std::wstring &text, const std::wstring &note = {});
// one of `choices`, `choice` chosen at first
FormField choice_field(const std::wstring &label, std::vector<std::wstring> choices, int choice, const std::wstring &note = {});
// a check box
FormField check_field(const std::wstring &label, bool checked, const std::wstring &note = {});

// Shows the window; returns true when OK was pressed (and every number is in its range). The buttons and the warning
// of a number out of its range are in the language of the app.
bool run_form(HWND owner, const std::wstring &title, std::vector<FormField> &fields);

}  // namespace ng
