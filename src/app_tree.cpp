// The tree of the book lines from a position: the nodes open, the positions asked of the main engine, the clicks
// and the script commands.
#include "app.h"

#include <algorithm>

#include "lang.h"

namespace ng {

namespace {

// a position where the side must pass goes on to the other side, as the book stores it
Position after_move(const Position &p, int sq)
{
	Position q = p.play(sq);
	if (q.must_pass()) q = q.play(kPass);
	return q;
}

}  // namespace

// The root stays while the position shown is the root or comes after it in the same game; else the position shown
// becomes the root. The nodes on the way from the root to the position shown are opened, so that its row is seen.
void App::tree_sync()
{
	const int cur = game_.cursor();
	const auto &base = tree_base_.moves();
	const auto &moves = game_.moves();
	const bool inside = tree_set_ && tree_base_.start() == game_.start() && static_cast<int>(base.size()) <= cur &&
	                    std::equal(base.begin(), base.end(), moves.begin());
	if (!inside) {
		tree_base_ = game_;
		tree_base_.go_to(cur);
		tree_base_.truncate();
		tree_open_.clear();
		tree_open_.insert(std::vector<int>{});  // the root open ("insert({})" is the insert of an empty list: nothing)
		tree_scroll_ = 0;
		tree_set_ = true;
	}
	std::vector<int> path;
	for (int i = static_cast<int>(tree_base_.moves().size()); i < cur; ++i) {
		if (moves[i] == kPass) continue;
		tree_open_.insert(path);
		path.push_back(moves[i]);
	}
}

Position App::tree_position(const std::vector<int> &path) const
{
	Position p = tree_base_.now();
	for (const int m : path) {
		if (!p.can_play(m)) break;
		p = after_move(p, m);
	}
	return p;
}

bool App::tree_visible(const std::vector<int> &path) const
{
	std::vector<int> pre;
	for (const int m : path) {
		if (!tree_open_.count(pre)) return false;
		pre.push_back(m);
	}
	return true;
}

// the next position of the open nodes still without its book analysis (the root first, then by the paths)
std::optional<Position> App::tree_next_wanted()
{
	const std::string params = book_params();
	for (const auto &path : tree_open_) {
		if (!tree_visible(path)) continue;
		const Position p = tree_position(path);
		if (p.game_over()) continue;
		PosInfo &info = cache_.get(p);
		if (info.book_params != params) {
			if (info.book_begun || info.book_done || info.count_done || info.levels_done) info.reset_book();
			info.book_params = params;
		}
		if (!info.book_done) return p;
	}
	return std::nullopt;
}

bool App::tree_waiting() const
{
	if (!use_book_ || !eng_[0].alive || learning_) return false;
	for (const auto &path : tree_open_) {
		if (!tree_visible(path)) continue;
		const Position p = tree_position(path);
		if (p.game_over()) continue;
		const PosInfo *info = cache_.peek(p);
		if (!info || !info->book_done) return true;
	}
	return false;
}

void App::tree_click(const TreeRow &row, bool on_arrow)
{
	if (on_arrow && row.can_open) {
		if (tree_open_.count(row.path)) tree_open_.erase(row.path);
		else tree_open_.insert(row.path);
		pump_graph();
		invalidate();
		return;
	}
	if (row.can_open) tree_open_.insert(row.path);
	tree_go(row.path);
}

// the position of a row on the board: the moves from the root, keeping the moves after them in the game when
// they are the same (not during a game against the computer)
void App::tree_go(const std::vector<int> &path)
{
	if (play_mode_ != PlayMode::Free || editing_) return;
	game_.go_to(static_cast<int>(tree_base_.moves().size()));
	for (const int m : path)
		if (!game_.play(m)) break;
	selected_sq_ = kNoMove;
	position_changed();
}

// tree root | tree open <moves> | tree close <moves> | tree toggle <moves> | tree go <moves> (the moves from the root)
bool App::tree_command(const std::vector<std::string> &w, const std::string &rest)
{
	if (w.size() < 2) return false;
	const std::string what = w[1];
	if (what == "root") {
		tree_set_ = false;
		tree_sync();
	} else {
		const std::string moves = rest.size() > what.size() ? rest.substr(what.size()) : std::string();
		const std::vector<int> path = parse_moves(moves).moves;
		if (what == "open") {
			std::vector<int> pre;
			for (const int m : path) {
				tree_open_.insert(pre);
				pre.push_back(m);
			}
			tree_open_.insert(pre);
		} else if (what == "close") {
			tree_open_.erase(path);
		} else if (what == "toggle") {
			// as the arrow of a row: that node alone opened or closed
			TreeRow row;
			row.path = path;
			row.can_open = true;
			tree_click(row, true);
		} else if (what == "go") {
			tree_go(path);
		} else {
			return false;
		}
	}
	pump_graph();
	invalidate();
	return true;
}

}  // namespace ng
