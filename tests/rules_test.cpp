// Tests of the rules: known move counts (perft), a plain square-by-square implementation on random games,
// the game record with passes, and reading coordinates from text.
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>

#include "../src/game.h"
#include "../src/rules.h"

using namespace ng;

static int failures = 0;
#define CHECK(c)                                                              \
	do {                                                                      \
		if (!(c)) {                                                           \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);          \
			++failures;                                                       \
		}                                                                     \
	} while (0)

static unsigned long long perft(const Position &p, int depth)
{
	if (depth == 0) return 1;
	u64 m = p.moves();
	if (m == 0) {
		if (p.game_over()) return 1;
		return perft(p.play(kPass), depth - 1);
	}
	if (depth == 1) return static_cast<unsigned long long>(std::popcount(m));
	unsigned long long n = 0;
	while (m) {
		const int sq = static_cast<int>(std::countr_zero(m));
		m &= m - 1;
		n += perft(p.play(sq), depth - 1);
	}
	return n;
}

// a plain implementation: walks from the square in the 8 directions
static u64 plain_flips(u64 me, u64 opp, int sq)
{
	if (((me | opp) >> sq) & 1) return 0;
	u64 all = 0;
	const int x0 = sq & 7, y0 = sq >> 3;
	for (int dy = -1; dy <= 1; ++dy)
		for (int dx = -1; dx <= 1; ++dx) {
			if (!dx && !dy) continue;
			u64 run = 0;
			int x = x0 + dx, y = y0 + dy;
			while (x >= 0 && x < 8 && y >= 0 && y < 8 && ((opp >> (y * 8 + x)) & 1)) {
				run |= 1ULL << (y * 8 + x);
				x += dx;
				y += dy;
			}
			if (run && x >= 0 && x < 8 && y >= 0 && y < 8 && ((me >> (y * 8 + x)) & 1)) all |= run;
		}
	return all;
}

static void test_perft()
{
	const unsigned long long expect[] = {1, 4, 12, 56, 244, 1396, 8200, 55092, 390216, 3005288, 24571284};
	for (int d = 0; d <= 10; ++d) {
		const unsigned long long n = perft(Position{}, d);
		if (n != expect[d]) std::printf("perft %d: %llu (expected %llu)\n", d, n, expect[d]);
		CHECK(n == expect[d]);
	}
}

static void test_random_games()
{
	std::mt19937_64 rng(12345);
	long long moves_checked = 0;
	for (int g = 0; g < 20000; ++g) {
		Position p;
		while (!p.game_over()) {
			u64 plain_moves = 0;
			for (int sq = 0; sq < 64; ++sq) {
				const u64 f = plain_flips(p.me(), p.opp(), sq);
				CHECK(f == flips(p.me(), p.opp(), sq));
				if (f) plain_moves |= 1ULL << sq;
			}
			CHECK(plain_moves == p.moves());
			if (plain_moves == 0) {
				CHECK(p.must_pass());
				p = p.play(kPass);
				continue;
			}
			int k = static_cast<int>(rng() % std::popcount(plain_moves));
			u64 m = plain_moves;
			while (k--) m &= m - 1;
			const int sq = static_cast<int>(std::countr_zero(m));
			const Position q = p.play(sq);
			CHECK(std::popcount(q.black | q.white) == std::popcount(p.black | p.white) + 1);
			CHECK((q.black & q.white) == 0);
			p = q;
			++moves_checked;
		}
	}
	std::printf("random games: %lld moves checked\n", moves_checked);
}

static void test_game_record()
{
	Game g;
	CHECK(g.now() == Position{});
	CHECK(!g.back());
	CHECK(g.play(parse_square("f5")));
	CHECK(g.play(parse_square("d6")));
	CHECK(!g.play(parse_square("a1")));
	CHECK(g.cursor() == 2);
	CHECK(g.back() && g.cursor() == 1);
	CHECK(g.play(parse_square("d6")) && g.size() == 2);  // the same move keeps the line
	CHECK(g.back());
	CHECK(g.play(parse_square("f6")) && g.size() == 2 && g.moves()[1] == parse_square("f6"));  // another move cuts it
	CHECK(g.to_text() == "f5f6");

	const ParsedMoves pm = parse_moves("棋譜：Ｆ５ｄ６ c3, black e3! 12 h9 F4 x1");
	CHECK(pm.moves.size() == 5);
	if (pm.moves.size() == 5) {
		CHECK(pm.moves[0] == parse_square("f5") && pm.moves[1] == parse_square("d6") &&
		      pm.moves[2] == parse_square("c3") && pm.moves[3] == parse_square("e3") &&
		      pm.moves[4] == parse_square("f4"));
	}
	CHECK(parse_moves("Edax 4.5.5 version").moves.empty());
}

static void test_symmetry()
{
	for (int sq = 0; sq < 64; ++sq) {
		const int x = sq & 7, y = sq >> 3;
		CHECK(symmetric_square(sq, 0) == sq);
		CHECK(symmetric_square(sq, 1) == y * 8 + (7 - x));
		CHECK(symmetric_square(sq, 2) == (7 - y) * 8 + x);
		CHECK(symmetric_square(sq, 3) == (7 - y) * 8 + (7 - x));
		CHECK(symmetric_square(sq, 4) == x * 8 + y);
	}
	// playing then turning the board is turning then playing the turned move
	std::mt19937_64 rng(7);
	for (int g = 0; g < 2000; ++g) {
		Position p;
		while (!p.game_over()) {
			u64 m = p.moves();
			int sq = kPass;
			if (m) {
				int k = static_cast<int>(rng() % std::popcount(m));
				while (k--) m &= m - 1;
				sq = std::countr_zero(m);
			}
			const Position q = p.play(sq);
			for (int s = 0; s < 8; ++s)
				CHECK(symmetric(q, s) == symmetric(p, s).play(symmetric_square(sq, s)));
			p = q;
		}
	}
	// the 8 boards of a position with no symmetry left are 8 different boards
	Game g;
	for (const char *mv : {"f5", "d6", "c3", "d3", "c4", "f4", "f6"}) g.play(parse_square(mv));
	for (int a = 0; a < 8; ++a)
		for (int b = a + 1; b < 8; ++b) CHECK(!(symmetric(g.now(), a) == symmetric(g.now(), b)));
}

// the test books' games: passes in the middle and at the end
static void test_games_file(const char *path)
{
	std::ifstream in(path);
	if (!in) {
		std::printf("skip %s\n", path);
		return;
	}
	std::string line;
	int games = 0;
	while (std::getline(in, line)) {
		if (line.empty()) continue;
		const ParsedMoves pm = parse_moves(line);
		Game g;
		const int n = replay(g, Position{}, pm.moves);
		CHECK(n == static_cast<int>(pm.moves.size()));
		const Position &end = g.now();
		CHECK(end.empties() == 60 - n);
		// back to the start and forward again over the passes
		while (g.back()) {}
		CHECK(g.cursor() == 0);
		while (g.forward()) {}
		CHECK(g.now() == end);
		++games;
	}
	std::printf("%s: %d games\n", path, games);
}

int main(int argc, char **argv)
{
	test_perft();
	test_random_games();
	test_game_record();
	test_symmetry();
	for (int i = 1; i < argc; ++i) test_games_file(argv[i]);
	if (failures) {
		std::printf("%d failures\n", failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
