#include "lang.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <cstdio>
#include <string>
#include <unordered_map>

#include "ini.h"
#include "platform.h"
#include "resource.h"

namespace ng::lang {

namespace {

struct Hash {
	using is_transparent = void;
	size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
};
std::unordered_map<std::string, std::wstring, Hash, std::equal_to<>> words;
std::string loaded = "ja";

// "\n" in a value is a new line
std::wstring unescape(std::string_view v)
{
	std::string s;
	s.reserve(v.size());
	for (size_t i = 0; i < v.size(); ++i) {
		if (v[i] == '\\' && i + 1 < v.size() && v[i + 1] == 'n') {
			s += '\n';
			++i;
		} else {
			s += v[i];
		}
	}
	return widen(s);
}

void read_text(std::string_view text)
{
	size_t i = 0;
	if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") i = 3;
	while (i < text.size()) {
		size_t e = text.find('\n', i);
		if (e == std::string_view::npos) e = text.size();
		const std::string line = trim(text.substr(i, e - i));
		i = e + 1;
		if (line.empty() || line[0] == '#') continue;
		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		words[trim(std::string_view(line).substr(0, eq))] = unescape(trim(std::string_view(line).substr(eq + 1)));
	}
}

}  // namespace

void load(const std::wstring &exe_dir, std::string_view code)
{
	words.clear();
	loaded = std::string(code);
	// Japanese in the exe first, then the file over it
#if defined(_WIN32)
	if (HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_LANG_JA), RT_RCDATA)) {
		if (HGLOBAL g = LoadResource(nullptr, r)) {
			const char *p = static_cast<const char *>(LockResource(g));
			read_text(std::string_view(p, SizeofResource(nullptr, r)));
		}
	}
#else
	// (lang/ja.txt made into a C++ string by the build: CMakeLists.txt)
	static constexpr unsigned char kJapanese[] = {
#include "lang_ja.inc"
	};
	read_text(std::string_view(reinterpret_cast<const char *>(kJapanese), sizeof kJapanese));
#endif
	const std::wstring path = path_join(exe_dir, L"lang\\" + widen(code) + L".txt");
	if (std::string all; read_file(path, &all)) read_text(all);
}

const std::wstring &t(std::string_view key)
{
	const auto it = words.find(key);
	if (it != words.end()) return it->second;
	return words.emplace(std::string(key), widen(key)).first->second;
}

std::wstring f(std::string_view key, std::initializer_list<std::wstring_view> args)
{
	const std::wstring &w = t(key);
	std::wstring out;
	out.reserve(w.size() + 16);
	for (size_t i = 0; i < w.size(); ++i) {
		if (w[i] == L'{' && i + 2 < w.size() && w[i + 2] == L'}' && w[i + 1] >= L'0' && w[i + 1] <= L'9') {
			const size_t k = w[i + 1] - L'0';
			if (k < args.size()) out += *(args.begin() + k);
			i += 2;
		} else {
			out += w[i];
		}
	}
	return out;
}

const std::string &code()
{
	return loaded;
}

std::wstring number(long long n)
{
	// (unsigned: -n of the smallest number is not a long long)
	const unsigned long long u = static_cast<unsigned long long>(n);
	const std::wstring digits = std::to_wstring(n < 0 ? 0 - u : u);
	std::wstring out = n < 0 ? L"-" : L"";
	for (size_t i = 0; i < digits.size(); ++i) {
		if (i > 0 && (digits.size() - i) % 3 == 0) out += L',';
		out += digits[i];
	}
	return out;
}

const wchar_t *ui_font()
{
#if defined(_WIN32)
	if (loaded == "en") return L"Segoe UI";
	if (loaded == "zh-Hans") return L"Microsoft YaHei UI";
	if (loaded == "zh-Hant") return L"Microsoft JhengHei UI";
	return L"Yu Gothic UI";
#else
	if (loaded == "en") return L"Helvetica Neue";
	if (loaded == "zh-Hans") return L"PingFang SC";
	if (loaded == "zh-Hant") return L"PingFang TC";
	return L"Hiragino Sans";
#endif
}

const wchar_t *locale()
{
	if (loaded == "en") return L"en-us";
	if (loaded == "zh-Hans") return L"zh-cn";
	if (loaded == "zh-Hant") return L"zh-tw";
	return L"ja-jp";
}

}  // namespace ng::lang
