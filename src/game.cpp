#include "game.h"

#include <algorithm>
#include <bit>
#include <cstdio>

namespace ng {

void Game::reset(const Position &start)
{
	moves_.clear();
	pos_.assign(1, start);
	cur_ = 0;
	settle_forward();
}

void Game::settle_forward()
{
	// a position where the side must pass is never the place shown: the pass is played at once
	while (pos_[cur_].must_pass()) {
		if (cur_ == size()) {
			moves_.push_back(kPass);
			pos_.push_back(pos_[cur_].play(kPass));
		}
		++cur_;
	}
}

bool Game::play(int sq)
{
	const Position &p = now();
	if (sq == kPass || !p.can_play(sq)) return false;
	if (cur_ < size() && moves_[cur_] == sq) {
		++cur_;
	} else {
		moves_.resize(cur_);
		pos_.resize(cur_ + 1);
		moves_.push_back(sq);
		pos_.push_back(p.play(sq));
		++cur_;
	}
	settle_forward();
	return true;
}

bool Game::back()
{
	const int old = cur_;
	if (cur_ == 0) return false;
	--cur_;
	while (cur_ > 0 && pos_[cur_].must_pass()) --cur_;
	if (pos_[cur_].must_pass()) {  // a start position where the side must pass
		cur_ = old;
		return false;
	}
	return true;
}

bool Game::forward()
{
	if (cur_ >= size()) return false;
	++cur_;
	settle_forward();
	return true;
}

void Game::go_to(int n)
{
	if (n < 0) n = 0;
	if (n > size()) n = size();
	cur_ = n;
	settle_forward();
}

std::string Game::to_text(bool upto_cursor) const
{
	std::string s;
	const int n = upto_cursor ? cur_ : size();
	s.reserve(n * 2);
	for (int i = 0; i < n; ++i)
		if (moves_[i] != kPass) s += square_name(moves_[i]);
	return s;
}

namespace {

// next code point of UTF-8 text (invalid bytes come back as themselves)
char32_t next_char(std::string_view s, size_t &i)
{
	const unsigned char c = static_cast<unsigned char>(s[i]);
	int n = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
	if (n <= 0 || i + n >= s.size()) {
		++i;
		return c;
	}
	char32_t u = c & (0x3f >> n);
	for (int k = 1; k <= n; ++k) {
		const unsigned char d = static_cast<unsigned char>(s[i + k]);
		if ((d & 0xc0) != 0x80) {
			++i;
			return c;
		}
		u = (u << 6) | (d & 0x3f);
	}
	i += n + 1;
	return u;
}

// full-width letters and digits become ASCII; other characters are kept
char32_t narrow(char32_t u)
{
	if (u >= 0xff21 && u <= 0xff3a) return u - 0xff21 + 'a';
	if (u >= 0xff41 && u <= 0xff5a) return u - 0xff41 + 'a';
	if (u >= 0xff10 && u <= 0xff19) return u - 0xff10 + '0';
	if (u >= 'A' && u <= 'Z') return u - 'A' + 'a';
	return u;
}

bool is_letter(char32_t u) { return u >= 'a' && u <= 'z'; }
bool is_digit(char32_t u) { return u >= '0' && u <= '9'; }

}  // namespace

ParsedMoves parse_moves(std::string_view utf8)
{
	std::vector<char32_t> text;
	text.reserve(utf8.size());
	for (size_t i = 0; i < utf8.size();) text.push_back(narrow(next_char(utf8, i)));

	ParsedMoves out;
	for (size_t i = 0; i + 1 < text.size(); ++i) {
		const char32_t c = text[i], r = text[i + 1];
		if (c < 'a' || c > 'h' || r < '1' || r > '8') continue;
		if (i > 0 && is_letter(text[i - 1])) continue;              // inside a word
		if (i + 2 < text.size() && is_digit(text[i + 2])) continue;  // part of a number
		out.moves.push_back((r - '1') * 8 + (c - 'a'));
		++i;
	}
	out.read = static_cast<int>(out.moves.size());
	return out;
}

int replay(Game &game, const Position &start, const std::vector<int> &moves)
{
	game.reset(start);
	int n = 0;
	for (const int sq : moves) {
		if (!game.play(sq)) break;
		++n;
	}
	return n;
}

}  // namespace ng

namespace ng {

std::optional<Position> parse_board_text(std::string_view text)
{
	size_t start = text.find("board:");
	start = start == std::string_view::npos ? 0 : start + 6;
	// the first run of 64 board characters (spaces between them are allowed)
	for (size_t i = start; i < text.size(); ++i) {
		u64 black = 0, white = 0;
		int n = 0;
		size_t k = i;
		for (; k < text.size() && n < 64; ++k) {
			const char c = text[k];
			if (c == ' ' && n > 0) continue;
			if (c == 'X' || c == 'x' || c == '*') black |= 1ULL << n;
			else if (c == 'O' || c == 'o') white |= 1ULL << n;
			else if (c != '-' && c != '.') break;
			++n;
		}
		if (n < 64) continue;
		Position p;
		p.black = black;
		p.white = white;
		p.side = kBlack;
		for (; k < text.size(); ++k) {
			const char c = text[k];
			if (c == ' ' || c == '\t') continue;
			if (c == 'O' || c == 'o' || c == 'W' || c == 'w') p.side = kWhite;
			break;
		}
		if (std::popcount(black) + std::popcount(white) < 4) return std::nullopt;
		return p;
	}
	return std::nullopt;
}

std::optional<std::vector<int>> parse_param_lines(std::string_view text)
{
	int cells[64];
	for (int row = 0; row < 8; ++row) {
		const std::string key = "Line" + std::to_string(row + 1);
		size_t at = text.find(key);
		if (at == std::string_view::npos) return std::nullopt;
		at = text.find("value", at);
		if (at == std::string_view::npos) return std::nullopt;
		const size_t q1 = text.find('"', at);
		const size_t q2 = q1 == std::string_view::npos ? q1 : text.find('"', q1 + 1);
		if (q2 == std::string_view::npos) return std::nullopt;
		const std::string_view v = text.substr(q1 + 1, q2 - q1 - 1);
		// numbers separated by anything, or 8 cells of 2 digits written together
		std::vector<int> nums;
		std::string digits;
		bool separated = false;
		for (char c : v) {
			if (c >= '0' && c <= '9') digits += c;
			else {
				separated = true;
				if (!digits.empty()) nums.push_back(std::stoi(digits));
				if (c != ' ' && c != ',' && c != '\t' && digits.empty()) nums.push_back(0);  // a mark for a first disc
				digits.clear();
			}
		}
		if (!digits.empty()) {
			if (!separated && digits.size() == 16) {
				for (int i = 0; i < 8; ++i) nums.push_back(std::stoi(digits.substr(i * 2, 2)));
			} else {
				nums.push_back(std::stoi(digits));
			}
		}
		if (nums.size() != 8) return std::nullopt;
		for (int col = 0; col < 8; ++col) cells[row * 8 + col] = nums[col];
	}
	std::vector<std::pair<int, int>> order;
	for (int sq = 0; sq < 64; ++sq)
		if (cells[sq] > 0) order.emplace_back(cells[sq], sq);
	std::sort(order.begin(), order.end());
	std::vector<int> moves;
	for (size_t i = 0; i < order.size(); ++i) {
		if (order[i].first != static_cast<int>(i) + 1) return std::nullopt;  // 1, 2, 3, ... without gaps
		moves.push_back(order[i].second);
	}
	if (moves.empty()) return std::nullopt;
	return moves;
}

}  // namespace ng

namespace ng {

Game transformed(const Game &g, int s)
{
	Game out;
	out.reset(symmetric(g.start(), s));
	for (const int m : g.moves())
		if (m != kPass) out.play(symmetric_square(m, s));  // the passes come again at the same places
	out.go_to(g.cursor());
	return out;
}

std::string bitboard_text(const Position &p, BitForm form)
{
	char b[96];
	const char side = p.side == kBlack ? 'X' : 'O';
	switch (form) {
	case BitForm::Hex:
		snprintf(b, sizeof b, "0x%016llX 0x%016llX %c", static_cast<unsigned long long>(p.black),
		         static_cast<unsigned long long>(p.white), side);
		break;
	case BitForm::Ntest:
		snprintf(b, sizeof b, "%llu %llu %c", static_cast<unsigned long long>(~(p.black | p.white)),
		         static_cast<unsigned long long>(p.me()), side);
		break;
	default:
		snprintf(b, sizeof b, "%llu %llu %c", static_cast<unsigned long long>(p.black),
		         static_cast<unsigned long long>(p.white), side);
		break;
	}
	return b;
}

namespace {

std::optional<u64> number_token(std::string_view t)
{
	int base = 10;
	if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
		t.remove_prefix(2);
		base = 16;
	} else if (t.size() == 16 && t.find_first_not_of("0123456789abcdefABCDEF") == std::string_view::npos &&
	           (t[0] == '0' || t.find_first_not_of("0123456789") != std::string_view::npos)) {
		base = 16;  // 16 hex digits (a decimal number does not begin with 0)
	}
	if (t.empty() || t.size() > 20) return std::nullopt;
	u64 v = 0;
	for (const char c : t) {
		int d;
		if (c >= '0' && c <= '9') d = c - '0';
		else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
		else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
		else return std::nullopt;
		if (v > (~0ULL - d) / base) return std::nullopt;  // too large
		v = v * base + d;
	}
	return v;
}

// 1 black, 2 white, 0 not a side
int side_token(std::string_view t)
{
	std::string s(t);
	for (auto &c : s)
		if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
	if (s == "x" || s == "*" || s == "b" || s == "black" || s == "\xE9\xBB\x92" || s == "\xE9\xBB\x91") return 1;
	if (s == "o" || s == "w" || s == "white" || s == "\xE7\x99\xBD") return 2;
	return 0;
}

}  // namespace

std::optional<Position> parse_bitboards(std::string_view text, bool ntest)
{
	std::vector<u64> nums;
	int side = 0;
	size_t i = 0;
	while (i < text.size()) {
		const unsigned char c = static_cast<unsigned char>(text[i]);
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == ';' || c == ':' || c == '=' ||
		    c == '(' || c == ')' || c == '[' || c == ']') {
			++i;
			continue;
		}
		size_t e = i;
		while (e < text.size()) {
			const char d = text[e];
			if (d == ' ' || d == '\t' || d == '\r' || d == '\n' || d == ',' || d == ';' || d == ':' || d == '=' ||
			    d == '(' || d == ')' || d == '[' || d == ']')
				break;
			++e;
		}
		const std::string_view t = text.substr(i, e - i);
		i = e;
		if (nums.size() < 2) {
			if (auto v = number_token(t)) {
				nums.push_back(*v);
				continue;
			}
			if (side_token(t)) continue;  // a label before the numbers ("black: ...")
			return std::nullopt;
		}
		if (const int s = side_token(t)) {
			side = s;
			break;
		}
		return std::nullopt;
	}
	if (nums.size() != 2) return std::nullopt;
	Position p;
	p.side = side == 2 ? kWhite : kBlack;
	if (ntest) {
		const u64 empty = nums[0], mover = nums[1];
		if (mover & empty) return std::nullopt;
		const u64 other = ~(empty | mover);
		p.black = p.side == kBlack ? mover : other;
		p.white = p.side == kBlack ? other : mover;
	} else {
		if (nums[0] & nums[1]) return std::nullopt;
		p.black = nums[0];
		p.white = nums[1];
	}
	if (std::popcount(p.black | p.white) < 4) return std::nullopt;
	return p;
}

std::string obf_text(const Position &p)
{
	return p.to_board_string() + ";";
}

}  // namespace ng
