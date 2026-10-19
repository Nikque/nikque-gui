#include "game.h"

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
