#include "paths.h"

#include <windows.h>

#include <iterator>

namespace ng {

std::optional<std::string> to_codepage(const std::wstring &w, unsigned code_page, WideToMulti convert)
{
	if (!convert) convert = WideCharToMultiByte;
	// UTF-8 (the code page of the system too when Windows is set to use it) takes every letter, and takes neither the
	// flag of the best fit nor the place that tells a lost letter (the call may fail with them): only a broken letter
	// (half of a pair) is refused
	const bool utf8 = (code_page == CP_ACP ? GetACP() : code_page) == CP_UTF8;
	const unsigned long flags = utf8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
	BOOL lost = FALSE;
	BOOL *const tell = utf8 ? nullptr : &lost;
	const int n = convert(code_page, flags, w.c_str(), -1, nullptr, 0, nullptr, tell);
	if (n <= 0 || lost) return std::nullopt;
	std::string s(static_cast<size_t>(n - 1), '\0');
	if (convert(code_page, flags, w.c_str(), -1, s.data(), n, nullptr, tell) <= 0 || lost) return std::nullopt;
	return s;
}

std::optional<std::string> acp(const std::wstring &w)
{
	return to_codepage(w, CP_ACP);
}

// A name on the command line of an engine is quoted (spaces are allowed); the engine reads it in the code page of the
// system. A name it would not keep (too long, or with letters not in the code page) is given with its folder in the
// short (8.3) form when the drive keeps those, else in the short form whole: the name of the file stays when it can,
// since the engine names the files it saves on the way after the book (".store", ".dev2", ...).
std::optional<std::wstring> engine_arg_path(const std::wstring &path, EngineName what)
{
	auto fits = [what](const std::wstring &w) {
		const auto a = acp(w);
		return a && (what == EngineName::Book ? a->size() <= kEngineBookNameMax : w.size() <= kEngineNameMax);
	};
	if (fits(path)) return path;
	wchar_t buf[MAX_PATH * 4];
	const DWORD size = static_cast<DWORD>(std::size(buf));
	if (const size_t slash = path.find_last_of(L"\\/"); slash != std::wstring::npos) {
		const DWORD n = GetShortPathNameW(path.substr(0, slash).c_str(), buf, size);
		if (n > 0 && n < size) {
			const std::wstring w = std::wstring(buf) + path.substr(slash);
			if (fits(w)) return w;
		}
	}
	const DWORD n = GetShortPathNameW(path.c_str(), buf, size);  // (a file still to be made has none)
	if (n > 0 && n < size && fits(buf)) return std::wstring(buf);
	return std::nullopt;
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
