// The game list: many games (a start position and the moves without passes), the moves of all the games kept
// in one buffer. Reading and writing text files (one game a line), sorting, joining the same games, cutting,
// shuffling, turning the games to one orientation or to random ones, and finding the games where a position
// appears.
#pragma once

#include <cstdint>
#include <random>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "rules.h"

namespace ng {

class GameList {
public:
	size_t size() const noexcept { return games_.size(); }
	bool empty() const noexcept { return games_.empty(); }
	const Position &start(size_t i) const noexcept { return starts_[games_[i].start]; }
	std::span<const std::uint8_t> moves(size_t i) const noexcept
	{
		return {pool_.data() + games_[i].off, games_[i].len};
	}
	void add(const Position &start, std::span<const std::uint8_t> moves);
	// the moves of game i written over its own: as many moves (an orientation keeps them all), else nothing is
	// written and false returned. Games of different numbers may be written at the same time (their moves never share
	// a place).
	bool set_moves(size_t i, std::span<const std::uint8_t> moves) noexcept;
	void erase(size_t i);
	void clear();
	// room for more games and moves (a file read at once does not grow the buffers again and again)
	void reserve(size_t games, size_t moves);
	// keeps the games of `order` (indices into this list), in that order
	void reorder(const std::vector<std::uint32_t> &order);
	size_t bytes() const noexcept;

private:
	struct Entry {
		std::uint32_t off;
		std::uint32_t start;
		std::uint8_t len;
	};
	std::uint32_t start_id(const Position &p);
	void pack();  // the moves of the games one after the other in their order, without the unused ones
	struct StartHash {
		size_t operator()(const Position &p) const noexcept
		{
			const u64 h = p.black * 0x9E3779B97F4A7C15ULL ^ (p.white + 0x632BE59BD9B4E019ULL) * 0xC2B2AE3D27D4EB4FULL;
			return static_cast<size_t>(h ^ (h >> 29) ^ static_cast<u64>(p.side));
		}
	};

	std::vector<Entry> games_;
	std::vector<std::uint8_t> pool_;
	std::vector<Position> starts_;
	std::unordered_map<Position, std::uint32_t, StartHash> start_ids_;
	size_t unused_ = 0;  // moves of games taken away still in the pool
};

struct ListReadStats {
	bool opened = false;
	bool cancelled = false;  // a stop was seen (asked before or while reading): the games read are not all of them
	size_t lines = 0;   // lines with a game
	size_t games = 0;   // games added
	size_t cut = 0;     // games read up to a move that cannot be played
};

// A line of a game: "f5d6c3..." from the usual start, or "<64 squares> <X|O>;moves" from another start
// (the form Edax writes), or any text with coordinates. The moves are played from the start; a move that
// cannot be played ends the game there (`cut`). Returns false when the line has no game.
bool parse_list_line(std::string_view line, Position *start, std::vector<std::uint8_t> *moves, bool *cut,
                     const Position &usual = Position{});
std::string list_line(const Position &start, std::span<const std::uint8_t> moves);
// `usual`: the start of the lines of moves only
ListReadStats read_list_text(std::string_view text, GameList &list, std::stop_token stop = {}, const Position &usual = Position{});
ListReadStats read_list(const std::wstring &path, GameList &list, std::stop_token stop = {}, const Position &usual = Position{});
bool write_list(const std::wstring &path, const GameList &list);

// shorter games first, then by the moves (as their names: a1 a2 .. b1 ..)
void sort_list(GameList &list);
// the same games (start and moves) once, the first kept; returns the number taken out
size_t unique_list(GameList &list);
// every game cut to its first n moves
void truncate_list(GameList &list, int n);
void shuffle_list(GameList &list, std::uint64_t seed);

// One orientation for a game: at every position that is symmetric (the start, and any later one), the
// symmetry that brings the next move to its first form is chosen (the first move from the usual start
// becomes f5). The moves stay legal; the start stays.
std::vector<std::uint8_t> normalized_moves(const Position &start, std::span<const std::uint8_t> moves);
// Random orientations: at every symmetric position where `sides` has the side to move (1 black, 2 white,
// 3 both), one of the symmetries keeping the position is drawn.
std::vector<std::uint8_t> scattered_moves(const Position &start, std::span<const std::uint8_t> moves,
                                          std::mt19937_64 &rng, int sides);
void normalize_list(GameList &list, std::stop_token stop = {});
void scatter_list(GameList &list, std::uint64_t seed, int sides, std::stop_token stop = {});

// A game where the position appears: after `ply` moves of the game (passes not counted), the game's position
// is symmetric(target, sym) (sym 0 when the symmetries are not looked at).
struct ListHit {
	std::uint32_t game;
	std::uint8_t ply;
	std::uint8_t sym;
};
std::vector<ListHit> search_list(const GameList &list, const Position &target, bool symmetries,
                                 std::stop_token stop = {});

// the symmetries s with symmetric(p, s) == p, as bits
unsigned symmetries_of(const Position &p) noexcept;

}  // namespace ng
