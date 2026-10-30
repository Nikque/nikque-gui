// Names of openings, read from a file of lines "name=moves" (moves from the start, written for f5 as
// the first move). Lines starting with "//" are notes; leading spaces (also full-width) only show the levels.
// A position is matched in its 8 symmetries.
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "analysis.h"
#include "game.h"

namespace ng {

class Openings {
public:
	// returns the number of names read (0 when the file is missing)
	int load(const std::wstring &path);
	bool empty() const noexcept { return names_.empty(); }
	size_t positions() const noexcept { return names_.size(); }
	// the name of the latest position of the game (up to the place shown) that has one
	const std::wstring *name_at(const Game &g) const;
	// what the position after a move is: 0 nothing, 1 on the way to an opening, 2 an opening (a forced pass goes with the move)
	int mark(const Position &p) const;

private:
	std::unordered_map<PosKey, std::wstring, PosKeyHash> names_;
	std::unordered_set<PosKey, PosKeyHash> route_;  // the positions on the lines of the openings
};

}  // namespace ng
