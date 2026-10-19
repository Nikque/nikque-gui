#include "keys.h"

#include <string_view>

#include "ini.h"

namespace ng {

namespace {

struct ActionInfo {
	const char *name;
	const char *keys;  // default
};

constexpr ActionInfo kActions[] = {
	{"back", "Left"},
	{"forward", "Right"},
	{"back10", "Ctrl+Left"},
	{"forward10", "Ctrl+Right"},
	{"start", "N, Home"},
	{"end", "End"},
	{"cand_up", "Up"},
	{"cand_down", "Down"},
	{"play_cand", "Enter, Space"},
	{"book", "B"},
	{"umi", "U"},
	{"hide_eval", "H"},
	{"copy", "Ctrl+C"},
	{"paste", "Ctrl+V"},
	{"panel", "P"},
	{"log", "L"},
};
static_assert(std::size(kActions) == static_cast<size_t>(Action::Count));

struct KeyName {
	const char *name;
	UINT vk;
};

constexpr KeyName kNames[] = {
	{"Left", VK_LEFT}, {"Right", VK_RIGHT}, {"Up", VK_UP}, {"Down", VK_DOWN}, {"Home", VK_HOME},
	{"End", VK_END}, {"PageUp", VK_PRIOR}, {"PageDown", VK_NEXT}, {"Enter", VK_RETURN}, {"Space", VK_SPACE},
	{"Tab", VK_TAB}, {"Esc", VK_ESCAPE}, {"Backspace", VK_BACK}, {"Delete", VK_DELETE}, {"Insert", VK_INSERT},
	{"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4}, {"F5", VK_F5}, {"F6", VK_F6},
	{"F7", VK_F7}, {"F8", VK_F8}, {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
};

std::vector<KeyChord> parse_list(std::string_view text)
{
	std::vector<KeyChord> out;
	size_t i = 0;
	while (i <= text.size()) {
		size_t e = text.find(',', i);
		if (e == std::string_view::npos) e = text.size();
		if (auto k = parse_chord(trim(text.substr(i, e - i)))) out.push_back(*k);
		i = e + 1;
	}
	return out;
}

}  // namespace

std::optional<KeyChord> parse_chord(std::string_view text)
{
	KeyChord k;
	std::string_view rest = text;
	for (;;) {
		const size_t plus = rest.find('+');
		if (plus == std::string_view::npos || plus + 1 >= rest.size()) break;
		const std::string mod = trim(rest.substr(0, plus));
		if (iequals(mod, "Ctrl")) k.ctrl = true;
		else if (iequals(mod, "Shift")) k.shift = true;
		else if (iequals(mod, "Alt")) k.alt = true;
		else return std::nullopt;
		rest = rest.substr(plus + 1);
	}
	const std::string key = trim(rest);
	if (key.size() == 1) {
		char c = key[0];
		if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
			k.vk = static_cast<UINT>(c);
			return k;
		}
	}
	for (const auto &n : kNames)
		if (iequals(key, n.name)) {
			k.vk = n.vk;
			return k;
		}
	return std::nullopt;
}

std::string chord_text(const KeyChord &k)
{
	std::string s;
	if (k.ctrl) s += "Ctrl+";
	if (k.shift) s += "Shift+";
	if (k.alt) s += "Alt+";
	if ((k.vk >= 'A' && k.vk <= 'Z') || (k.vk >= '0' && k.vk <= '9')) {
		s += static_cast<char>(k.vk);
		return s;
	}
	for (const auto &n : kNames)
		if (n.vk == k.vk) return s + n.name;
	return s + "?";
}

KeyMap::KeyMap()
{
	for (size_t i = 0; i < std::size(kActions); ++i) keys_[i] = parse_list(kActions[i].keys);
}

void KeyMap::load(Ini &ini)
{
	for (size_t i = 0; i < std::size(kActions); ++i) {
		const std::string v = ini.get("keys", kActions[i].name);
		if (v.empty()) {
			ini.set("keys", kActions[i].name, kActions[i].keys);
			keys_[i] = parse_list(kActions[i].keys);
		} else {
			keys_[i] = parse_list(v);
		}
	}
}

std::optional<Action> KeyMap::find(const KeyChord &k) const
{
	for (size_t i = 0; i < keys_.size(); ++i)
		for (const auto &c : keys_[i])
			if (c == k) return static_cast<Action>(i);
	return std::nullopt;
}

std::wstring KeyMap::describe(Action a) const
{
	const auto &list = keys_[static_cast<size_t>(a)];
	if (list.empty()) return {};
	return widen(chord_text(list.front()));
}

const char *KeyMap::name(Action a)
{
	return kActions[static_cast<size_t>(a)].name;
}

}  // namespace ng
