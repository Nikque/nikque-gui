#include "linefind.h"

#include <bit>
#include <unordered_set>

namespace ng {

namespace {

struct Key {
	u64 black, white;
	int side;
	friend bool operator==(const Key &, const Key &) = default;
};
struct KeyHash {
	size_t operator()(const Key &k) const noexcept
	{
		const u64 h = k.black * 0x9E3779B97F4A7C15ULL ^ (k.white + 0x632BE59BD9B4E019ULL) * 0xC2B2AE3D27D4EB4FULL;
		return static_cast<size_t>(h ^ (h >> 29) ^ static_cast<u64>(k.side));
	}
};

constexpr u64 kNotA = 0xfefefefefefefefeULL, kNotH = 0x7f7f7f7f7f7f7f7fULL;

// every square one step in a direction (0..7: +1 -1 +8 -8 +9 -9 +7 -7); squares leaving the board vanish
u64 shift(u64 b, int d) noexcept
{
	switch (d) {
	case 0: return (b << 1) & kNotA;
	case 1: return (b >> 1) & kNotH;
	case 2: return b << 8;
	case 3: return b >> 8;
	case 4: return (b << 9) & kNotA;
	case 5: return (b >> 9) & kNotH;
	case 6: return (b << 7) & kNotH;
	default: return (b >> 7) & kNotA;
	}
}

// The discs that may still be turned: a disc is turned by a move on a square still empty that has the disc
// between it and another disc on a line, every square of the line between being filled then; so on some
// line, a square to come (a target square still empty) is reached through target squares on one side, and
// the next square on the other side is a target square.
u64 may_turn(u64 target, u64 to_come) noexcept
{
	u64 any = 0;
	for (int d = 0; d < 8; ++d) {
		const int back = d ^ 1;  // the opposite direction
		u64 reach = 0;           // squares from which a square to come is reached going in direction d
		for (int k = 0; k < 7; ++k) reach = shift(to_come | (target & reach), back);
		any |= reach & shift(target, d);  // the square before x (x - offset) is a target square
	}
	return any;
}

struct Searcher {
	Position target;
	u64 tocc = 0;
	long long limit = 0;
	std::stop_token stop;
	long long nodes = 0;
	bool gave_up = false;
	std::unordered_set<Key, KeyHash> dead;  // positions that do not lead to the target
	std::vector<int> line;

	bool dfs(const Position &p)
	{
		if (p == target) return true;
		if (gave_up) return false;
		if (++nodes > limit || ((nodes & 1023) == 0 && stop.stop_requested())) {
			gave_up = true;
			return false;
		}
		const u64 occ = p.black | p.white;
		if (occ == tocc) {
			// only the side may differ: a pass
			if (p.must_pass() && p.play(kPass) == target) {
				line.push_back(kPass);
				return true;
			}
			return false;
		}
		// a disc that can no longer be turned has its color for good
		if ((p.black ^ target.black) & occ & ~may_turn(tocc, tocc & ~occ)) return false;
		const Key key{p.black, p.white, p.side};
		if (dead.count(key)) return false;
		const u64 all = p.moves();
		if (!all) {
			if (p.must_pass()) {
				line.push_back(kPass);
				if (dfs(p.play(kPass))) return true;
				line.pop_back();
			}
			if (!gave_up) dead.insert(key);
			return false;
		}
		u64 m = all & tocc;
		struct Cand {
			int sq, score;
		} cs[32];
		int n = 0;
		while (m && n < 32) {
			const int sq = std::countr_zero(m);
			m &= m - 1;
			const Position q = p.play(sq);
			const int score = std::popcount((q.black & target.black) | (q.white & target.white));
			int k = n++;
			while (k > 0 && cs[k - 1].score < score) {
				cs[k] = cs[k - 1];
				--k;
			}
			cs[k] = {sq, score};
		}
		for (int i = 0; i < n; ++i) {
			line.push_back(cs[i].sq);
			if (dfs(p.play(cs[i].sq))) return true;
			line.pop_back();
			if (gave_up) return false;
		}
		dead.insert(key);
		return false;
	}
};

}  // namespace

LineSearch find_line(const Position &from, const Position &target, long long node_limit, std::stop_token stop)
{
	LineSearch r;
	Searcher s;
	s.target = target;
	s.tocc = target.black | target.white;
	s.limit = node_limit;
	s.stop = stop;
	const u64 focc = from.black | from.white;
	if ((focc & ~s.tocc) == 0 && s.dfs(from)) r.line = s.line;
	r.nodes = s.nodes;
	r.gave_up = s.gave_up;
	return r;
}

}  // namespace ng
