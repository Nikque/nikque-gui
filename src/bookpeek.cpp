#include "bookpeek.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>

namespace ng {

namespace {

constexpr size_t kHeader = 42;       // form 1: "XADEKOOB", version, release, date, settings, number of positions
constexpr size_t kFixed = 40;        // of a position: the board, the counts, the values, the number of moves, the level
constexpr size_t kHeader2 = 64;      // form 2: "NEDXBOOK", the smallest header (its size is in it), and 16 bytes after the positions
constexpr size_t kTrailer2 = 16;
constexpr size_t kFixed2 = 23;       // of a position: the board, the number of moves, the level, the values, the leaf
constexpr int kNoMoveInFile = 65;
constexpr int kMaxMoves = 33;
constexpr size_t kSample = 1 << 20;  // the start of the file, to see the order of the positions
constexpr size_t kBlock = 1 << 16;   // read to find a place in the file
constexpr u64 kNear = 1 << 18;       // this close, the positions are read one after the other
constexpr int kChain = 8;            // positions in order: the place found begins a position

struct Row {
	u64 player = 0, opponent = 0;
	unsigned lines = 0;
	int value = 0, level = 0, leaf = kNoMoveInFile, leaf_value = 0;
	size_t size = 0;
};

constexpr std::array<u32, 256> crc_table()
{
	std::array<u32, 256> t{};
	for (u32 i = 0; i < 256; ++i) {
		u32 c = i;
		for (int k = 0; k < 8; ++k) c = c & 1 ? (c >> 1) ^ 0x82F63B78u : c >> 1;  // CRC-32C
		t[i] = c;
	}
	return t;
}
constexpr auto kCrc = crc_table();

u32 crc_word(u32 crc, u64 x) noexcept
{
	for (int k = 0; k < 8; ++k) {
		crc = kCrc[(crc ^ static_cast<u32>(x)) & 255] ^ (crc >> 8);
		x >>= 8;
	}
	return crc;
}

template <class T>
T take(const unsigned char *p)
{
	T x;
	std::memcpy(&x, p, sizeof x);
	return x;
}

// a count of a position of form 2 (7 bits a byte, at most 32 bits), at `p`, of the `n` bytes there: its bytes, 0 if it is none
size_t parse_count(const unsigned char *p, size_t n, unsigned *out) noexcept
{
	unsigned v = 0;
	for (size_t k = 0; k < 5 && k < n; ++k) {
		if (k == 4 && p[k] > 0x0f) return 0;
		v |= static_cast<unsigned>(p[k] & 0x7f) << (7 * k);
		if (!(p[k] & 0x80)) {
			*out = v;
			return k + 1;
		}
	}
	return 0;
}

bool parse_row2(const unsigned char *p, size_t n, Row *r) noexcept
{
	if (n < kFixed2 + 4) return false;
	r->player = take<u64>(p);
	r->opponent = take<u64>(p + 8);
	if (r->player & r->opponent) return false;
	const int links = p[16];
	const int leaf = p[22];
	if (links > kMaxMoves || p[17] > 60 || leaf > kNoMoveInFile || links + (leaf != kNoMoveInFile) > kMaxMoves) return false;
	size_t at = kFixed2;
	unsigned count = 0;
	for (int k = 0; k < 4; ++k) {  // wins, draws, losses, lines
		const size_t used = parse_count(p + at, n - at, &count);
		if (!used) return false;
		at += used;
	}
	r->size = at + 2 * static_cast<size_t>(links);
	if (n < r->size) return false;
	for (int k = 0; k < links; ++k)
		if (p[at + 2 * k + 1] > kPass) return false;
	r->lines = count;
	r->value = static_cast<signed char>(p[18]);
	r->level = p[17];
	r->leaf = leaf;
	r->leaf_value = static_cast<signed char>(p[21]);
	return true;
}

// the position at `p`, of the `n` bytes there; false when they cannot be one (or it does not end there)
bool parse_row(const unsigned char *p, size_t n, int form, Row *r) noexcept
{
	if (form == 2) return parse_row2(p, n, r);
	if (n < kFixed) return false;
	r->player = take<u64>(p);
	r->opponent = take<u64>(p + 8);
	if (r->player & r->opponent) return false;
	const int links = p[38];
	r->size = kFixed + 2 * (static_cast<size_t>(links) + 1);
	if (links > kMaxMoves || p[39] > 60 || n < r->size) return false;
	for (int k = 0; k < links; ++k)
		if (p[kFixed + 2 * k + 1] > kPass) return false;
	const unsigned char *leaf = p + kFixed + 2 * links;
	if (leaf[1] > kNoMoveInFile || links + (leaf[1] != kNoMoveInFile) > kMaxMoves) return false;
	r->lines = take<unsigned>(p + 28);
	r->value = std::clamp<int>(take<short>(p + 32), -127, 127);
	r->level = p[39];
	r->leaf = leaf[1];
	r->leaf_value = static_cast<signed char>(leaf[0]);
	return true;
}

u32 buckets_for(u64 positions) noexcept
{
	u64 b = 1 << 16;
	while (b < (1u << 28) && b * 16 < positions) b <<= 1;
	return static_cast<u32>(b);
}

}  // namespace

BookBoard book_board(const Position &p) noexcept
{
	BookBoard best;
	for (int s = 0; s < 8; ++s) {
		const Position q = symmetric(p, s);
		if (s == 0 || q.me() < best.player || (q.me() == best.player && q.opp() < best.opponent)) {
			best.player = q.me();
			best.opponent = q.opp();
			best.symmetry = s;
		}
	}
	return best;
}

u64 book_hash(u64 player, u64 opponent) noexcept
{
	const u32 a = crc_word(0, player);
	return static_cast<u64>(a) << 32 | crc_word(a, opponent);
}

u32 BookFile::bucket(u64 player, u64 opponent) const noexcept
{
	return static_cast<u32>(book_hash(player, opponent) & (buckets_ - 1));
}

void BookFile::close()
{
	if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
	file_ = INVALID_HANDLE_VALUE;
	marks_.clear();
	buckets_ = 0;
	failed_ = false;
}

bool BookFile::read(u64 at, unsigned char *out, size_t size, size_t *got)
{
	OVERLAPPED o{};
	o.Offset = static_cast<DWORD>(at);
	o.OffsetHigh = static_cast<DWORD>(at >> 32);
	DWORD n = 0;
	if (!ReadFile(file_, out, static_cast<DWORD>(size), &n, &o)) {
		if (GetLastError() != ERROR_HANDLE_EOF) return false;
		n = 0;
	}
	*got = n;
	return true;
}

bool BookFile::open(const std::wstring &path)
{
	close();
	// (the engine may save its book over the file: this handle does not keep it from that)
	file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
	                    FILE_FLAG_RANDOM_ACCESS, nullptr);
	if (file_ == INVALID_HANDLE_VALUE) return false;
	LARGE_INTEGER size{};
	std::vector<unsigned char> head((1 << 16) + kSample);
	size_t got = 0;
	if (!GetFileSizeEx(file_, &size) || !read(0, head.data(), head.size(), &got) || got < kHeader2) {
		close();
		return false;
	}
	u64 count = 0;
	end_ = static_cast<u64>(size.QuadPart);
	if (std::memcmp(head.data(), "XADEKOOB", 8) == 0 && head[8] == 4) {
		form_ = 1;
		start_ = kHeader;
		count = take<u32>(head.data() + 38);
	} else if (std::memcmp(head.data(), "NEDXBOOK", 8) == 0 && take<unsigned short>(head.data() + 8) == 2 &&
	           take<unsigned short>(head.data() + 10) >= kHeader2 && take<u32>(head.data() + 12) == 0 && head[18] == 1 &&
	           end_ >= take<unsigned short>(head.data() + 10) + kTrailer2) {
		form_ = 2;
		start_ = take<unsigned short>(head.data() + 10);
		count = take<u64>(head.data() + 48);
		end_ -= kTrailer2;
	} else {
		close();
		return false;
	}
	// the positions of the start of the file: each one as a book holds it, and their hash numbers
	std::vector<u32> hash;
	size_t at = static_cast<size_t>(start_);
	const size_t end = static_cast<size_t>(std::min<u64>(got, end_));
	while (hash.size() < count) {
		Row r;
		if (at > end || !parse_row(head.data() + at, end - at, form_, &r)) break;
		Position q;
		q.black = r.player;
		q.white = r.opponent;
		const BookBoard b = book_board(q);
		if (b.player != r.player || b.opponent != r.opponent) {
			close();
			return false;
		}
		hash.push_back(static_cast<u32>(book_hash(r.player, r.opponent)));
		at += r.size;
	}
	if (hash.empty() || (hash.size() == count && at != end_)) {
		close();
		return false;
	}
	// the number of buckets of the book that wrote the file: the one the engine gives a book of this size, else the
	// largest one with the positions in order
	auto in_order = [&](u32 n) {
		for (size_t k = 1; k < hash.size(); ++k)
			if ((hash[k] & (n - 1)) < (hash[k - 1] & (n - 1))) return false;
		return true;
	};
	u32 n = buckets_for(count);
	if (!in_order(n)) {
		n = 1u << 28;
		while (n > 256 && !in_order(n)) n >>= 1;
		if (!in_order(n)) {
			close();
			return false;
		}
	}
	buckets_ = n;
	gap_ = static_cast<u32>(std::clamp<u64>(u64{n} * 64 / count, 1, n));  // (far more than positions next to each other are apart)
	block_.resize(kBlock);
	return true;
}

// a place of the file before the positions of the bucket `target`, close to them
bool BookFile::find_bounds(u32 target, u64 *from)
{
	u64 lo = start_, hi = end_;
	u32 lo_bucket = 0, hi_bucket = buckets_;
	if (auto it = marks_.lower_bound(target); it != marks_.end()) {
		hi = it->second;
		hi_bucket = it->first;
	}
	if (auto it = marks_.lower_bound(target); it != marks_.begin()) {
		--it;
		lo = it->second;
		lo_bucket = it->first;
	}
	for (int round = 0; hi > lo && hi - lo > kNear; ++round) {
		if (round > 200) return false;
		const u64 span = hi - lo;
		const u64 part = hi_bucket > lo_bucket ? target - lo_bucket : 0;
		// where the bucket should be if the positions are spread evenly; in the middle when that does not close in on it
		u64 mid = round < 8 ? lo + static_cast<u64>(static_cast<double>(span) * part / std::max<u32>(hi_bucket - lo_bucket, 1)) : lo + span / 2;
		mid = std::clamp(mid, lo + span / 16, hi - span / 16);
		if (form_ == 1) mid -= (mid - start_) & 1;  // (every position takes an even number of bytes)
		size_t got = 0;
		if (!read(mid, block_.data(), block_.size(), &got)) return false;
		got = static_cast<size_t>(std::min<u64>(got, end_ - mid));
		// The first place that begins positions as a book holds them, each one close after the last. Bytes in a position
		// may read as one that ends where the next begins: the place kept is that of the second one.
		u64 place = 0;
		u32 first = 0;
		bool found = false;
		for (size_t k = 0; k < got && !found; k += form_ == 1 ? 2 : 1) {
			size_t at = k;
			u32 last = 0;
			int chain = 0;
			for (; chain < kChain; ++chain) {
				Row r;
				if (!parse_row(block_.data() + at, got - at, form_, &r)) break;
				Position q;
				q.black = r.player;
				q.white = r.opponent;
				const BookBoard board = book_board(q);
				if (board.player != r.player || board.opponent != r.opponent) break;
				const u32 b = bucket(r.player, r.opponent);
				if (chain && (b < last || b - last > gap_)) break;
				if (chain == 1) {
					place = mid + at;
					first = b;
				}
				last = b;
				at += r.size;
			}
			found = chain == kChain;
		}
		if (!found) return false;
		if (place >= hi) {
			hi = mid;  // (no position begins between them)
			continue;
		}
		if (marks_.size() > (1 << 20)) marks_.clear();
		marks_.try_emplace(first, place);
		if (first < target) {
			lo = place;
			lo_bucket = first;
		} else {
			hi = place;
			hi_bucket = first;
		}
	}
	*from = lo;
	return true;
}

std::optional<BookRecord> BookFile::find(const Position &p)
{
	if (!usable()) return std::nullopt;
	const BookBoard want = book_board(p);
	const u32 target = bucket(want.player, want.opponent);
	u64 at = 0;
	if (!find_bounds(target, &at)) {
		failed_ = true;
		return std::nullopt;
	}
	// the positions one after the other, up to the first of a later bucket
	for (;;) {
		size_t got = 0;
		if (at >= end_) return std::nullopt;
		if (!read(at, block_.data(), block_.size(), &got)) {
			failed_ = true;
			return std::nullopt;
		}
		got = static_cast<size_t>(std::min<u64>(got, end_ - at));
		size_t k = 0;
		for (;;) {
			Row r;
			if (k >= got || !parse_row(block_.data() + k, got - k, form_, &r)) break;
			const u32 b = bucket(r.player, r.opponent);
			if (b > target) return std::nullopt;
			if (r.player == want.player && r.opponent == want.opponent) {
				BookRecord out;
				out.value = r.value;
				out.level = r.level;
				out.lines = r.lines;
				out.symmetry = want.symmetry;
				out.leaf = r.leaf >= kNoMoveInFile ? kNoMove : r.leaf;
				out.leaf_value = r.leaf_value;
				return out;
			}
			k += r.size;
		}
		if (at + k >= end_) return std::nullopt;
		if (k == 0) {  // (not a position: the file is not what it seemed)
			failed_ = true;
			return std::nullopt;
		}
		at += k;
	}
}

BookPeekAnswer book_peek_moves(BookFile &file, const Position &p, const std::atomic_bool *stop)
{
	BookPeekAnswer a;
	a.pos = p;
	const std::optional<BookRecord> here = file.find(p);
	a.in_book = here.has_value();
	u64 m = p.moves();
	std::vector<int> moves;
	if (m == 0) {
		if (!p.game_over()) moves.push_back(kPass);
	}
	for (; m; m &= m - 1) moves.push_back(std::countr_zero(m));
	for (const int sq : moves) {
		if (stop && *stop) break;
		BookPeekMove e;
		e.sq = sq;
		if (const auto next = file.find(p.play(sq))) {
			e.in_book = true;
			e.value = -next->value;
			e.level = next->level;
			e.lines = next->lines;
		} else if (here && here->leaf != kNoMove && (sq == kPass ? here->leaf == kPass : symmetric_square(sq, here->symmetry) == here->leaf)) {
			e.leaf = true;
			e.value = here->leaf_value;
			e.level = here->level;
		}
		a.moves.push_back(e);
	}
	return a;
}

void BookPeek::start(const std::wstring &path, HWND notify, UINT msg, unsigned generation)
{
	stop();
	quit_ = false;
	wanted_.clear();
	answers_.clear();
	thread_ = std::thread([this, path, notify, msg, generation] { run(path, notify, msg, generation); });
}

void BookPeek::stop()
{
	if (!thread_.joinable()) return;
	{
		std::lock_guard lock(mutex_);
		quit_ = true;
	}
	wake_.notify_all();
	thread_.join();
}

void BookPeek::ask(std::vector<Position> positions)
{
	{
		std::lock_guard lock(mutex_);
		wanted_ = std::move(positions);
	}
	wake_.notify_all();
}

void BookPeek::take(std::vector<BookPeekAnswer> &out)
{
	std::lock_guard lock(mutex_);
	out.swap(answers_);
	answers_.clear();
}

void BookPeek::run(std::wstring path, HWND notify, UINT msg, unsigned generation)
{
	BookFile file;
	if (!file.open(path)) return;
	for (;;) {
		Position p;
		{
			std::unique_lock lock(mutex_);
			wake_.wait(lock, [this] { return quit_ || !wanted_.empty(); });
			if (quit_) return;
			p = wanted_.front();
			wanted_.erase(wanted_.begin());
		}
		BookPeekAnswer a = book_peek_moves(file, p, &quit_);
		if (quit_ || !file.usable()) return;
		a.generation = generation;
		{
			std::lock_guard lock(mutex_);
			answers_.push_back(std::move(a));
		}
		PostMessageW(notify, msg, 0, 0);
	}
}

}  // namespace ng
