// config.ini: sections and "key=value" lines in UTF-8. Lines are kept as they are (comments too);
// a value set again replaces only its own line.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ng {

class Ini {
public:
	bool load(const std::wstring &path);
	bool save(const std::wstring &path);
	bool dirty() const noexcept { return dirty_; }

	std::string get(std::string_view section, std::string_view key, std::string_view def = {}) const;
	int get_int(std::string_view section, std::string_view key, int def, int lo, int hi) const;
	bool get_bool(std::string_view section, std::string_view key, bool def) const;
	void set(std::string_view section, std::string_view key, std::string_view value);
	void set_int(std::string_view section, std::string_view key, int value);
	// all "key=value" of a section, in file order
	std::vector<std::pair<std::string, std::string>> section(std::string_view section) const;

private:
	// line index of the key in the section, or -1; `end` gets the line after the section's last entry
	int find(std::string_view section, std::string_view key, int *end) const;

	std::vector<std::string> lines_;
	bool dirty_ = false;
};

std::string trim(std::string_view s);
bool iequals(std::string_view a, std::string_view b) noexcept;
std::wstring widen(std::string_view utf8);
std::string narrow(std::wstring_view wide);

}  // namespace ng
