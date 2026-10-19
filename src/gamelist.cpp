#include "gamelist.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <thread>

#include "game.h"

namespace ng {

namespace {

// the order of squares by name: a1 a2 .. a8 b1 ..
constexpr int name_key(int sq) noexcept { return (sq & 7) * 8 + (sq >> 3); }

// the key that makes f5 the first of the four first moves (and orders the other squares in a fixed way)
int orient_key(int sq) noexcept { return symmetric_square(sq, 7); }

std::uint64_t splitmix(std::uint64_t &s) noexcept
{
	std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

// the legal moves of the side to move, after a forced pass (one computation of the moves in a game)
u64 moves_after_pass(Position &p) noexcept
{
	u64 m = p.moves();
	if (!m) {
		const Position q = p.play(kPass);
		m = q.moves();
		if (m) p = q;  // the game goes on with the other side (no move at all: the game is over)
	}
	return m;
}

// plays a move of a game known to be legal (a pass first when the side to move cannot play it there)
void play_known(Position &p, int sq) noexcept
{
	u64 f = flips(p.me(), p.opp(), sq);
	if (!f) {
		p.side ^= 1;
		f = flips(p.me(), p.opp(), sq);
	}
	const u64 turned = f | (1ULL << sq);
	if (p.side == kBlack) {
		p.black |= turned;
		p.white &= ~turned;
	} else {
		p.white |= turned;
		p.black &= ~turned;
	}
	p.side ^= 1;
}

size_t moves_in(const GameList &l)
{
	size_t n = 0;
	for (size_t i = 0; i < l.size(); ++i) n += l.moves(i).size();
	return n;
}

bool same_game(const GameList &l, size_t a, size_t b)
{
	if (!(l.start(a) == l.start(b))) return false;
	const auto x = l.moves(a), y = l.moves(b);
	return x.size() == y.size() && std::equal(x.begin(), x.end(), y.begin());
}

}  // namespace

std::uint32_t GameList::start_id(const Position &p)
{
	const auto [it, added] = start_ids_.try_emplace(p, static_cast<std::uint32_t>(starts_.size()));
	if (added) starts_.push_back(p);
	return it->second;
}

void GameList::add(const Position &start, std::span<const std::uint8_t> moves)
{
	const size_t n = std::min<size_t>(moves.size(), 255);
	// the usual start is looked up first (nearly every game has it)
	const std::uint32_t sid = (!starts_.empty() && starts_[0] == start) ? 0 : start_id(start);
	games_.push_back({static_cast<std::uint32_t>(pool_.size()), sid, static_cast<std::uint8_t>(n)});
	pool_.insert(pool_.end(), moves.begin(), moves.begin() + n);
}

void GameList::erase(size_t i)
{
	if (i >= games_.size()) return;
	unused_ += games_[i].len;
	games_.erase(games_.begin() + static_cast<std::ptrdiff_t>(i));
	if (unused_ > (1u << 16) && unused_ * 2 > pool_.size()) {
		std::vector<std::uint32_t> all(games_.size());
		std::iota(all.begin(), all.end(), 0u);
		reorder(all);
	}
}

void GameList::clear()
{
	games_.clear();
	pool_.clear();
	starts_.clear();
	start_ids_.clear();
	unused_ = 0;
}

void GameList::reserve(size_t games, size_t moves)
{
	games_.reserve(games_.size() + games);
	pool_.reserve(pool_.size() + moves);
}

void GameList::reorder(const std::vector<std::uint32_t> &order)
{
	std::vector<Entry> games;
	std::vector<std::uint8_t> pool;
	games.reserve(order.size());
	size_t total = 0;
	for (const auto i : order) total += games_[i].len;
	pool.reserve(total);
	for (const auto i : order) {
		const Entry &e = games_[i];
		games.push_back({static_cast<std::uint32_t>(pool.size()), e.start, e.len});
		pool.insert(pool.end(), pool_.begin() + e.off, pool_.begin() + e.off + e.len);
	}
	games_ = std::move(games);
	pool_ = std::move(pool);
	unused_ = 0;
}

size_t GameList::bytes() const noexcept
{
	return games_.capacity() * sizeof(Entry) + pool_.capacity() + starts_.capacity() * sizeof(Position);
}

bool parse_list_line(std::string_view line, Position *start, std::vector<std::uint8_t> *moves, bool *cut, const Position &usual)
{
	*start = usual;
	moves->clear();
	*cut = false;
	size_t i = 0;
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
	if (i >= line.size() || line[i] == '#' || line[i] == '%') return false;
	std::string_view rest = line.substr(i);
	bool board = false;
	if (rest.size() >= 64) {
		u64 b = 0, w = 0;
		board = true;
		for (int k = 0; k < 64 && board; ++k) {
			const char c = rest[k];
			if (c == 'X' || c == 'x' || c == '*') b |= 1ULL << k;
			else if (c == 'O' || c == 'o') w |= 1ULL << k;
			else if (c != '-' && c != '.') board = false;
		}
		if (board && std::popcount(b | w) >= 4) {
			size_t k = 64;
			while (k < rest.size() && (rest[k] == ' ' || rest[k] == '\t')) ++k;
			int side = kBlack;
			if (k < rest.size() && (rest[k] == 'O' || rest[k] == 'o')) side = kWhite, ++k;
			else if (k < rest.size() && (rest[k] == 'X' || rest[k] == 'x' || rest[k] == '*')) ++k;
			*start = Position{b, w, side};
			rest = rest.substr(k);
		} else {
			board = false;
		}
	}
	// coordinates: ASCII text is read here directly (the same rules as parse_moves), other text by parse_moves
	thread_local std::vector<int> coords;
	coords.clear();
	if (std::all_of(rest.begin(), rest.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; })) {
		auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
		for (size_t k = 0; k + 1 < rest.size(); ++k) {
			const char c = lower(rest[k]), r = rest[k + 1];
			if (c < 'a' || c > 'h' || r < '1' || r > '8') continue;
			if (k > 0 && lower(rest[k - 1]) >= 'a' && lower(rest[k - 1]) <= 'z') continue;  // inside a word
			if (k + 2 < rest.size() && rest[k + 2] >= '0' && rest[k + 2] <= '9') continue;    // part of a number
			coords.push_back((r - '1') * 8 + (c - 'a'));
			++k;
		}
	} else {
		coords = parse_moves(rest).moves;
	}
	if (coords.empty() && !board) return false;
	Position p = *start;
	for (const int sq : coords) {
		// a move that turns discs is legal; one that does not is legal only after a forced pass (the side to move
		// has no move at all), so the list of moves is made only then
		u64 f = flips(p.me(), p.opp(), sq);
		if (!f && legal_moves(p.me(), p.opp()) == 0) {
			p.side ^= 1;
			f = flips(p.me(), p.opp(), sq);
		}
		if (!f) {
			*cut = true;
			break;
		}
		const u64 turned = f | (1ULL << sq);
		if (p.side == kBlack) {
			p.black |= turned;
			p.white &= ~turned;
		} else {
			p.white |= turned;
			p.black &= ~turned;
		}
		p.side ^= 1;
		moves->push_back(static_cast<std::uint8_t>(sq));
	}
	return true;
}

std::string list_line(const Position &start, std::span<const std::uint8_t> moves)
{
	std::string s;
	s.reserve(moves.size() * 2 + 2);
	if (!(start == Position{})) {
		s = start.to_board_string();
		s += ';';
	}
	for (const auto m : moves) s += square_name(m);
	return s;
}

namespace {

// reads lines from text, the last one may be incomplete when `final` is false (it is left in `text`)
void take_lines(std::string &text, bool final, GameList &list, ListReadStats &st, std::stop_token stop, const Position &usual)
{
	Position start;
	std::vector<std::uint8_t> moves;
	size_t i = 0;
	for (;;) {
		size_t e = text.find('\n', i);
		if (e == std::string::npos) {
			if (!final) break;
			e = text.size();
		}
		std::string_view line(text.data() + i, e - i);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		bool cut = false;
		if (parse_list_line(line, &start, &moves, &cut, usual)) {
			++st.lines;
			list.add(start, moves);
			++st.games;
			if (cut) ++st.cut;
			if ((st.lines & 4095) == 0 && stop.stop_requested()) {
				st.cancelled = true;
				text.clear();
				return;
			}
		}
		if (e >= text.size()) {
			text.clear();
			return;
		}
		i = e + 1;
	}
	text.erase(0, i);
}

}  // namespace

ListReadStats read_list_text(std::string_view text, GameList &list, std::stop_token stop, const Position &usual)
{
	ListReadStats st;
	st.opened = true;
	std::string t(text);
	take_lines(t, true, list, st, stop, usual);
	return st;
}

ListReadStats read_list(const std::wstring &path, GameList &list, std::stop_token stop, const Position &usual)
{
	ListReadStats st;
	FILE *f = nullptr;
	if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return st;
	st.opened = true;
	// room for the games at once: two letters a move; a full game is about 120 letters
	if (_fseeki64(f, 0, SEEK_END) == 0) {
		const long long size = _ftelli64(f);
		if (size > 0) list.reserve(static_cast<size_t>(size / 100), static_cast<size_t>(size / 2));
		_fseeki64(f, 0, SEEK_SET);
	}
	std::string text;
	char buf[1 << 16];
	size_t n;
	bool first = true;
	while (!st.cancelled && (n = fread(buf, 1, sizeof buf, f)) > 0) {
		size_t skip = 0;
		if (first && n >= 3 && buf[0] == '\xEF' && buf[1] == '\xBB' && buf[2] == '\xBF') skip = 3;
		first = false;
		text.append(buf + skip, n - skip);
		take_lines(text, false, list, st, stop, usual);
	}
	if (!st.cancelled) take_lines(text, true, list, st, stop, usual);
	fclose(f);
	return st;
}

bool write_list(const std::wstring &path, const GameList &list)
{
	FILE *f = nullptr;
	if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) return false;
	std::string out;
	bool ok = true;
	for (size_t i = 0; i < list.size() && ok; ++i) {
		out += list_line(list.start(i), list.moves(i));
		out += "\r\n";
		if (out.size() > (1u << 20)) {
			ok = fwrite(out.data(), 1, out.size(), f) == out.size();
			out.clear();
		}
	}
	if (ok && !out.empty()) ok = fwrite(out.data(), 1, out.size(), f) == out.size();
	return fclose(f) == 0 && ok;
}

void sort_list(GameList &list)
{
	std::vector<std::uint32_t> order(list.size());
	std::iota(order.begin(), order.end(), 0u);
	std::stable_sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) {
		const auto x = list.moves(a), y = list.moves(b);
		if (x.size() != y.size()) return x.size() < y.size();
		for (size_t k = 0; k < x.size(); ++k)
			if (x[k] != y[k]) return name_key(x[k]) < name_key(y[k]);
		return false;
	});
	list.reorder(order);
}

size_t unique_list(GameList &list)
{
	const size_t n = list.size();
	std::vector<std::uint32_t> order(n);
	std::iota(order.begin(), order.end(), 0u);
	auto less = [&](std::uint32_t a, std::uint32_t b) {
		const Position &sa = list.start(a), &sb = list.start(b);
		if (!(sa == sb)) {
			if (sa.black != sb.black) return sa.black < sb.black;
			if (sa.white != sb.white) return sa.white < sb.white;
			return sa.side < sb.side;
		}
		const auto x = list.moves(a), y = list.moves(b);
		if (x.size() != y.size()) return x.size() < y.size();
		const int c = std::memcmp(x.data(), y.data(), x.size());
		if (c) return c < 0;
		return a < b;
	};
	std::sort(order.begin(), order.end(), less);
	std::vector<char> keep(n, 1);
	for (size_t k = 1; k < n; ++k)
		if (same_game(list, order[k - 1], order[k])) keep[order[k]] = 0;  // the first (smaller index) stays
	std::vector<std::uint32_t> kept;
	kept.reserve(n);
	for (size_t i = 0; i < n; ++i)
		if (keep[i]) kept.push_back(static_cast<std::uint32_t>(i));
	const size_t removed = n - kept.size();
	if (removed) list.reorder(kept);
	return removed;
}

void truncate_list(GameList &list, int n)
{
	GameList out;
	out.reserve(list.size(), list.size() * static_cast<size_t>(std::max(0, n)));
	for (size_t i = 0; i < list.size(); ++i) {
		const auto m = list.moves(i);
		out.add(list.start(i), m.first(std::min<size_t>(m.size(), static_cast<size_t>(std::max(0, n)))));
	}
	list = std::move(out);
}

void shuffle_list(GameList &list, std::uint64_t seed)
{
	std::vector<std::uint32_t> order(list.size());
	std::iota(order.begin(), order.end(), 0u);
	std::uint64_t s = seed;
	for (size_t i = order.size(); i > 1; --i) {
		const size_t j = static_cast<size_t>(splitmix(s) % i);
		std::swap(order[i - 1], order[j]);
	}
	list.reorder(order);
}

unsigned symmetries_of(const Position &p) noexcept
{
	// a symmetry maps the halves of the board onto each other: the left and right halves (mirror, 180 degrees,
	// 90 degrees), the top and bottom halves (mirror, 180, 90), or the two sides of a diagonal hold as many discs.
	// When none do (nearly every position after the first moves), the position has no symmetry.
	constexpr u64 kLeft = 0x0f0f0f0f0f0f0f0fULL, kTop = 0x00000000ffffffffULL;
	const u64 occ = p.black | p.white;
	{
		// the two sides of the a1-h8 diagonal (x > y, x < y) and of the a8-h1 diagonal (x + y < 7, x + y > 7)
		struct Halves {
			u64 above = 0, below = 0, anti_lo = 0, anti_hi = 0;
		};
		static constexpr Halves tri = [] {
			Halves m;
			for (int sq = 0; sq < 64; ++sq) {
				const int x = sq & 7, y = sq >> 3;
				if (x > y) m.above |= 1ULL << sq;
				if (x < y) m.below |= 1ULL << sq;
				if (x + y < 7) m.anti_lo |= 1ULL << sq;
				if (x + y > 7) m.anti_hi |= 1ULL << sq;
			}
			return m;
		}();
		const bool lr_eq = std::popcount(occ & kLeft) == std::popcount(occ & ~kLeft);
		const bool tb_eq = std::popcount(occ & kTop) == std::popcount(occ & ~kTop);
		const bool d_eq = std::popcount(occ & tri.above) == std::popcount(occ & tri.below);
		const bool a_eq = std::popcount(occ & tri.anti_lo) == std::popcount(occ & tri.anti_hi);
		if (!lr_eq && !tb_eq && !d_eq && !a_eq) return 1;
	}
	// the occupied squares (the 7 images from 3 transforms and byte swaps), then the black discs
	const u64 lr = symmetric(occ, 1), t = symmetric(occ, 4), tlr = symmetric(t, 1);
	const u64 img[8] = {occ, lr, std::byteswap(occ), std::byteswap(lr), t, tlr, std::byteswap(t), std::byteswap(tlr)};
	unsigned bits = 1;
	for (int s = 1; s < 8; ++s)
		if (img[s] == occ && symmetric(p.black, s) == p.black) bits |= 1u << s;
	return bits;
}

namespace {

// plays the moves into `out`, choosing at each symmetric position a symmetry with `pick` (bits of the
// symmetries, the position, the move as it is now) -> the symmetry to apply to the rest
template <class Pick>
void reoriented_into(const Position &start, std::span<const std::uint8_t> moves, Pick pick, std::vector<std::uint8_t> &out)
{
	std::array<std::uint8_t, 64> map;
	for (int i = 0; i < 64; ++i) map[i] = static_cast<std::uint8_t>(i);
	out.clear();
	Position p = start;
	for (const auto m : moves) {
		// a move that turns nothing for the side to move comes after a forced pass (a symmetry keeping the
		// position keeps that too); no list of legal moves is needed for a game read in
		int t = map[m];
		u64 f = flips(p.me(), p.opp(), t);
		if (!f) {
			p.side ^= 1;
			f = flips(p.me(), p.opp(), t);
		}
		const unsigned sym = symmetries_of(p);
		if (sym != 1) {
			const int s = pick(sym, p, t);
			if (s) {
				for (auto &x : map) x = static_cast<std::uint8_t>(symmetric_square(x, s));
				t = map[m];
				f = flips(p.me(), p.opp(), t);
			}
		}
		if (!f) break;  // not a game of legal moves (cannot happen for a list read in)
		out.push_back(static_cast<std::uint8_t>(t));
		const u64 turned = f | (1ULL << t);
		if (p.side == kBlack) {
			p.black |= turned;
			p.white &= ~turned;
		} else {
			p.white |= turned;
			p.black &= ~turned;
		}
		p.side ^= 1;
	}
}

// the first form of a move among the symmetries keeping the position (f5 first)
int normal_pick(unsigned sym, int t)
{
	int best = 0, key = orient_key(t);
	for (int s = 1; s < 8; ++s)
		if ((sym >> s) & 1) {
			const int k = orient_key(symmetric_square(t, s));
			if (k < key) {
				key = k;
				best = s;
			}
		}
	return best;
}

// one of the symmetries in `sym`, drawn with r
int drawn_pick(unsigned sym, std::uint64_t r)
{
	int k = static_cast<int>(r % static_cast<unsigned>(std::popcount(sym)));
	for (int s = 0; s < 8; ++s)
		if ((sym >> s) & 1 && k-- == 0) return s;
	return 0;
}

// Work over many games is split into parts on threads: each part has at least 65,536 games, at most 8 threads
// (the work of a moment; the engines keep the rest of the CPU).
unsigned parts_for(size_t n)
{
	const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
	return static_cast<unsigned>(std::clamp<size_t>(n / 65536, 1, std::min(8u, hw)));
}

template <class F>
void in_parts(size_t n, unsigned k, F f)
{
	if (k <= 1) {
		f(size_t{0}, n, 0u);
		return;
	}
	std::vector<std::jthread> threads;
	for (unsigned part = 0; part < k; ++part) threads.emplace_back(f, n * part / k, n * (part + 1) / k, part);
}

// The list made again from new moves of every game (`make(i, out)`), in parts; false when stopped (the list
// stays as it was).
template <class Make>
bool rebuild(GameList &list, std::stop_token stop, Make make)
{
	const size_t n = list.size();
	const unsigned k = parts_for(n);
	struct Part {
		std::vector<std::uint8_t> pool, lens;
	};
	std::vector<Part> parts(k);
	std::atomic<bool> stopped{false};
	in_parts(n, k, [&](size_t b, size_t e, unsigned part) {
		Part &pt = parts[part];
		pt.lens.reserve(e - b);
		pt.pool.reserve((e - b) * 60);
		std::vector<std::uint8_t> buf;
		buf.reserve(64);
		for (size_t i = b; i < e; ++i) {
			if ((i & 4095) == 0 && (stop.stop_requested() || stopped)) {
				stopped = true;
				return;
			}
			make(i, buf);
			pt.pool.insert(pt.pool.end(), buf.begin(), buf.end());
			pt.lens.push_back(static_cast<std::uint8_t>(buf.size()));
		}
	});
	if (stopped) return false;
	size_t total = 0;
	for (const auto &pt : parts) total += pt.pool.size();
	GameList out;
	out.reserve(n, total);
	size_t i = 0;
	for (const auto &pt : parts) {
		size_t off = 0;
		for (const auto len : pt.lens) {
			out.add(list.start(i++), std::span<const std::uint8_t>(pt.pool.data() + off, len));
			off += len;
		}
	}
	list = std::move(out);
	return true;
}

}  // namespace

std::vector<std::uint8_t> normalized_moves(const Position &start, std::span<const std::uint8_t> moves)
{
	std::vector<std::uint8_t> out;
	reoriented_into(start, moves, [](unsigned sym, const Position &, int t) { return normal_pick(sym, t); }, out);
	return out;
}

std::vector<std::uint8_t> scattered_moves(const Position &start, std::span<const std::uint8_t> moves,
                                          std::mt19937_64 &rng, int sides)
{
	std::vector<std::uint8_t> out;
	reoriented_into(start, moves, [&](unsigned sym, const Position &p, int) {
		return (sides >> p.side) & 1 ? drawn_pick(sym, rng()) : 0;
	}, out);
	return out;
}

void normalize_list(GameList &list, std::stop_token stop)
{
	rebuild(list, stop, [&](size_t i, std::vector<std::uint8_t> &out) {
		reoriented_into(list.start(i), list.moves(i), [](unsigned sym, const Position &, int t) { return normal_pick(sym, t); }, out);
	});
}

void scatter_list(GameList &list, std::uint64_t seed, int sides, std::stop_token stop)
{
	// every game draws from its own numbers (the seed and its place): the same list whatever the threads
	rebuild(list, stop, [&](size_t i, std::vector<std::uint8_t> &out) {
		std::uint64_t r = seed ^ (static_cast<std::uint64_t>(i) * 0xD1B54A32D192ED03ULL);
		reoriented_into(list.start(i), list.moves(i), [&](unsigned sym, const Position &p, int) {
			return (sides >> p.side) & 1 ? drawn_pick(sym, splitmix(r)) : 0;
		}, out);
	});
}

std::vector<ListHit> search_list(const GameList &list, const Position &target, bool symmetries, std::stop_token stop)
{
	const int td = std::popcount(target.black | target.white);
	Position ts[8];
	const int ns = symmetries ? 8 : 1;
	for (int s = 0; s < ns; ++s) ts[s] = symmetric(target, s);
	auto match = [&](const Position &q) {
		for (int s = 0; s < ns; ++s)
			if (q == ts[s]) return s;
		return -1;
	};
	const unsigned k = parts_for(list.size());
	std::vector<std::vector<ListHit>> found(k);
	in_parts(list.size(), k, [&](size_t b, size_t e, unsigned part) {
		for (size_t i = b; i < e; ++i) {
			if ((i & 4095) == 0 && stop.stop_requested()) break;
			const Position &st = list.start(i);
			// a move adds one disc: the position can only be after this number of moves
			const int kk = td - std::popcount(st.black | st.white);
			const auto mv = list.moves(i);
			if (kk < 0 || kk > static_cast<int>(mv.size())) continue;
			Position p = st;
			for (int j = 0; j < kk; ++j) play_known(p, mv[j]);
			int m = match(p);
			if (m < 0 && p.must_pass()) m = match(p.play(kPass));
			if (m >= 0) found[part].push_back({static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(kk), static_cast<std::uint8_t>(m)});
		}
	});
	std::vector<ListHit> hits;
	for (auto &f : found) hits.insert(hits.end(), f.begin(), f.end());
	return hits;
}

}  // namespace ng
