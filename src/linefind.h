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
	size_t dead = 0;                        // positions kept as not leading to the target
};

// The positions known not to lead to the target are kept up to `dead_limit` (about 66 bytes each: a search of 50
// million positions that finds nothing would keep about 15 million of them); the positions met after that are searched
// again when met again. A search that ends before `node_limit` finds the same line; the positions searched again count
// toward the limit, so a search near it may give up where keeping them all would have found the line.
inline constexpr size_t kDeadLimit = 2'000'000;
LineSearch find_line(const Position &from, const Position &target, long long node_limit, std::stop_token stop = {},
                     size_t dead_limit = kDeadLimit);

}  // namespace ng
