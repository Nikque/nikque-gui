#include "rules.h"

#include <bit>

namespace ng {

namespace {

constexpr u64 kNotFileA = 0xfefefefefefefefeULL;
constexpr u64 kNotFileH = 0x7f7f7f7f7f7f7f7fULL;

// moves every disc one step in a direction; discs leaving the board vanish
template <int D>
constexpr u64 step(u64 b) noexcept
{
	if constexpr (D == 1) return (b << 1) & kNotFileA;   // column + 1
	else if constexpr (D == -1) return (b >> 1) & kNotFileH;
	else if constexpr (D == 8) return b << 8;           // row + 1
	else if constexpr (D == -8) return b >> 8;
	else if constexpr (D == 9) return (b << 9) & kNotFileA;
	else if constexpr (D == 7) return (b << 7) & kNotFileH;
	else if constexpr (D == -7) return (b >> 7) & kNotFileA;
	else return (b >> 9) & kNotFileH;                     // -9
}

// empty squares reached from a disc of `me` over one or more discs of `opp`
template <int D>
u64 moves_in(u64 me, u64 opp, u64 empty) noexcept
{
	u64 run = step<D>(me) & opp;
	run |= step<D>(run) & opp;
	run |= step<D>(run) & opp;
	run |= step<D>(run) & opp;
	run |= step<D>(run) & opp;
	run |= step<D>(run) & opp;
	return step<D>(run) & empty;
}

template <int D>
u64 flips_in(u64 me, u64 opp, u64 from) noexcept
{
	u64 turned = 0;
	u64 b = step<D>(from);
	while (b & opp) {
		turned |= b;
		b = step<D>(b);
	}
	return (b & me) ? turned : 0;
}

}  // namespace

u64 legal_moves(u64 me, u64 opp) noexcept
{
	const u64 empty = ~(me | opp);
	return moves_in<1>(me, opp, empty) | moves_in<-1>(me, opp, empty) | moves_in<8>(me, opp, empty) |
	       moves_in<-8>(me, opp, empty) | moves_in<9>(me, opp, empty) | moves_in<-9>(me, opp, empty) |
	       moves_in<7>(me, opp, empty) | moves_in<-7>(me, opp, empty);
}

u64 flips(u64 me, u64 opp, int sq) noexcept
{
	const u64 m = 1ULL << sq;
	if ((me | opp) & m) return 0;
	return flips_in<1>(me, opp, m) | flips_in<-1>(me, opp, m) | flips_in<8>(me, opp, m) |
	       flips_in<-8>(me, opp, m) | flips_in<9>(me, opp, m) | flips_in<-9>(me, opp, m) |
	       flips_in<7>(me, opp, m) | flips_in<-7>(me, opp, m);
}

bool Position::can_play(int sq) const noexcept
{
	if (sq == kPass) return must_pass();
	return sq >= 0 && sq < 64 && ((moves() >> sq) & 1);
}

bool Position::must_pass() const noexcept
{
	return moves() == 0 && legal_moves(opp(), me()) != 0;
}

bool Position::game_over() const noexcept
{
	return moves() == 0 && legal_moves(opp(), me()) == 0;
}

int Position::empties() const noexcept
{
	return 64 - std::popcount(black | white);
}

Position Position::play(int sq) const noexcept
{
	Position next = *this;
	if (sq != kPass) {
		const u64 turned = flips(me(), opp(), sq) | (1ULL << sq);
		if (side == kBlack) {
			next.black |= turned;
			next.white &= ~turned;
		} else {
			next.white |= turned;
			next.black &= ~turned;
		}
	}
	next.side = side ^ 1;
	return next;
}

std::string Position::to_board_string() const
{
	std::string s(64, '-');
	for (int i = 0; i < 64; ++i) {
		if ((black >> i) & 1) s[i] = 'X';
		else if ((white >> i) & 1) s[i] = 'O';
	}
	s += side == kBlack ? " X" : " O";
	return s;
}

std::string square_name(int sq)
{
	if (sq == kPass) return "pa";
	if (sq < 0 || sq >= 64) return "--";
	return {static_cast<char>('a' + (sq & 7)), static_cast<char>('1' + (sq >> 3))};
}

int parse_square(std::string_view s) noexcept
{
	if (s.size() != 2) return kNoMove;
	char c = s[0], r = s[1];
	if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
	if (r >= 'A' && r <= 'Z') r = static_cast<char>(r - 'A' + 'a');
	if (c == 'p' && (r == 'a' || r == 's')) return kPass;
	if (c < 'a' || c > 'h' || r < '1' || r > '8') return kNoMove;
	return (r - '1') * 8 + (c - 'a');
}

}  // namespace ng

namespace ng {

namespace {

u64 mirror_lr(u64 b) noexcept  // column x -> 7 - x
{
	b = ((b >> 1) & 0x5555555555555555ULL) | ((b & 0x5555555555555555ULL) << 1);
	b = ((b >> 2) & 0x3333333333333333ULL) | ((b & 0x3333333333333333ULL) << 2);
	return ((b >> 4) & 0x0f0f0f0f0f0f0f0fULL) | ((b & 0x0f0f0f0f0f0f0f0fULL) << 4);
}

u64 mirror_tb(u64 b) noexcept  // row y -> 7 - y
{
	return std::byteswap(b);
}

u64 transpose(u64 b) noexcept  // (x, y) -> (y, x)
{
	u64 t = (b ^ (b >> 7)) & 0x00aa00aa00aa00aaULL;
	b ^= t ^ (t << 7);
	t = (b ^ (b >> 14)) & 0x0000cccc0000ccccULL;
	b ^= t ^ (t << 14);
	t = (b ^ (b >> 28)) & 0x00000000f0f0f0f0ULL;
	return b ^ t ^ (t << 28);
}

}  // namespace

u64 symmetric(u64 b, int s) noexcept
{
	if (s & 4) b = transpose(b);
	if (s & 1) b = mirror_lr(b);
	if (s & 2) b = mirror_tb(b);
	return b;
}

Position symmetric(const Position &p, int s) noexcept
{
	return {symmetric(p.black, s), symmetric(p.white, s), p.side};
}

int symmetric_square(int sq, int s) noexcept
{
	if (sq < 0 || sq >= 64) return sq;
	return std::countr_zero(symmetric(1ULL << sq, s));
}

}  // namespace ng
