#include "paths.h"

#include <windows.h>

#include <iterator>

namespace ng {

std::optional<std::string> to_codepage(const std::wstring &w, unsigned code_page)
{
	BOOL lost = FALSE;
	const int n = WideCharToMultiByte(code_page, WC_NO_BEST_FIT_CHARS, w.c_str(), -1, nullptr, 0, nullptr, &lost);
	if (n <= 0 || lost) return std::nullopt;
	std::string s(static_cast<size_t>(n - 1), '\0');
	WideCharToMultiByte(code_page, WC_NO_BEST_FIT_CHARS, w.c_str(), -1, s.data(), n, nullptr, &lost);
	if (lost) return std::nullopt;
	return s;
}

std::optional<std::string> acp(const std::wstring &w)
{
	return to_codepage(w, CP_ACP);
}

// A name on the command line of an engine is quoted (spaces are allowed); the engine reads it in the code page of
// the system and keeps a book name of FILENAME_MAX - 8 bytes at most (a longer one is ignored: its own book is
// opened). Too long a name, or one with letters not in the code page, becomes its short (8.3) form when the drive
// keeps those; for a file still to be made, the folder's short form and the name.
std::optional<std::wstring> engine_arg_path(const std::wstring &path, size_t max_bytes)
{
	auto fits = [max_bytes](const std::wstring &w) {
		const auto a = acp(w);
		return a && a->size() <= max_bytes;
	};
	if (fits(path)) return path;
	wchar_t buf[MAX_PATH * 4];
	const DWORD size = static_cast<DWORD>(std::size(buf));
	DWORD n = GetShortPathNameW(path.c_str(), buf, size);
	if (n > 0 && n < size && fits(buf)) return std::wstring(buf);
	const size_t slash = path.find_last_of(L"\\/");
	if (slash == std::wstring::npos) return std::nullopt;
	n = GetShortPathNameW(path.substr(0, slash).c_str(), buf, size);
	if (n == 0 || n >= size) return std::nullopt;
	const std::wstring w = std::wstring(buf) + path.substr(slash);
	if (!fits(w)) return std::nullopt;
	return w;
}

// A name the engine can open: in the code page of the system and without a space (the engine reads a name up
// to a space). A name with spaces or other letters becomes its short (8.3) form when the drive keeps those;
// for a file still to be made, the folder's short form and the name.
std::optional<std::string> engine_path(const std::wstring &path, bool exists)
{
	auto plain = [](const std::wstring &w) -> std::optional<std::string> {
		if (w.find(L' ') != std::wstring::npos) return std::nullopt;
		return acp(w);
	};
	if (auto s = plain(path)) return s;
	wchar_t buf[MAX_PATH * 4];
	const DWORD size = static_cast<DWORD>(std::size(buf));
	if (exists) {
		const DWORD n = GetShortPathNameW(path.c_str(), buf, size);
		if (n == 0 || n >= size) return std::nullopt;
		return plain(buf);
	}
	const size_t slash = path.find_last_of(L"\\/");
	if (slash == std::wstring::npos) return std::nullopt;
	const DWORD n = GetShortPathNameW(path.substr(0, slash).c_str(), buf, size);
	if (n == 0 || n >= size) return std::nullopt;
	return plain(std::wstring(buf) + L"\\" + path.substr(slash + 1));
}

}  // namespace ng
