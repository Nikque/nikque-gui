// Looking up positions in a book file while the main engine reads it (it answers nothing until then). A few blocks of
// the file are read for each position; the file holds its positions in the order of their hash numbers (in both forms
// of a book, "XADEKOOB" and "NEDXBOOK"). A book not in that order is not looked up: the values wait for the engine.
#pragma once

#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "analysis.h"
#include "rules.h"

namespace ng {

using u32 = std::uint32_t;

// a position as a book holds it: the smallest of its 8 symmetries, the side to move first
struct BookBoard {
	u64 player = 0, opponent = 0;
	int symmetry = 0;  // the symmetry that makes it from the position
};
BookBoard book_board(const Position &p) noexcept;
u64 book_hash(u64 player, u64 opponent) noexcept;

struct BookRecord {
	int value = 0;
	int level = 0;
	long long lines = 0;
	int symmetry = 0;       // of the position asked
	int leaf = kNoMove;     // the leaf move, on the board of the record (kPass too)
	int leaf_value = 0;
};

class BookFile {
public:
	BookFile() = default;
	BookFile(const BookFile &) = delete;
	BookFile &operator=(const BookFile &) = delete;
	~BookFile() { close(); }
	// false: the file is not of the form, or its positions are not in order
	bool open(const std::wstring &path);
	void close();
	std::optional<BookRecord> find(const Position &p);
	bool usable() const noexcept { return file_ != INVALID_HANDLE_VALUE && !failed_; }

private:
	bool read(u64 at, unsigned char *out, size_t size, size_t *got);
	bool find_bounds(u32 target, u64 *from);
	u32 bucket(u64 player, u64 opponent) const noexcept;

	HANDLE file_ = INVALID_HANDLE_VALUE;
	int form_ = 1;              // 1 "XADEKOOB" (Edax), 2 "NEDXBOOK" (newedax)
	u64 start_ = 0, end_ = 0;   // where the positions begin and end
	u32 buckets_ = 0;
	u32 gap_ = 0;               // the most buckets between two positions next to each other in the file
	bool failed_ = false;
	std::map<u32, u64> marks_;  // places found: the number of a bucket, the offset of a position of it
	std::vector<unsigned char> block_;
};

// what the book says of the moves of a position (the same values as the lines of book moveinfo)
struct BookPeekMove {
	int sq = kNoMove;
	bool in_book = false;
	int value = 0;
	int level = 0;
	long long lines = -1;
	bool leaf = false;
};
struct BookPeekAnswer {
	Position pos;
	unsigned generation = 0;
	bool in_book = false;
	std::vector<BookPeekMove> moves;
};
// (`stop`: gives up between the moves; the answer is then not whole)
BookPeekAnswer book_peek_moves(BookFile &file, const Position &p, const std::atomic_bool *stop = nullptr);

// The lookups on a thread of their own. Each position asked is answered by a message to the window.
class BookPeek {
public:
	BookPeek() = default;
	BookPeek(const BookPeek &) = delete;
	BookPeek &operator=(const BookPeek &) = delete;
	~BookPeek() { stop(); }
	void start(const std::wstring &path, HWND notify, UINT msg, unsigned generation);
	void stop();
	bool running() const noexcept { return thread_.joinable(); }
	void ask(std::vector<Position> positions);  // in place of the ones not looked up yet
	void take(std::vector<BookPeekAnswer> &out);

private:
	void run(std::wstring path, HWND notify, UINT msg, unsigned generation);

	std::thread thread_;
	std::mutex mutex_;
	std::condition_variable wake_;
	std::atomic_bool quit_ = false;
	std::vector<Position> wanted_;
	std::vector<BookPeekAnswer> answers_;
};

}  // namespace ng
