// The words of a menu item where the menus have no access keys of Windows (macOS, Linux, Android): the mark of the
// key goes ("Game(&G)" -> "Game", "&File" -> "File", "A && B" -> "A & B").
#pragma once

#include <string>

namespace ng {

inline std::wstring menu_words(const std::wstring &text)
{
	std::wstring out;
	out.reserve(text.size());
	for (size_t i = 0; i < text.size(); ++i) {
		const wchar_t c = text[i];
		// "(&X)" as a whole
		if (c == L'(' && i + 3 < text.size() && text[i + 1] == L'&' && text[i + 2] != L'&' && text[i + 3] == L')') {
			i += 3;
			continue;
		}
		if (c == L'&') {
			if (i + 1 < text.size() && text[i + 1] == L'&') {
				out += L'&';
				++i;
			}
			continue;
		}
		out += c;
	}
	return out;
}

}  // namespace ng
