#include "paths.h"

#include <windows.h>

#include <iterator>

namespace ng {

std::optional<std::string> acp(const std::wstring &w)
{
	BOOL lost = FALSE;
	const int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), -1, nullptr, 0, nullptr, &lost);
	if (n <= 0 || lost) return std::nullopt;
	std::string s(static_cast<size_t>(n - 1), '\0');
	WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), -1, s.data(), n, nullptr, &lost);
	if (lost) return std::nullopt;
	return s;
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
