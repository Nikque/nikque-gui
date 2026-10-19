#include "openings.h"

#include <windows.h>

#include <cstdio>

#include "ini.h"

namespace ng {

namespace {

// the text of the file: UTF-8 when it is valid UTF-8, else Shift_JIS (code page 932)
std::wstring decode(const std::string &bytes)
{
	size_t i = bytes.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
	const char *p = bytes.data() + i;
	const int len = static_cast<int>(bytes.size() - i);
	if (len <= 0) return {};
	for (const UINT cp : {static_cast<UINT>(CP_UTF8), 932u}) {
		const int n = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, p, len, nullptr, 0);
		if (n <= 0) continue;
		std::wstring w(n, L'\0');
		MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, p, len, w.data(), n);
		return w;
	}
	return widen(std::string_view(p, len));
}

std::wstring trim_w(std::wstring_view s)
{
	size_t a = 0, b = s.size();
	auto space = [](wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == 0x3000; };
	while (a < b && space(s[a])) ++a;
	while (b > a && space(s[b - 1])) --b;
	return std::wstring(s.substr(a, b - a));
}

}  // namespace

int Openings::load(const std::wstring &path)
{
	names_.clear();
	FILE *f = nullptr;
	if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return 0;
	std::string all;
	char buf[16384];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0) all.append(buf, n);
	fclose(f);
	const std::wstring text = decode(all);
	int count = 0;
	size_t i = 0;
	while (i < text.size()) {
		size_t e = text.find(L'\n', i);
		if (e == std::wstring::npos) e = text.size();
		// the leading spaces (also full-width) only show the levels
		const std::wstring line = trim_w(std::wstring_view(text).substr(i, e - i));
		i = e + 1;
		if (line.empty() || line.starts_with(L"//")) continue;
		const size_t eq = line.rfind(L'=');
		if (eq == std::wstring::npos || eq == 0) continue;
		const std::wstring name = trim_w(std::wstring_view(line).substr(0, eq));
		const ParsedMoves pm = parse_moves(narrow(std::wstring_view(line).substr(eq + 1)));
		if (pm.moves.empty() || name.empty()) continue;
		Game g;
		if (replay(g, Position{}, pm.moves) != static_cast<int>(pm.moves.size())) continue;  // not a legal line
		for (int s = 0; s < 8; ++s) names_.try_emplace(key_of(symmetric(g.now(), s)), name);
		++count;
	}
	return count;
}

const std::wstring *Openings::name_at(const Game &g) const
{
	if (names_.empty()) return nullptr;
	for (int k = g.cursor(); k >= 0; --k) {
		auto it = names_.find(key_of(g.at(k)));
		if (it != names_.end()) return &it->second;
	}
	return nullptr;
}

}  // namespace ng
