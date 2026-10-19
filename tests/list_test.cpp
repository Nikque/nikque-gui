// Tests of the game list (reading, writing, sorting, joining, cutting, shuffling, orientations, searching),
// the symmetries, the bitboards and the line found without the book; then timings on a large list.
// Arguments: the game files of the test books (one game a line).
#include <bit>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <string>

#include "../src/game.h"
#include "../src/gamelist.h"
#include "../src/linefind.h"
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

static double ms_since(std::chrono::steady_clock::time_point t)
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
}

// the position after the moves (passes added)
static Position play_all(const Position &start, std::span<const std::uint8_t> moves, size_t n = 99)
{
	Position p = start;
	for (size_t i = 0; i < moves.size() && i < n; ++i) {
		if (p.must_pass()) p = p.play(kPass);
		p = p.play(moves[i]);
	}
	return p;
}

static bool legal_game(const Position &start, std::span<const std::uint8_t> moves)
{
	Position p = start;
	for (const auto m : moves) {
		if (p.must_pass()) p = p.play(kPass);
		if (!p.can_play(m)) return false;
		p = p.play(m);
	}
	return true;
}

// a random game of up to `len` moves
static std::vector<std::uint8_t> random_game(std::mt19937_64 &rng, int len)
{
	std::vector<std::uint8_t> out;
	Position p;
	while (static_cast<int>(out.size()) < len && !p.game_over()) {
		if (p.must_pass()) p = p.play(kPass);
		u64 m = p.moves();
		int k = static_cast<int>(rng() % static_cast<unsigned>(std::popcount(m)));
		while (k--) m &= m - 1;
		const int sq = std::countr_zero(m);
		out.push_back(static_cast<std::uint8_t>(sq));
		p = p.play(sq);
	}
	return out;
}

static void test_symmetries()
{
	for (int a = 0; a < 8; ++a) {
		CHECK(symmetry_compose(a, symmetry_inverse(a)) == 0);
		for (int b = 0; b < 8; ++b) {
			const int c = symmetry_compose(a, b);
			for (int sq = 0; sq < 64; ++sq) CHECK(symmetric_square(sq, c) == symmetric_square(symmetric_square(sq, a), b));
		}
	}
	// the usual start is kept by 4 symmetries
	CHECK(symmetries_of(Position{}) == ((1u << 0) | (1u << 3) | (1u << 4) | (1u << 7)));
	// a game turned keeps its passes and its place
	Game g;
	replay(g, Position{}, parse_moves("f5d6c3d3c4f4c5b3c2").moves);
	g.go_to(5);
	for (int s = 0; s < 8; ++s) {
		const Game t = transformed(g, s);
		CHECK(t.size() == g.size() && t.cursor() == g.cursor());
		CHECK(t.now() == symmetric(g.now(), s));
		const Game back = transformed(t, symmetry_inverse(s));
		CHECK(back.moves() == g.moves());
	}
}

static void test_bitboards()
{
	Position p;
	for (const char *mv : {"f5", "d6", "c3"}) p = p.play(parse_square(mv));
	for (BitForm f : {BitForm::Decimal, BitForm::Hex}) {
		auto q = parse_bitboards(bitboard_text(p, f), false);
		CHECK(q && *q == p);
	}
	auto n = parse_bitboards(bitboard_text(p, BitForm::Ntest), true);
	CHECK(n && *n == p);
	CHECK(bitboard_text(Position{}, BitForm::Decimal) == "34628173824 68853694464 X");
	CHECK(bitboard_text(Position{}, BitForm::Hex) == "0x0000000810000000 0x0000001008000000 X");
	auto w = parse_bitboards("black: 0x0000000810000000, white: 0x0000001008000000 white", false);
	CHECK(w && w->black == Position{}.black && w->white == Position{}.white && w->side == kWhite);
	auto j = parse_bitboards("0000000810000000 0000001008000000 \xE7\x99\xBD", false);  // 16 hex digits, "white"
	CHECK(j && j->side == kWhite && j->black == Position{}.black);
	CHECK(!parse_bitboards("1 1", false));             // overlapping
	CHECK(!parse_bitboards("f5d6c3", false));
	CHECK(!parse_bitboards("34628173824", false));     // one number
	CHECK(!parse_bitboards("1 2", false));             // too few discs
	CHECK(obf_text(Position{}) == "---------------------------OX------XO--------------------------- X;");
}

static void test_list_basics()
{
	const std::string text =
		"f5d6c3d3c4\r\n"
		"# a note\n"
		"\n"
		"F5D6C3D3C4\n"                                  // the same game
		"f5f6\n"
		"f5d6a1c3\n"                                    // cut at a1
		"---------------------------OX------XO--------------------------- O;D6C6\n"  // white to move first
		"1. f5 2. d6 3. c3\n";                          // coordinates in other text
	GameList l;
	const ListReadStats st = read_list_text(text, l);
	CHECK(st.games == 6 && st.cut == 1 && l.size() == 6);
	CHECK(l.moves(0).size() == 5 && l.moves(3).size() == 2);
	CHECK(l.start(4).side == kWhite && l.moves(4).size() == 2);
	CHECK(list_line(l.start(0), l.moves(0)) == "f5d6c3d3c4");
	CHECK(list_line(l.start(4), l.moves(4)) == "---------------------------OX------XO--------------------------- O;d6c6");

	// ASCII text is read on its own path: the same coordinates as parse_moves (and full-width text)
	for (const char *t : {"black f5, white d6 (12) c3 x1 h9 Edax 4.5.5 d3", "F5d6C3D3c4 f4", "a1b2 f5", "f5d6c3d3c4f4c5b3c2e6"}) {
		Position s0;
		std::vector<std::uint8_t> mv;
		bool cut;
		parse_list_line(t, &s0, &mv, &cut);
		Game g;
		replay(g, Position{}, parse_moves(t).moves);
		std::string a = list_line(s0, mv), b = g.to_text();
		CHECK(a == b);
	}
	{
		Position s0;
		std::vector<std::uint8_t> mv;
		bool cut;
		CHECK(parse_list_line("\xEF\xBD\x86\xEF\xBC\x95 d6", &s0, &mv, &cut) && mv.size() == 2);  // "ｆ５ d6"
	}

	// a file written and read again
	const std::wstring path = (std::filesystem::temp_directory_path() / "ng_list_test.txt").wstring();
	CHECK(write_list(path, l));
	GameList r;
	CHECK(read_list(path, r).games == l.size());
	for (size_t i = 0; i < l.size(); ++i) CHECK(list_line(r.start(i), r.moves(i)) == list_line(l.start(i), l.moves(i)));
	std::filesystem::remove(path);

	GameList u = l;
	CHECK(unique_list(u) == 1 && u.size() == 5);
	CHECK(list_line(u.start(1), u.moves(1)) == "f5f6");  // the order of the first ones is kept
	GameList s = u;
	sort_list(s);
	CHECK(s.moves(0).size() == 2 && s.moves(s.size() - 1).size() == 5);
	// same length: by the names of the moves (f5d6 < f5f6)
	GameList t;
	read_list_text("f5f6\nf5d6\nd3c3\n", t);
	sort_list(t);
	CHECK(list_line(t.start(0), t.moves(0)) == "d3c3" && list_line(t.start(1), t.moves(1)) == "f5d6");
	GameList c = l;
	truncate_list(c, 3);
	CHECK(c.moves(0).size() == 3 && c.moves(3).size() == 2);
	// shuffling: the same seed, the same order; every game stays
	GameList a = u, b = u;
	shuffle_list(a, 7);
	shuffle_list(b, 7);
	std::multiset<std::string> x, y;
	for (size_t i = 0; i < a.size(); ++i) {
		CHECK(list_line(a.start(i), a.moves(i)) == list_line(b.start(i), b.moves(i)));
		x.insert(list_line(a.start(i), a.moves(i)));
		y.insert(list_line(u.start(i), u.moves(i)));
	}
	CHECK(x == y);
	// erasing many games keeps the others
	GameList e;
	std::mt19937_64 rng(3);
	std::vector<std::string> want;
	for (int i = 0; i < 6000; ++i) {
		auto g = random_game(rng, 40);
		e.add(Position{}, g);
		if (i % 3) want.push_back(list_line(Position{}, g));
	}
	for (int i = 5999; i >= 0; --i)
		if (i % 3 == 0) e.erase(static_cast<size_t>(i));
	CHECK(e.size() == want.size());
	for (size_t i = 0; i < e.size(); ++i) CHECK(list_line(e.start(i), e.moves(i)) == want[i]);
}

static void test_orientations()
{
	std::mt19937_64 rng(11);
	std::set<int> firsts;
	for (int i = 0; i < 3000; ++i) {
		const auto g = random_game(rng, 30);
		const auto n = normalized_moves(Position{}, g);
		CHECK(n.size() == g.size() && legal_game(Position{}, n));
		CHECK(n[0] == parse_square("f5"));
		// the same game up to a symmetry: the positions are symmetric to each other
		bool same = false;
		const Position pg = play_all(Position{}, g), pn = play_all(Position{}, n);
		for (int s = 0; s < 8; ++s) same |= symmetric(pg, s) == pn;
		CHECK(same);
		// every symmetric form of a game becomes the same
		for (int s : {3, 4, 7}) {
			std::vector<std::uint8_t> h(g.size());
			for (size_t k = 0; k < g.size(); ++k) h[k] = static_cast<std::uint8_t>(symmetric_square(g[k], s));
			CHECK(normalized_moves(Position{}, h) == n);
		}
		const auto sc = scattered_moves(Position{}, g, rng, 3);
		CHECK(sc.size() == g.size() && legal_game(Position{}, sc));
		firsts.insert(sc[0]);
		// the white side only: the first move (black) stays
		const auto sw = scattered_moves(Position{}, n, rng, 2);
		CHECK(sw[0] == n[0] && legal_game(Position{}, sw));
	}
	CHECK(firsts.size() == 4);
	// the whole list: every game begins with f5
	GameList l;
	read_list_text("f5f6e6f4e3\nd3c3c4e3f4\n", l);
	normalize_list(l);
	CHECK(l.moves(0)[0] == parse_square("f5") && l.moves(1)[0] == parse_square("f5"));
}

static void test_search()
{
	GameList l;
	read_list_text("f5d6c3d3c4\nd3c5f6f5e6\nc4e3f6e6f5\nf5f6e6\n", l);
	// f5 d6: also as d3 c5 (a symmetric form) and c4 e3
	Position p;
	p = p.play(parse_square("f5")).play(parse_square("d6"));
	auto exact = search_list(l, p, false);
	CHECK(exact.size() == 1 && exact[0].game == 0 && exact[0].ply == 2 && exact[0].sym == 0);
	auto sym = search_list(l, p, true);
	CHECK(sym.size() == 3);
	for (const auto &h : sym) {
		const Position q = play_all(l.start(h.game), l.moves(h.game), h.ply);
		CHECK(q == symmetric(p, h.sym));
	}
	// the start itself
	CHECK(search_list(l, Position{}, false).size() == 4);
}

// lines found without the book, to positions of the test games (up to ply 20)
static void test_find_line(int argc, char **argv)
{
	int tried = 0, found = 0;
	long long nodes = 0, worst = 0;
	double worst_ms = 0;
	const auto t0 = std::chrono::steady_clock::now();
	for (int a = 1; a < argc; ++a) {
		std::ifstream in(argv[a]);
		std::string line;
		while (std::getline(in, line)) {
			const ParsedMoves pm = parse_moves(line);
			for (int ply : {8, 14, 20}) {
				if (static_cast<int>(pm.moves.size()) < ply) continue;
				Game g;
				replay(g, Position{}, std::vector<int>(pm.moves.begin(), pm.moves.begin() + ply));
				const Position target = g.now();
				const auto t = std::chrono::steady_clock::now();
				const LineSearch r = find_line(Position{}, target, 20'000'000);
				const double ms = ms_since(t);
				++tried;
				nodes += r.nodes;
				worst = std::max(worst, r.nodes);
				worst_ms = std::max(worst_ms, ms);
				CHECK(r.line.has_value());
				if (!r.line) continue;
				++found;
				Position q;
				bool ok = true;
				for (const int m : *r.line) {
					if (!q.can_play(m)) ok = false;
					if (!ok) break;
					q = q.play(m);
				}
				CHECK(ok && q == target);
			}
		}
	}
	// a position that cannot come from the start
	Position bad;
	bad.black = 0x0000000810000001ULL;
	bad.white = 0x0000001008000000ULL;
	CHECK(!find_line(Position{}, bad, 1'000'000).line);
	std::printf("find_line: %d positions (ply 8, 14, 20), %d found, %lld nodes (largest %lld, %.1f ms), %.0f ms\n", tried,
	            found, nodes, worst, worst_ms, ms_since(t0));
}

// timings on a large list of random games
static void timings()
{
	const int n = 100000;
	std::mt19937_64 rng(5);
	GameList l;
	for (int i = 0; i < n; ++i) l.add(Position{}, random_game(rng, 60));
	std::printf("list of %d games: %.1f MB\n", n, l.bytes() / 1048576.0);
	const std::wstring path = (std::filesystem::temp_directory_path() / "ng_list_big.txt").wstring();
	auto t = std::chrono::steady_clock::now();
	write_list(path, l);
	std::printf("  write %.0f ms (%.1f MB)\n", ms_since(t), std::filesystem::file_size(path) / 1048576.0);
	GameList r;
	t = std::chrono::steady_clock::now();
	read_list(path, r);
	std::printf("  read %.0f ms\n", ms_since(t));
	CHECK(r.size() == l.size());
	std::filesystem::remove(path);
	t = std::chrono::steady_clock::now();
	sort_list(r);
	std::printf("  sort %.0f ms\n", ms_since(t));
	t = std::chrono::steady_clock::now();
	unique_list(r);
	std::printf("  unique %.0f ms\n", ms_since(t));
	t = std::chrono::steady_clock::now();
	normalize_list(r);
	std::printf("  normalize %.0f ms\n", ms_since(t));
	t = std::chrono::steady_clock::now();
	shuffle_list(r, 1);
	std::printf("  shuffle %.0f ms\n", ms_since(t));
	Position p;
	p = p.play(parse_square("f5")).play(parse_square("d6"));
	t = std::chrono::steady_clock::now();
	const auto hits = search_list(r, p, true);
	std::printf("  search (8 symmetries, ply 2) %.0f ms, %zu games\n", ms_since(t), hits.size());
	const Position deep = play_all(r.start(0), r.moves(0), 40);
	t = std::chrono::steady_clock::now();
	const auto hits2 = search_list(r, deep, true);
	std::printf("  search (ply 40) %.0f ms, %zu games\n", ms_since(t), hits2.size());
	CHECK(!hits2.empty());
}

int main(int argc, char **argv)
{
	// list_test --write <file> <games>: a file of random games, for measuring the GUI with a large list
	if (argc == 4 && std::string(argv[1]) == "--write") {
		std::mt19937_64 rng(9);
		GameList l;
		const int n = std::atoi(argv[3]);
		for (int i = 0; i < n; ++i) l.add(Position{}, random_game(rng, 60));
		return write_list(std::filesystem::path(argv[2]).wstring(), l) ? 0 : 1;
	}
	test_symmetries();
	test_bitboards();
	test_list_basics();
	test_orientations();
	test_search();
	test_find_line(argc, argv);
	timings();
	if (failures) {
		std::printf("%d failures\n", failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
