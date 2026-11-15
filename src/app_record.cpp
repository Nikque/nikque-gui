// The row of the moves as text, right above the graph: a text box with the moves up to the position shown, and the
// buttons to copy and to paste. Pasted text loads at once, typed text with Enter; Esc or leaving the box for the board
// takes it back.
// The box itself is the system's own: src/win/app_record_win.cpp, src/mac/app_record_mac.mm.
#include "app.h"

namespace ng {

std::wstring App::record_text() const
{
	std::wstring s = widen(game_.to_text(true));
	if (upper_moves_)
		for (auto &c : s)
			if (c >= L'a' && c <= L'z') c = static_cast<wchar_t>(c - L'a' + L'A');
	return s;
}

bool App::record_take(const std::string &u)
{
	if (editing_ || !paste_text(u)) return false;
	sync_record_box(true);
	leave_record_box();
	return true;
}

}  // namespace ng
