// src/paths.h on macOS: the engines read the names of files in UTF-8 (the names of the system), so a name is given as it
// is; a name with a space cannot be given in a command (the engine reads a name up to a space), and then goes through
// the temporary folder (App::engine_path_or_say). There are no short (8.3) names.
#include "paths.h"

#include <climits>

#include "ini.h"
#include "platform.h"

namespace ng {

std::optional<std::string> to_codepage(const std::wstring &w, unsigned, WideToMulti) { return narrow(w); }

unsigned engine_code_page() { return 65001; }

std::optional<std::string> acp(const std::wstring &w) { return narrow(w); }

std::wstring engine_text(std::string_view s) { return widen(s); }

std::optional<std::string> engine_command(std::string_view utf8)
{
	if (utf8.empty()) return std::string();
	if (!decode_text(utf8, 65001)) return std::nullopt;
	return std::string(utf8);
}

std::optional<std::wstring> engine_arg_path(const std::wstring &path, EngineName what)
{
	// (quoted on the command line; the engine keeps a book name of FILENAME_MAX - 8 bytes, the other names of PATH_MAX)
	const size_t bytes = narrow(path).size();
	if (what == EngineName::Book ? bytes <= FILENAME_MAX - 8 : bytes < PATH_MAX) return path;
	return std::nullopt;
}

std::optional<std::string> engine_path(const std::wstring &path, bool)
{
	if (path.find(L' ') != std::wstring::npos) return std::nullopt;
	return narrow(path);
}

}  // namespace ng
