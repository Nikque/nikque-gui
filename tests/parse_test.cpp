// Reading the engines' lines: hint lines and the "@" lines of book moveinfo (lines taken from the engines).
#include <cstdio>

#include "analysis.h"
#include "game.h"

using namespace ng;

static int failures = 0;
#define CHECK(c)                                                              \
	do {                                                                      \
		if (!(c)) {                                                           \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);          \
			++failures;                                                       \
		}                                                                     \
	} while (0)

// every move of a position searched, with the value given by `value(sq)`
template <class F>
static void searched(AnalysisCache &c, const Position &p, F value, int depth = 10)
{
	PosInfo &info = c.get(p);
	for (auto &m : info.moves) {
		m.searched = true;
		m.score = value(m.sq);
		m.depth = depth;
	}
	info.search_done = true;
}

static void test_backward()
{
	Game g;
	for (const char *mv : {"f5", "d6"}) g.play(parse_square(mv));
	AnalysisCache c;
	const int f5 = parse_square("f5"), d6 = parse_square("d6");
	searched(c, g.at(0), [&](int sq) { return sq == f5 ? 1 : -2; });
	searched(c, g.at(1), [&](int sq) { return sq == d6 ? -3 : -6; });  // white to move: best -3
	auto line = backward_line(g, c, true, true);
	CHECK(line.size() == 3 && line[1].known && line[1].complete && line[1].best == -3);
	// f5 takes 3 (the best of the next position, from the other side), marked
	CHECK(line[0].played_known && line[0].played == 3 && line[0].played_improved && line[0].best == 3);
	// off: the search value stays
	line = backward_line(g, c, true, false);
	CHECK(line[0].played == 1 && !line[0].played_improved && line[0].best == 1);
	// the next position not complete: no change
	c.get(g.at(1)).moves[0].searched = false;
	line = backward_line(g, c, true, true);
	CHECK(!line[1].complete && line[0].played == 1 && !line[0].played_improved);
	c.get(g.at(1)).moves[0].searched = true;
	// a book value is kept
	MoveEval *m = c.get(g.at(0)).find(f5);
	m->in_book = m->book_row = true;
	m->book_value = 0;
	line = backward_line(g, c, true, true);
	CHECK(line[0].played == 0 && !line[0].played_improved);
	m->in_book = m->book_row = false;
	// a search to the end is kept
	m->depth = 59;
	m->selectivity = 100;
	line = backward_line(g, c, true, true);
	CHECK(line[0].played == 1 && line[0].played_exact && !line[0].played_improved);

	// a pass between: the value goes back with the sign of the side to move
	const char *moves = "f5 f4 d3 f6 g6 c4 f7 d2 f3 g7 d6 f8 g8 f2 g3 h7 c3 b4 d1 e2 c2 h3 e8 c7 h6 e1 g4 c5 b5 b3 e6 c1 a3 "
	                    "g5 a4 h8 b6 e7 g2 g1 b1 f1 e3 b2 h2 a7 h5 a2 d7 a6 h4 a5 b7 a1 pa b8";
	Game h;
	replay(h, Position{}, parse_moves(moves).moves);
	int pass_at = -1;
	for (int i = 0; i < h.size(); ++i)
		if (h.moves()[i] == kPass) pass_at = i;
	CHECK(pass_at > 0 && h.at(pass_at).must_pass());
	AnalysisCache d;
	searched(d, h.at(pass_at + 1), [](int) { return 7; }, 1);  // shallow: not a search to the end
	line = backward_line(h, d, true, true);
	CHECK(line[pass_at].known && line[pass_at].best == -7);  // the side to move changes with the pass
	searched(d, h.at(pass_at - 1), [](int) { return 0; }, 1);
	line = backward_line(h, d, true, true);
	// two side changes (the move, then the pass): the same side as after the pass
	CHECK(line[pass_at - 1].played_improved && line[pass_at - 1].played == 7);
}

// the book answers cleared (another range, the book changed): the values of the search and of the other engine stay
static void test_reset_book()
{
	AnalysisCache c;
	PosInfo &info = c.get(Position{});
	for (auto &m : info.moves) {
		m.searched = true;
		m.score = 3;
		m.depth = 12;
		m.pv = {m.sq};
		m.cmp_searched = true;
		m.cmp_score = -5;
		m.cmp_depth = 14;
		m.in_book = m.book_row = true;
		m.stage = 4;
	}
	info.search_done = info.cmp_done = info.book_done = true;
	info.search_level = info.cmp_level = 21;
	info.reset_book();
	CHECK(!info.book_done && info.search_done && info.search_level == 21 && info.cmp_done && info.cmp_level == 21);
	for (const auto &m : info.moves) {
		CHECK(!m.in_book && !m.book_row && m.stage == -1);
		CHECK(m.searched && m.score == 3 && m.depth == 12 && m.pv.size() == 1 && m.pv[0] == m.sq);
		CHECK(m.cmp_searched && m.cmp_score == -5 && m.cmp_depth == 14);
	}
}

// the numbers of the requests: 1 .. 99,999 (the engines take id= up to 100,000), again from 1, the numbers in use skipped
static void test_request_ids()
{
	auto none = [](int) { return false; };
	static_assert(kMaxRequestId < 100000);
	CHECK(next_request_id(0, none) == 1);
	CHECK(next_request_id(41, none) == 42);
	CHECK(next_request_id(kMaxRequestId - 1, none) == kMaxRequestId);
	CHECK(next_request_id(kMaxRequestId, none) == 1);
	CHECK(next_request_id(kMaxRequestId, [](int id) { return id <= 3; }) == 4);
	CHECK(next_request_id(7, [](int id) { return id == 8 || id == 9; }) == 10);
	// a long run of requests never goes beyond the limit
	int id = 0;
	bool ok = true;
	for (int k = 0; k < 250000; ++k) {
		id = next_request_id(id, none);
		ok = ok && id >= 1 && id <= kMaxRequestId;
	}
	CHECK(ok);
}

// a blog board with a number longer than an int: no board, and nothing thrown
static void test_param_numbers()
{
	auto read = [](const std::string &text, bool *threw) {
		*threw = false;
		try {
			return parse_param_lines(text);
		} catch (...) {
			*threw = true;
			return std::optional<std::vector<int>>{};
		}
	};
	std::string text;
	for (int row = 1; row <= 8; ++row)
		text += "<PARAM name=\"Line" + std::to_string(row) + "\" value=\"" + (row == 3 ? "99999999999999999999" : "0,0,0,0,0,0,0,0") + "\">\n";
	bool threw = false;
	auto r = read(text, &threw);
	CHECK(!threw && !r);
	r = read("<PARAM name=\"Line1\" value=\"1,2,3,4,5,6,7,12345678901\">", &threw);
	CHECK(!threw && !r);
}

static void test_missing()
{
	auto a = parse_missing_line(Fields("@line id=2 moves=d3e3 leaf=f4 value=-3 empties=58"));
	CHECK(a && a->moves.size() == 2 && a->moves[0] == parse_square("d3") && a->leaf == parse_square("f4"));
	CHECK(a && a->value == -3 && a->empties == 58);
	auto b = parse_missing_line(Fields("@line id=2 moves=a1pab2 leaf=c3 value=4 empties=3 loss=2"));
	CHECK(b && b->moves.size() == 3 && b->moves[1] == kPass);
	CHECK(!parse_missing_line(Fields("@line id=2 moves=a1x leaf=c3")));
	CHECK(!parse_missing_line(Fields("@end id=2 count=1 shown=1 stage=1 ms=0")));
}

int main()
{
	{
		auto h = parse_hint_line("16@73%  +00        0:00.046        429076    9327739 d3 E3 f4 C3 c4 D6 e6 C5");
		CHECK(h && !h->book && h->depth == 16 && h->selectivity == 73 && h->score == 0 && h->bound == 0);
		CHECK(h && h->pv.size() == 8 && h->pv[0] == parse_square("d3") && h->pv[1] == parse_square("e3"));
		auto e = parse_hint_line("24  -12        0:01.000       12345678            a1 PS b2");
		CHECK(e && e->depth == 24 && e->selectivity == 100 && e->score == -12);
		CHECK(e && e->pv.size() == 3 && e->pv[1] == kPass);
		auto b = parse_hint_line("book    -3                                          d3 E3 f4               ");
		CHECK(b && b->book && b->score == -3 && b->pv.size() == 3);
		auto bound = parse_hint_line("21@98%  <+04       0:00.500    100   2000 c4 c3");
		CHECK(bound && bound->bound == '<' && bound->score == 4);
		CHECK(!parse_hint_line("New book 10 20......done"));
		CHECK(!parse_hint_line("Edax version 4.5.5 Oct 19 2026 20:05:32 for Windows"));
		CHECK(!parse_hint_line("@begin id=1 ply=0 turn=black moves=4 book=1"));
		// the answers of "echo M<n>" (both engines), in order with the lines of hint
		CHECK(echo_marker("WARNING: echo: \"M101\" is not on or off; ignored") == 101);
		CHECK(echo_marker("WARNING: echo: \"on\" ignored") == -1);
		CHECK(echo_marker("    8   -03        0:00.000           903            d3 E3 f3") == -1);
		CHECK(!parse_hint_line("WARNING: echo: \"M101\" is not on or off; ignored"));
	}
	{
		Position p;
		AnalysisCache cache(2);
		PosInfo &info = cache.get(p);
		CHECK(info.moves.size() == 4);
		const Fields b("@begin id=12 ply=0 turn=black moves=4 book=1");
		CHECK(b.kind() == "begin" && b.num("id") == 12 && b.num("book") == 1 && b.get("turn") == "black");
		apply_move_line(info, Fields("@move id=1 sq=d3 book=1 value=-3 level=4 lines=1286118 stage=5 missing=3 link=-1"));
		apply_move_line(info, Fields("@move id=1 sq=c4 book=0 leaf=1 value=9 level=24 stage=0"));
		apply_move_line(info, Fields("@move id=1 sq=d3 part=umi umi=10:7 exact=0"));
		apply_move_line(info, Fields("@move id=1 sq=f5 part=umi umi=none"));
		apply_move_line(info, Fields("@move id=1 sq=d3 part=count positions=409493"));
		// the rows of an answer without the stage part (parts=count) keep the stage known before
		apply_move_line(info, Fields("@move id=2 sq=d3 book=1 value=-3 level=4 lines=1286118"));
		const MoveEval *d3 = info.find(parse_square("d3"));
		CHECK(d3 && d3->in_book && d3->book_value == -3 && d3->book_level == 4 && d3->lines == 1286118);
		CHECK(d3 && d3->stage == 5 && d3->missing == 3 && d3->has_link && d3->link == -1);
		CHECK(d3 && d3->umi_known && d3->umi_black == 10 && d3->umi_white == 7 && !d3->umi_exact);
		CHECK(d3 && d3->positions == 409493);
		const MoveEval *c4 = info.find(parse_square("c4"));
		CHECK(c4 && !c4->in_book && c4->leaf && c4->leaf_value == 9 && c4->leaf_level == 24 && c4->stage == 0);
		const MoveEval *f5 = info.find(parse_square("f5"));
		CHECK(f5 && f5->umi_none);
		int v = 0;
		CHECK(shown_value(*d3, true, &v) && v == -3);
		CHECK(!shown_value(*d3, false, &v));
		CHECK(shown_value(*c4, true, &v) && v == 9 && shown_level(*c4, true) == 24);

		// the oldest position goes beyond the limit
		cache.get(p.play(parse_square("f5")));
		cache.get(p.play(parse_square("d3")));
		CHECK(cache.size() == 2 && cache.peek(p) == nullptr);
	}
	test_backward();
	test_missing();
	test_reset_book();
	test_request_ids();
	test_param_numbers();
	if (failures) {
		std::printf("%d failures\n", failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
