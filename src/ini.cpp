#include "ini.h"

#include "platform.h"

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

namespace {

// A to Z as a to z, the other bytes as they are
char lower_char(char c) noexcept { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

}  // namespace

bool iequals(std::string_view a, std::string_view b) noexcept
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (lower_char(a[i]) != lower_char(b[i])) return false;
	return true;
}

std::string lower(std::string s)
{
	for (auto &c : s) c = lower_char(c);
	return s;
}

bool read_file(const std::wstring &path, std::string *out)
{
	FILE *f = open_file(path, L"rb");
	if (!f) return false;
	out->clear();
	char buf[16384];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0) out->append(buf, n);
	fclose(f);
	return true;
}

bool write_file(const std::wstring &path, std::string_view data)
{
	const std::wstring tmp = path + L".tmp";
	FILE *f = open_file(tmp, L"wb");
	if (!f) return false;
	bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
	ok = (fclose(f) == 0) && ok;
	if (!ok || !move_file(tmp, path)) {
		delete_file(tmp);
		return false;
	}
	return true;
}

#if defined(_WIN32)

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

#else

// wchar_t is a code point (UTF-32); a byte that does not make UTF-8 becomes U+FFFD, as on Windows
std::wstring widen(std::string_view s)
{
	std::wstring w;
	w.reserve(s.size());
	for (size_t i = 0; i < s.size();) {
		const unsigned char c = static_cast<unsigned char>(s[i]);
		int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
		char32_t cp = len == 1 ? c : len == 2 ? c & 0x1f : len == 3 ? c & 0x0f : c & 0x07;
		bool ok = len > 0 && i + len <= s.size();
		for (int k = 1; ok && k < len; ++k) {
			const unsigned char d = static_cast<unsigned char>(s[i + k]);
			if ((d >> 6) != 2) ok = false;
			else cp = (cp << 6) | (d & 0x3f);
		}
		static constexpr char32_t kMin[5] = {0, 0, 0x80, 0x800, 0x10000};
		if (ok && (cp < kMin[len] || cp > 0x10ffff || (cp >= 0xd800 && cp < 0xe000))) ok = false;
		if (!ok) {
			w += static_cast<wchar_t>(0xfffd);
			++i;
			continue;
		}
		w += static_cast<wchar_t>(cp);
		i += len;
	}
	return w;
}

std::string narrow(std::wstring_view w)
{
	std::string s;
	s.reserve(w.size());
	for (const wchar_t wc : w) {
		char32_t c = static_cast<char32_t>(wc);
		if (c > 0x10ffff || (c >= 0xd800 && c < 0xe000)) c = 0xfffd;
		if (c < 0x80) {
			s += static_cast<char>(c);
		} else if (c < 0x800) {
			s += static_cast<char>(0xc0 | (c >> 6));
			s += static_cast<char>(0x80 | (c & 0x3f));
		} else if (c < 0x10000) {
			s += static_cast<char>(0xe0 | (c >> 12));
			s += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
			s += static_cast<char>(0x80 | (c & 0x3f));
		} else {
			s += static_cast<char>(0xf0 | (c >> 18));
			s += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
			s += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
			s += static_cast<char>(0x80 | (c & 0x3f));
		}
	}
	return s;
}

#endif

bool Ini::load(const std::wstring &path)
{
	lines_.clear();
	dirty_ = false;
	std::string all;
	if (!read_file(path, &all)) return false;
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
	std::string text;  // (every line with CRLF, the last one too)
	for (const auto &l : lines_) {
		text += l;
		text += "\r\n";
	}
	if (!write_file(path, text)) return false;
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

bool Ini::rename_section(std::string_view from, std::string_view to)
{
	std::string name;
	for (const auto &l : lines_)
		if (section_header(l, &name) && iequals(name, to)) return false;
	bool renamed = false;
	for (auto &l : lines_)
		if (section_header(l, &name) && iequals(name, from)) {
			l = "[" + std::string(to) + "]";
			renamed = true;
		}
	if (renamed) dirty_ = true;
	return renamed;
}

}  // namespace ng
