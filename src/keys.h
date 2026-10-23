// Keys for the actions. Every key can be changed in config.ini, [keys]: "back=Left", "back10=Ctrl+Left",
// several keys with ",".
#pragma once

#include <windows.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace ng {

class Ini;

enum class Action : int {
	Back,
	Forward,
	Back10,
	Forward10,
	Start,
	End,
	CandUp,
	CandDown,
	PlayCand,
	ToggleBook,
	ToggleUmi,
	HideEval,
	CopyMoves,
	PasteMoves,
	TogglePanel,
	ToggleLog,
	Rotate,
	Screenshot,
	ListPrev,
	ListNext,
	ListDelete,
	ListReload,
	Keep,
	EditBoard,
	ToggleDesign,
	Count
};

struct KeyChord {
	UINT vk = 0;
	bool ctrl = false, shift = false, alt = false;
	friend bool operator==(const KeyChord &, const KeyChord &) = default;
};

class KeyMap {
public:
	KeyMap();
	// reads [keys]; writes the default of every action missing there, so that the file shows them all
	void load(Ini &ini);
	std::optional<Action> find(const KeyChord &k) const;
	// "Ctrl+Left" etc. for menus and the list of keys
	std::wstring describe(Action a) const;
	static const char *name(Action a);

private:
	std::array<std::vector<KeyChord>, static_cast<size_t>(Action::Count)> keys_;
};

std::optional<KeyChord> parse_chord(std::string_view text);
std::string chord_text(const KeyChord &k);

}  // namespace ng
