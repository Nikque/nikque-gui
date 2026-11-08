#include "paths.h"

#include <windows.h>

#include <iterator>

namespace ng {

namespace {

// the text in a code page as wide letters (none when it cannot be read with `flags`, or is empty)
std::optional<std::wstring> from_codepage(std::string_view s, unsigned code_page, unsigned long flags)
{
	const int n = MultiByteToWideChar(code_page, flags, s.data(), static_cast<int>(s.size()), nullptr, 0);
	if (n <= 0) return std::nullopt;
	std::wstring w(static_cast<size_t>(n), L'\0');
	MultiByteToWideChar(code_page, flags, s.data(), static_cast<int>(s.size()), w.data(), n);
	return w;
}

// the short (8.3) form of the name of a file or a folder that is there (none: no short form, or too long)
std::optional<std::wstring> short_form(const std::wstring &path)
{
	wchar_t buf[MAX_PATH * 4];
	const DWORD size = static_cast<DWORD>(std::size(buf));
	const DWORD n = GetShortPathNameW(path.c_str(), buf, size);
	if (n == 0 || n >= size) return std::nullopt;
	return std::wstring(buf);
}

}  // namespace

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

unsigned engine_code_page()
{
	// the code page of the system (Nls\CodePage\ACP); CP_ACP of this program is UTF-8 (its manifest)
	static const unsigned cp = [] {
		wchar_t value[16] = L"";
		DWORD size = sizeof value;
		if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Nls\\CodePage", L"ACP", RRF_RT_REG_SZ,
		                 nullptr, value, &size) == ERROR_SUCCESS) {
			const unsigned v = static_cast<unsigned>(wcstoul(value, nullptr, 10));
			if (v && IsValidCodePage(v)) return v;
		}
		return static_cast<unsigned>(GetACP());
	}();
	return cp;
}

std::optional<std::string> acp(const std::wstring &w)
{
	return to_codepage(w, engine_code_page());
}

std::wstring engine_text(std::string_view s)
{
	if (auto w = from_codepage(s, engine_code_page(), 0)) return *std::move(w);
	return std::wstring(s.begin(), s.end());
}

std::optional<std::string> engine_command(std::string_view utf8)
{
	if (utf8.empty()) return std::string();
	const auto w = from_codepage(utf8, CP_UTF8, MB_ERR_INVALID_CHARS);
	if (!w) return std::nullopt;
	return acp(*w);
}

// A name on the command line of an engine is quoted (spaces are allowed); the engine reads it in the code page of the
// system. A name it would not keep (too long, or with letters not in the code page) is given with its folder in the
// short (8.3) form when the drive keeps those. The other files may also be given in the short form whole; a book may
// not: the engine saves a book by writing another file and renaming it to the name it was given (the book would come
// back under its short name), and names the files it saves on the way after that name (".store", ".dev2", ...).
std::optional<std::wstring> engine_arg_path(const std::wstring &path, EngineName what)
{
	auto fits = [what](const std::wstring &w) {
		const auto a = acp(w);
		return a && (what == EngineName::Book ? a->size() <= kEngineBookNameMax : w.size() <= kEngineNameMax);
	};
	if (fits(path)) return path;
	if (const size_t slash = path.find_last_of(L"\\/"); slash != std::wstring::npos) {
		if (const auto dir = short_form(path.substr(0, slash))) {
			const std::wstring w = *dir + path.substr(slash);
			if (fits(w)) return w;
		}
	}
	if (what == EngineName::Book) return std::nullopt;
	const auto whole = short_form(path);  // (a file still to be made has none)
	if (whole && fits(*whole)) return whole;
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
	if (exists) {
		const auto whole = short_form(path);
		return whole ? plain(*whole) : std::nullopt;
	}
	const size_t slash = path.find_last_of(L"\\/");
	if (slash == std::wstring::npos) return std::nullopt;
	const auto dir = short_form(path.substr(0, slash));
	if (!dir) return std::nullopt;
	return plain(*dir + L"\\" + path.substr(slash + 1));
}

}  // namespace ng
