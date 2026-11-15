// What the shared POSIX parts (src/posix) ask of the system they are built for (src/linux, src/android).
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ng {

// text in a code page other than UTF-8 (932 Shift_JIS, ...); nothing when it is not text of that code page, or when the
// system cannot read that code page
std::optional<std::wstring> decode_codepage(std::string_view bytes, unsigned code_page);

}  // namespace ng
