// What the engines said about the moves of a position: search values (hint), book values and the book
// analyses (book moveinfo). Kept per position, up to a limit.
#pragma once

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
	// level counts (book levels)
	bool levels_done = false;
	std::vector<std::pair<int, long long>> levels;
	long long levels_positions = -1;

	MoveEval *find(int sq);
	const MoveEval *find(int sq) const;
	void reset_book();
	void reset_search();
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

// applies an "@move" line of book moveinfo to the position's moves
void apply_move_line(PosInfo &info, const Fields &f);

// the value shown for a move: the book's when it is in the book (and the book is used), else the search's,
// else the leaf's. Returns false when nothing is known yet.
bool shown_value(const MoveEval &m, bool use_book, int *value);
// the "Lv" shown for a value that is not from the book (search depth, or the leaf's level)
int shown_level(const MoveEval &m, bool use_book);

}  // namespace ng
