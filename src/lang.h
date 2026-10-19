// The words on the screen, read from lang\<code>.txt ("key=value", UTF-8). Japanese is also in the exe.
#pragma once

#include <initializer_list>
#include <string>
#include <string_view>

namespace ng::lang {

// reads lang\<code>.txt next to the exe; keys missing there come from the Japanese in the exe
void load(const std::wstring &exe_dir, std::string_view code);
// the words for a key (the key itself when it is unknown)
// the font of the screen and the forms, and the locale of the text, for the language loaded
const wchar_t *ui_font();
const wchar_t *locale();
const std::string &code();
const std::wstring &t(std::string_view key);
// replaces {0}, {1}, ... in the words for a key
std::wstring f(std::string_view key, std::initializer_list<std::wstring_view> args);

}  // namespace ng::lang
