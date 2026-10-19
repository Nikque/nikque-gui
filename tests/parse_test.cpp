// Reading the engines' lines: hint lines and the "@" lines of book moveinfo (lines taken from the engines).
#include <cstdio>

#include "analysis.h"

using namespace ng;

static int failures = 0;
#define CHECK(c)                                                              \
	do {                                                                      \
		if (!(c)) {                                                           \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);          \
			++failures;                                                       \
		}                                                                     \
	} while (0)

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
	if (failures) {
		std::printf("%d failures\n", failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
