// A line of moves that leads from one position to another, found without the book (when the book has none):
// every move goes on a square that has a disc in the target, the positions known not to lead there are kept,
// and the moves that bring the colors nearer to the target are tried first.
#pragma once

#include <optional>
#include <stop_token>
#include <vector>

#include "rules.h"

namespace ng {

struct LineSearch {
	std::optional<std::vector<int>> line;  // the moves (passes as kPass); empty optional: none found
	long long nodes = 0;
	bool gave_up = false;                   // the node limit or a stop came first
};

LineSearch find_line(const Position &from, const Position &target, long long node_limit, std::stop_token stop = {});

}  // namespace ng
