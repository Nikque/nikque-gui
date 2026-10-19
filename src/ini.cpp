#include "ini.h"

#include <windows.h>

#include <charconv>
#include <cstdio>

namespace ng {

std::string trim(std::string_view s)
{
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
	return std::string(s.substr(a, b - a));
}

bool iequals(std::string_view a, std::string_view b) noexcept
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i) {
		char x = a[i], y = b[i];
		if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
		if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
		if (x != y) return false;
	}
	return true;
}

std::wstring widen(std::string_view s)
{
	if (s.empty()) return {};
	const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
	return w;
}

std::string narrow(std::wstring_view w)
{
	if (w.empty()) return {};
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
	return s;
}

bool Ini::load(const std::wstring &path)
{
	lines_.clear();
	dirty_ = false;
	FILE *f = nullptr;
	if (_wfopen_s(&f, path.c_str(), L"rb") != 0) f = nullptr;
	if (!f) return false;
	std::string all;
	char buf[16384];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0) all.append(buf, n);
	fclose(f);
	size_t i = 0;
	if (all.size() >= 3 && all.compare(0, 3, "\xEF\xBB\xBF") == 0) i = 3;
	while (i <= all.size()) {
		size_t e = all.find('\n', i);
		if (e == std::string::npos) e = all.size();
		std::string line = all.substr(i, e - i);
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (e < all.size() || !line.empty()) lines_.push_back(std::move(line));
		i = e + 1;
	}
	return true;
}

bool Ini::save(const std::wstring &path)
{
	const std::wstring tmp = path + L".tmp";
	FILE *f = nullptr;
	if (_wfopen_s(&f, tmp.c_str(), L"wb") != 0) f = nullptr;
	if (!f) return false;
	bool ok = true;
	for (const auto &l : lines_) {
		ok = ok && fwrite(l.data(), 1, l.size(), f) == l.size();
		ok = ok && fwrite("\r\n", 1, 2, f) == 2;
	}
	ok = (fclose(f) == 0) && ok;
	if (!ok || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileW(tmp.c_str());
		return false;
	}
	dirty_ = false;
	return true;
}

static bool section_header(std::string_view line, std::string *name)
{
	const std::string t = trim(line);
	if (t.size() < 2 || t.front() != '[' || t.back() != ']') return false;
	*name = trim(std::string_view(t).substr(1, t.size() - 2));
	return true;
}

static bool key_value(std::string_view line, std::string *key, std::string *value)
{
	const std::string t = trim(line);
	if (t.empty() || t[0] == ';' || t[0] == '#') return false;
	const size_t eq = t.find('=');
	if (eq == std::string::npos) return false;
	*key = trim(std::string_view(t).substr(0, eq));
	if (value) *value = trim(std::string_view(t).substr(eq + 1));
	return true;
}

int Ini::find(std::string_view section, std::string_view key, int *end) const
{
	bool in = false;
	int found = -1;
	if (end) *end = -1;
	std::string name, k;
	for (int i = 0; i < static_cast<int>(lines_.size()); ++i) {
		if (section_header(lines_[i], &name)) {
			in = iequals(name, section);
			if (in && end) *end = i + 1;
			continue;
		}
		if (!in) continue;
		if (key_value(lines_[i], &k, nullptr)) {
			if (end) *end = i + 1;
			if (found < 0 && iequals(k, key)) found = i;
		}
	}
	return found;
}

std::string Ini::get(std::string_view section, std::string_view key, std::string_view def) const
{
	const int i = find(section, key, nullptr);
	if (i < 0) return std::string(def);
	std::string k, v;
	key_value(lines_[i], &k, &v);
	return v;
}

int Ini::get_int(std::string_view section, std::string_view key, int def, int lo, int hi) const
{
	const std::string v = get(section, key);
	int x = def;
	if (!v.empty()) {
		const auto r = std::from_chars(v.data(), v.data() + v.size(), x);
		if (r.ec != std::errc()) x = def;
	}
	return x < lo ? lo : x > hi ? hi : x;
}

bool Ini::get_bool(std::string_view section, std::string_view key, bool def) const
{
	const std::string v = get(section, key);
	if (v.empty()) return def;
	return v == "1" || iequals(v, "on") || iequals(v, "true") || iequals(v, "yes");
}

void Ini::set(std::string_view section, std::string_view key, std::string_view value)
{
	int end;
	const int i = find(section, key, &end);
	std::string line = std::string(key) + "=" + std::string(value);
	if (i >= 0) {
		if (lines_[i] != line) {
			lines_[i] = std::move(line);
			dirty_ = true;
		}
		return;
	}
	if (end < 0) {
		if (!lines_.empty() && !trim(lines_.back()).empty()) lines_.emplace_back();
		lines_.push_back("[" + std::string(section) + "]");
		lines_.push_back(std::move(line));
	} else {
		lines_.insert(lines_.begin() + end, std::move(line));
	}
	dirty_ = true;
}

void Ini::set_int(std::string_view section, std::string_view key, int value)
{
	set(section, key, std::to_string(value));
}

std::vector<std::pair<std::string, std::string>> Ini::section(std::string_view section) const
{
	std::vector<std::pair<std::string, std::string>> out;
	bool in = false;
	std::string name, k, v;
	for (const auto &l : lines_) {
		if (section_header(l, &name)) {
			in = iequals(name, section);
			continue;
		}
		if (in && key_value(l, &k, &v)) out.emplace_back(k, v);
	}
	return out;
}

}  // namespace ng
