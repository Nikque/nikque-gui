#include "analysis.h"

#include <bit>
#include <charconv>

namespace ng {

MoveEval *PosInfo::find(int sq)
{
	for (auto &m : moves)
		if (m.sq == sq) return &m;
	return nullptr;
}

const MoveEval *PosInfo::find(int sq) const
{
	for (const auto &m : moves)
		if (m.sq == sq) return &m;
	return nullptr;
}

void PosInfo::reset_book()
{
	for (auto &m : moves) {
		const MoveEval keep = m;
		m = MoveEval{};
		m.sq = keep.sq;
		m.searched = keep.searched;
		m.score = keep.score;
		m.depth = keep.depth;
		m.selectivity = keep.selectivity;
		m.bound = keep.bound;
		m.pv = keep.pv;
	}
	book_params.clear();
	book_begun = book_done = in_book = count_done = false;
	levels_done = false;
	levels.clear();
	levels_positions = -1;
}

void PosInfo::reset_search()
{
	for (auto &m : moves) {
		m.searched = false;
		m.score = m.depth = 0;
		m.selectivity = 100;
		m.bound = 0;
		m.pv.clear();
	}
	search_level = 0;
	search_done = false;
}

static PosInfo new_info(const Position &p)
{
	PosInfo info;
	u64 m = p.moves();
	if (m == 0) {
		if (!p.game_over()) info.moves.push_back(MoveEval{.sq = kPass});
	}
	while (m) {
		MoveEval e;
		e.sq = std::countr_zero(m);
		m &= m - 1;
		info.moves.push_back(std::move(e));
	}
	return info;
}

PosInfo &AnalysisCache::get(const Position &p)
{
	const PosKey k = key_of(p);
	auto it = map_.find(k);
	if (it != map_.end()) {
		order_.splice(order_.begin(), order_, it->second.age);
		return it->second.info;
	}
	while (map_.size() >= limit_ && !order_.empty()) {
		map_.erase(order_.back());
		order_.pop_back();
	}
	order_.push_front(k);
	auto &e = map_[k];
	e.info = new_info(p);
	e.age = order_.begin();
	return e.info;
}

PosInfo *AnalysisCache::peek(const Position &p)
{
	auto it = map_.find(key_of(p));
	return it == map_.end() ? nullptr : &it->second.info;
}

const PosInfo *AnalysisCache::peek(const Position &p) const
{
	auto it = map_.find(key_of(p));
	return it == map_.end() ? nullptr : &it->second.info;
}

void AnalysisCache::clear_book()
{
	for (auto &[k, e] : map_) e.info.reset_book();
}

void AnalysisCache::clear_search()
{
	for (auto &[k, e] : map_) e.info.reset_search();
}

Fields::Fields(std::string_view line)
{
	size_t i = 0;
	if (!line.empty() && line[0] == '@') i = 1;
	bool first = true;
	while (i < line.size()) {
		while (i < line.size() && line[i] == ' ') ++i;
		size_t e = line.find(' ', i);
		if (e == std::string_view::npos) e = line.size();
		const std::string_view w = line.substr(i, e - i);
		i = e;
		if (w.empty()) continue;
		if (first) {
			kind_ = w;
			first = false;
			continue;
		}
		const size_t eq = w.find('=');
		if (eq == std::string_view::npos) kv_.emplace_back(w, std::string_view{});
		else kv_.emplace_back(w.substr(0, eq), w.substr(eq + 1));
	}
}

std::optional<std::string_view> Fields::get(std::string_view key) const
{
	for (const auto &[k, v] : kv_)
		if (k == key) return v;
	return std::nullopt;
}

std::optional<long long> Fields::num(std::string_view key) const
{
	const auto v = get(key);
	if (!v || v->empty()) return std::nullopt;
	long long x = 0;
	const char *b = v->data(), *e = v->data() + v->size();
	if (*b == '+') ++b;
	const auto r = std::from_chars(b, e, x);
	if (r.ec != std::errc() || r.ptr != e) return std::nullopt;
	return x;
}

std::optional<HintLine> parse_hint_line(std::string_view line)
{
	std::vector<std::string_view> w;
	for (size_t i = 0; i < line.size();) {
		while (i < line.size() && line[i] == ' ') ++i;
		size_t e = line.find(' ', i);
		if (e == std::string_view::npos) e = line.size();
		if (e > i) w.push_back(line.substr(i, e - i));
		i = e;
	}
	if (w.size() < 3) return std::nullopt;
	HintLine h;
	size_t k = 0;
	auto read_int = [](std::string_view s, int *out) {
		const char *b = s.data(), *e = s.data() + s.size();
		if (b < e && *b == '+') ++b;
		const auto r = std::from_chars(b, e, *out);
		return r.ec == std::errc() && r.ptr == e;
	};
	if (w[0] == "book") {
		h.book = true;
		k = 1;
	} else {
		// depth, "@selectivity%" optional
		const std::string_view d = w[0];
		const size_t at = d.find('@');
		if (!read_int(d.substr(0, at), &h.depth)) return std::nullopt;
		if (at != std::string_view::npos) {
			std::string_view s = d.substr(at + 1);
			if (!s.empty() && s.back() == '%') s.remove_suffix(1);
			if (!read_int(s, &h.selectivity)) return std::nullopt;
		}
		k = 1;
	}
	std::string_view sc = w[k++];
	if (!sc.empty() && (sc[0] == '<' || sc[0] == '>')) {
		h.bound = sc[0];
		sc.remove_prefix(1);
	}
	if (!read_int(sc, &h.score)) return std::nullopt;
	// the rest: time, nodes, speed, then the moves
	for (; k < w.size(); ++k) {
		const int sq = parse_square(w[k]);
		if (sq != kNoMove) h.pv.push_back(sq);
		else if (!h.pv.empty()) break;
	}
	if (h.pv.empty()) return std::nullopt;
	return h;
}

void apply_move_line(PosInfo &info, const Fields &f)
{
	const auto sqs = f.get("sq");
	if (!sqs) return;
	MoveEval *m = info.find(parse_square(*sqs));
	if (!m) return;
	const auto part = f.get("part");
	if (!part) {
		// an answer without the stage part (parts=count) has these rows without stage, missing and link:
		// they keep what an answer with the stage part said
		m->in_book = f.num("book").value_or(0) == 1;
		if (auto st = f.num("stage")) {
			m->book_row = true;
			m->stage = static_cast<int>(*st);
			m->missing = f.num("missing").value_or(-1);
			const auto l = f.num("link");
			m->has_link = l.has_value();
			m->link = static_cast<int>(l.value_or(0));
		}
		if (m->in_book) {
			m->book_value = static_cast<int>(f.num("value").value_or(0));
			m->book_level = static_cast<int>(f.num("level").value_or(0));
			m->lines = f.num("lines").value_or(-1);
		} else if (f.num("leaf").value_or(0) == 1) {
			m->leaf = true;
			m->leaf_value = static_cast<int>(f.num("value").value_or(0));
			m->leaf_level = static_cast<int>(f.num("level").value_or(0));
		}
	} else if (*part == "umi") {
		m->umi_known = true;
		const std::string_view u = f.get("umi").value_or("");
		const size_t c = u.find(':');
		if (u == "none" || c == std::string_view::npos) {
			m->umi_none = true;
		} else {
			std::from_chars(u.data(), u.data() + c, m->umi_black);
			std::from_chars(u.data() + c + 1, u.data() + u.size(), m->umi_white);
		}
		m->umi_exact = f.num("exact").value_or(1) != 0;
	} else if (*part == "count") {
		m->positions = f.num("positions").value_or(-1);
	}
}

bool shown_value(const MoveEval &m, bool use_book, int *value)
{
	if (use_book && m.in_book) {
		*value = m.book_value;
		return true;
	}
	if (m.searched) {
		*value = m.score;
		return true;
	}
	if (use_book && m.leaf) {
		*value = m.leaf_value;
		return true;
	}
	return false;
}

int shown_level(const MoveEval &m, bool use_book)
{
	if (m.searched) return m.depth;
	if (use_book && m.leaf) return m.leaf_level;
	return 0;
}

}  // namespace ng

namespace ng {

int echo_marker(std::string_view line)
{
	const size_t e = line.find("echo");
	if (e == std::string_view::npos) return -1;
	const size_t m = line.find("\"M", e);
	if (m == std::string_view::npos) return -1;
	int n = 0;
	size_t i = m + 2;
	if (i >= line.size() || line[i] < '0' || line[i] > '9') return -1;
	for (; i < line.size() && line[i] >= '0' && line[i] <= '9'; ++i) n = n * 10 + (line[i] - '0');
	if (i >= line.size() || line[i] != '"') return -1;
	return n;
}

}  // namespace ng

#include "game.h"

namespace ng {

bool move_exact(const MoveEval &m, const Position &p)
{
	return m.searched && m.selectivity >= 100 && m.depth >= p.empties() - 1;
}

std::vector<LineEval> backward_line(const Game &g, const AnalysisCache &cache, bool use_book, bool backward)
{
	const int n = g.size();
	std::vector<LineEval> line(n + 1);
	for (int i = n; i >= 0; --i) {
		const Position &p = g.at(i);
		LineEval &e = line[i];
		const LineEval *next = i < n ? &line[i + 1] : nullptr;
		const bool turn = i < n && g.at(i + 1).side != p.side;  // the side changes to the next position
		auto from_next = [&](int v) { return turn ? -v : v; };
		if (p.game_over()) {
			const int b = std::popcount(p.black), w = std::popcount(p.white), em = 64 - b - w;
			const int diff = b > w ? b - w + em : b < w ? b - w - em : 0;
			e.known = e.complete = e.exact = true;
			e.best = p.side == kBlack ? diff : -diff;
			continue;
		}
		if (p.must_pass()) {
			if (next && next->known) {
				e.known = e.played_known = true;
				e.complete = next->complete;
				e.best = e.played = from_next(next->best);
				e.exact = e.played_exact = next->exact;
			}
			continue;
		}
		const PosInfo *info = cache.peek(p);
		if (!info) continue;
		int best = -999;
		bool all = !info->moves.empty(), best_exact = false;
		for (const MoveEval &m : info->moves) {
			int v = 0;
			bool known = shown_value(m, use_book, &v);
			bool ex = known && !(use_book && m.in_book) && move_exact(m, p);
			bool improved = false;
			const bool played = i < n && m.sq == g.moves()[i];
			if (played && backward && next && next->complete && !(use_book && m.in_book) && !ex) {
				v = from_next(next->best);
				ex = next->exact;
				known = improved = true;
			}
			if (played) {
				e.played_known = known;
				e.played = v;
				e.played_exact = ex;
				e.played_improved = improved;
			}
			if (!known) {
				all = false;
				continue;
			}
			if (v > best) {
				best = v;
				best_exact = ex;
			}
		}
		if (best != -999) {
			e.known = true;
			e.best = best;
			e.exact = best_exact;
		}
		e.complete = all;
	}
	return line;
}

std::optional<MissingLine> parse_missing_line(const Fields &f)
{
	if (f.kind() != "line") return std::nullopt;
	MissingLine ml;
	const std::string_view mv = f.get("moves").value_or("");
	if (mv.size() % 2) return std::nullopt;
	for (size_t k = 0; k < mv.size(); k += 2) {
		const int sq = parse_square(mv.substr(k, 2));
		if (sq == kNoMove) return std::nullopt;
		ml.moves.push_back(sq);
	}
	ml.leaf = parse_square(f.get("leaf").value_or(""));
	ml.value = static_cast<int>(f.num("value").value_or(0));
	ml.empties = static_cast<int>(f.num("empties").value_or(0));
	if (ml.moves.empty()) return std::nullopt;
	return ml;
}

}  // namespace ng
