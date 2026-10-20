// Names of files for the engines: the engine opens names in the code page of the system and reads a name up to
// a space, so a name with spaces or other letters is given as its short (8.3) form.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ng {

// WideCharToMultiByte, or a stand-in for the tests
using WideToMulti = int(__stdcall *)(unsigned, unsigned long, const wchar_t *, int, char *, int, const char *, int *);
// the text in a code page (nothing when a letter is not there); `convert`: nullptr for WideCharToMultiByte
std::optional<std::string> to_codepage(const std::wstring &w, unsigned code_page, WideToMulti convert = nullptr);
// the code page the engines read names in: the code page of the system (not CP_ACP of the GUI, which is UTF-8)
unsigned engine_code_page();
// the name in the code page of the system (nothing when a letter is not there)
std::optional<std::string> acp(const std::wstring &w);
// text of an engine (its lines, and the commands made for it) to show: read in the code page of the engines
std::wstring engine_text(std::string_view s);
// UTF-8 text (a command of a script, typed, or kept in config.ini) for an engine: in the code page of the engines
// (nothing when a letter is not there)
std::optional<std::string> engine_command(std::string_view utf8);
// the name to give the engine; `exists`: the file is there (else only its folder); nothing when there is none
std::optional<std::string> engine_path(const std::wstring &path, bool exists);
// What the engines keep of a name given on their command line: a book name of FILENAME_MAX - 8 bytes at most (a
// longer one is ignored, and their own book is read); the other files are opened with the functions of the code page
// of the system, which take MAX_PATH - 1 letters whatever their bytes.
enum class EngineName { Book, File };
inline constexpr size_t kEngineBookNameMax = 252;  // bytes in the code page of the system
inline constexpr size_t kEngineNameMax = 259;      // letters
// a name for the command line of an engine (quoted there: spaces are allowed) that it keeps; nothing when there is none
// (a book keeps its own file name: only its folder may be given in the short form)
std::optional<std::wstring> engine_arg_path(const std::wstring &path, EngineName what);

}  // namespace ng
