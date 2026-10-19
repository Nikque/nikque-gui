// Othello rules on 64-bit boards: a1 is bit 0, b1 bit 1, ..., h8 bit 63 (index = 8 * row + column).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ng {

using u64 = std::uint64_t;

inline constexpr int kPass = 64;     // the move "pass"
inline constexpr int kNoMove = -1;

enum Side : int { kBlack = 0, kWhite = 1 };

// squares where the side with the discs `me` may play
u64 legal_moves(u64 me, u64 opp) noexcept;
// discs turned by playing on `sq` (0 when the move does not turn anything)
u64 flips(u64 me, u64 opp, int sq) noexcept;

struct Position {
	u64 black = 0x0000000810000000ULL;  // e4 d5
	u64 white = 0x0000001008000000ULL;  // d4 e5
	int side = kBlack;

	u64 me() const noexcept { return side == kBlack ? black : white; }
	u64 opp() const noexcept { return side == kBlack ? white : black; }
	u64 moves() const noexcept { return legal_moves(me(), opp()); }
	bool can_play(int sq) const noexcept;
	// the side to move has no move but the other side has one
	bool must_pass() const noexcept;
	bool game_over() const noexcept;
	int empties() const noexcept;
	int ply() const noexcept { return 60 - empties(); }
	// the position after the move (kPass changes the side only); the move must be legal
	Position play(int sq) const noexcept;
	// 64 squares a1..h8 ('X' black, 'O' white, '-' empty), a space and the side ('X' or 'O')
	std::string to_board_string() const;

	friend bool operator==(const Position &, const Position &) = default;
};

// the 8 symmetries of the board: 0 identity, 1 mirror left-right, 2 mirror top-bottom, 3 rotate 180,
// 4 transpose (a1-h8 diagonal), 5..7 the transpose with 1..3
u64 symmetric(u64 b, int s) noexcept;
Position symmetric(const Position &p, int s) noexcept;
int symmetric_square(int sq, int s) noexcept;

// "a1".."h8", "pa"
std::string square_name(int sq);
// accepts a1..h8 in either case and "pa"/"ps"; returns kNoMove otherwise
int parse_square(std::string_view s) noexcept;

}  // namespace ng
