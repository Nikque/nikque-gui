// Names of files for the engines: the engine opens names in the code page of the system and reads a name up to
// a space, so a name with spaces or other letters is given as its short (8.3) form.
#pragma once

#include <optional>
#include <string>

namespace ng {

// the name in the code page of the system (nothing when a letter is not there)
std::optional<std::string> acp(const std::wstring &w);
// the name to give the engine; `exists`: the file is there (else only its folder); nothing when there is none
std::optional<std::string> engine_path(const std::wstring &path, bool exists);

}  // namespace ng
