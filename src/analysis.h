// What the engines said about the moves of a position: search values (hint), book values and the book
// analyses (book moveinfo). Kept per position, up to a limit.
#pragma once

#include <algorithm>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "rules.h"

namespace ng {

struct MoveEval {
	int sq = kNoMove;

	// search (hint), from the side to move
	bool searched = false;
	int score = 0;
	int depth = 0;
	int selectivity = 100;
	char bound = 0;          // '<' or '>' when the score is a bound
	std::vector<int> pv;     // the expected moves, this move first
	// the other engine (compare mode), from the side to move
	bool cmp_searched = false;
	int cmp_score = 0, cmp_depth = 0, cmp_selectivity = 100;
	char cmp_bound = 0;

	// book (book moveinfo)
	bool book_row = false;   // the row of the move came
	bool in_book = false;
	int book_value = 0;
	int book_level = 0;
	long long lines = -1;
	int stage = -1;          // 0..7, -1 not known yet
	long long missing = -1;
	bool has_link = false;
	int link = 0;
	bool leaf = false;       // the book's leaf move of the position (not in the book)
	int leaf_value = 0;
	int leaf_level = 0;
	bool umi_known = false;
	bool umi_none = false;   // too large
	bool umi_exact = true;
	int umi_black = -1, umi_white = -1;
	long long positions = -1;
};

struct PosInfo {
	std::vector<MoveEval> moves;   // every legal move (or the pass)
	// book analyses
	std::string book_params;       // the parameters the book parts were asked with
	bool book_begun = false;
	bool book_done = false;
	bool in_book = false;
	bool count_done = false;
	// search
	int search_level = 0;
	bool search_done = false;
	// the other engine (compare mode)
	int cmp_level = 0;
	bool cmp_done = false;
	// level counts (book levels)
	bool levels_done = false;
	std::vector<std::pair<int, long long>> levels;
	long long levels_positions = -1;
	// the lines of best moves (book bestlines, loss 0): lines, end positions, and those not solved
	bool best_done = false, best_error = false;
	long long best_lines = -1, best_ends = -1, best_incomplete = -1, best_incomplete_ends = -1;

	MoveEval *find(int sq);
	const MoveEval *find(int sq) const;
	void reset_book();
	void reset_search();
	// the book parts to be asked with `params`: what was known with other ones goes; true when they changed
	bool set_book_params(const std::string &params);
};

struct PosKey {
	u64 black, white;
	int side;
	friend bool operator==(const PosKey &, const PosKey &) = default;
};
struct PosKeyHash {
	size_t operator()(const PosKey &k) const noexcept
	{
		u64 h = k.black * 0x9E3779B97F4A7C15ULL ^ (k.white + 0x632BE59BD9B4E019ULL) * 0xC2B2AE3D27D4EB4FULL;
		return static_cast<size_t>(h ^ (h >> 29) ^ static_cast<u64>(k.side));
	}
};
inline PosKey key_of(const Position &p) { return {p.black, p.white, p.side}; }

// the positions seen, the oldest dropped beyond `limit`
class AnalysisCache {
public:
	explicit AnalysisCache(size_t limit = 4096) : limit_(limit) {}
	PosInfo &get(const Position &p);
	PosInfo *peek(const Position &p);
	const PosInfo *peek(const Position &p) const;
	void clear_book();     // the book changed
	void clear_search();
	// room for at least `n` positions (the open nodes of the tree, kept while it is shown): the limit is never lowered
	void keep_at_least(size_t n) noexcept { limit_ = std::max(limit_, n); }
	// what is known of a position, for the position turned by a symmetry (the values and the book are the same)
	void copy_symmetric(const Position &p, int s);
	size_t size() const noexcept { return map_.size(); }

private:
	struct Entry {
		PosInfo info;
		std::list<PosKey>::iterator age;
	};
	std::unordered_map<PosKey, Entry, PosKeyHash> map_;
	std::list<PosKey> order_;  // most recent first
	size_t limit_;
};

// "key=value" words of an "@..." line
class Fields {
public:
	explicit Fields(std::string_view line);
	std::string_view kind() const noexcept { return kind_; }  // "begin", "move", "end", ...
	std::optional<std::string_view> get(std::string_view key) const;
	std::optional<long long> num(std::string_view key) const;

private:
	std::string_view kind_;
	std::vector<std::pair<std::string_view, std::string_view>> kv_;
};

// a line of hint: "16@73%  +00  0:00.046  429076  9327739 d3 E3 f4 C3 ..."
struct HintLine {
	bool book = false;
	int depth = 0;
	int selectivity = 100;
	int score = 0;
	char bound = 0;
	std::vector<int> pv;
};
std::optional<HintLine> parse_hint_line(std::string_view line);

// the number n of a marker "echo Mn": the engine answers with a warning line that names "Mn"; -1 otherwise
int echo_marker(std::string_view line);

// The engine writes a line of the book analyses whole, from another thread, while its main thread may write a line in
// parts (a warning, the result of a command): the analysis line then comes after the first part of that line
// ("WARNING: @end id=5 ms=0", the rest of the warning on the next line). The analysis lines ("@<word> id=<number>")
// are taken out where they begin, and each line of the main thread is put together again: its first part waits in
// `open` for its rest, the next line that is not an analysis line.
void unmix_lines(std::vector<std::string> &lines, std::string &open);

// The numbers of the requests to the engines (id=, the echo markers): the book analyses take id= up to 100,000.
inline constexpr int kMaxRequestId = 99999;
// the number after `last`, from 1 again after kMaxRequestId, skipping the numbers `in_use` says are taken
template <class InUse>
int next_request_id(int last, InUse in_use)
{
	int id = last;
	for (int k = 0; k < kMaxRequestId; ++k) {
		id = id >= kMaxRequestId || id < 1 ? 1 : id + 1;
		if (!in_use(id)) return id;
	}
	return id;  // every number taken (a few requests are kept at a time: not reached)
}

// applies an "@move" line of book moveinfo to the position's moves
void apply_move_line(PosInfo &info, const Fields &f);

// a search to the end (the move is searched as deep as the empty squares after it)
bool move_exact(const MoveEval &m, const Position &p);

// the game line after the backward analysis: position i (after i moves)
struct LineEval {
	bool known = false;      // a best value is known
	bool complete = false;   // every move has a value (so the best value can go back)
	int best = 0;            // from the side to move
	bool exact = false;
	bool played_known = false;  // the move played from here (the next move of the game)
	int played = 0;
	bool played_exact = false;
	bool played_improved = false;
};

class Game;
// The values along the game, from the end backward: the value of the move played from a position becomes the
// best value of the next position when every move there has a value (`backward`; never for book values and
// searches to the end).
std::vector<LineEval> backward_line(const Game &g, const AnalysisCache &cache, bool use_book, bool backward);

// a line of book missing: the moves from the position (the move first), the leaf to add, its value
struct MissingLine {
	std::vector<int> moves;
	int leaf = kNoMove;
	int value = 0;
	int empties = 0;
};
std::optional<MissingLine> parse_missing_line(const Fields &f);

// the value shown for a move: the book's when it is in the book (and the book is used), else the search's,
// else the leaf's. Returns false when nothing is known yet.
bool shown_value(const MoveEval &m, bool use_book, int *value);
// the "Lv" shown for a value that is not from the book (search depth, or the leaf's level)
int shown_level(const MoveEval &m, bool use_book);

}  // namespace ng
