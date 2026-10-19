// A small modal window of settings: a column of labels with a number, a text, a choice or a check box,
// and OK / Cancel. Built from code (no dialog resources), sized for the DPI of the owner.
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace ng {

struct FormField {
	enum Kind { Number, Text, Choice, Check, Heading } kind = Number;
	std::wstring label;
	std::wstring text;                  // Number and Text (in and out)
	int lo = 0, hi = 0;                 // Number: the range
	std::vector<std::wstring> choices;  // Choice
	int choice = 0;                     // Choice (in and out)
	bool checked = false;               // Check (in and out)
	std::wstring note;                  // a hint on the right
	HWND control = nullptr;             // set while the window is open
};

// Shows the window; returns true when OK was pressed (and every number is in its range).
bool run_form(HWND owner, const std::wstring &title, std::vector<FormField> &fields, const std::wstring &ok,
              const std::wstring &cancel, const std::wstring &bad_number);

}  // namespace ng
