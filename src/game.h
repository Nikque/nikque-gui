// The game on the board: the moves played from a start position and the place shown.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "rules.h"

namespace ng {

class Game {
public:
	Game() { reset(Position{}); }

	void reset(const Position &start);
	const Position &now() const noexcept { return pos_[cur_]; }
	const Position &at(int i) const noexcept { return pos_[i]; }
	const Position &start() const noexcept { return pos_[0]; }
	int cursor() const noexcept { return cur_; }
	int size() const noexcept { return static_cast<int>(moves_.size()); }
	const std::vector<int> &moves() const noexcept { return moves_; }

	// plays a legal move at the place shown; the moves after it are kept when the same move was there.
	// A forced pass that follows is added at once.
	bool play(int sq);
	// one move back / forward (a forced pass goes with the move before it)
	bool back();
	bool forward();
	// to the place after `n` moves (a place where the side must pass moves on past the pass)
	void go_to(int n);

	// the moves up to the end, without passes, as "f5d6c3..."
	std::string to_text(bool upto_cursor = false) const;

private:
	void settle_forward();

	std::vector<int> moves_;
	std::vector<Position> pos_;  // pos_[i] is the position after i moves
	int cur_ = 0;
};

struct ParsedMoves {
	std::vector<int> moves;   // squares, no passes
	int read = 0;             // coordinates found in the text
	int bad_at = -1;          // index (in `moves` order) of the first coordinate that was not legal, or -1
};

// Finds coordinates (a1..h8, also full-width letters and digits) in any text, ignoring the rest.
ParsedMoves parse_moves(std::string_view utf8);

// A board written as 64 squares a1..h8 ("X" or "*" black, "O" white, "-" or "." empty), with or without
// "board:" before, then the side to move ("X" black, "O" white; black when missing).
std::optional<Position> parse_board_text(std::string_view text);
// The 8 lines of a blog board (`<PARAM name="Line1" value="...">` .. Line8) with the number of the move on
// each square (0 or nothing for the 4 first discs): the moves in their order.
std::optional<std::vector<int>> parse_param_lines(std::string_view text);

// Plays the coordinates from `start`, adding the passes; stops at the first move that is not legal.
// Returns the number of coordinates played.
int replay(Game &game, const Position &start, const std::vector<int> &moves);

}  // namespace ng
