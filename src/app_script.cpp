// Command scripts: a text file of commands (one a line, "#" notes) run one after another, also at the start
// (config.ini [script] startup). A command that starts work (reading a list, learning, a tool, the analysis of
// the position) waits for its end before the next line. The commands are listed in readme.txt.
#include "platform.h"

#include <cerrno>
#include <cstdio>
#include <limits>

#include "app.h"
#include "lang.h"
#include "paths.h"

namespace ng {

namespace {

constexpr int kMaxDepth = 4;  // scripts run from scripts, at most

std::vector<std::string> words(const std::string &s)
{
	std::vector<std::string> out;
	size_t i = 0;
	while (i < s.size()) {
		while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
		size_t e = i;
		while (e < s.size() && s[e] != ' ' && s[e] != '\t') ++e;
		if (e > i) out.push_back(s.substr(i, e - i));
		i = e;
	}
	return out;
}

// the text after the first n words, as it is
std::string text_after(const std::string &line, int n)
{
	size_t i = 0;
	for (int k = 0; k < n; ++k) {
		while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
		while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
	}
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
	return line.substr(i);
}

// the text after the first n words, without the "..." around it
std::string rest_after(const std::string &line, int n)
{
	std::string r = text_after(line, n);
	if (r.size() >= 2 && r.front() == '"' && r.back() == '"') r = r.substr(1, r.size() - 2);
	return r;
}

// "<file> [word]": a file of one word, or in "..." (also with spaces), then at most one word (empty when left out).
// Null, or the text of the error.
const char *file_then_word(const std::string &text, std::string *file, std::string *word)
{
	size_t i;
	if (text.starts_with('"')) {
		i = text.find('"', 1);
		if (i == std::string::npos) return "script.bad_file";
		*file = text.substr(1, i - 1);
		if (++i < text.size() && text[i] != ' ' && text[i] != '\t') return "script.bad_word";
	} else {
		i = std::min(text.find_first_of(" \t"), text.size());
		*file = text.substr(0, i);
	}
	if (file->empty()) return "script.bad_file";
	const std::vector<std::string> w = words(text.substr(i));
	if (w.size() > 1) return "script.bad_word";
	*word = w.empty() ? "" : w[0];
	return nullptr;
}

std::optional<int> number(const std::string &s)
{
	if (s.empty()) return std::nullopt;
	char *end = nullptr;
	errno = 0;
	const long v = std::strtol(s.c_str(), &end, 10);
	if (errno == ERANGE || end == s.c_str() || *end ||
	    v < (std::numeric_limits<int>::min)() || v > (std::numeric_limits<int>::max)()) return std::nullopt;
	return static_cast<int>(v);
}

}  // namespace

void App::run_script_file(const std::wstring &file, int depth)
{
	if (depth >= kMaxDepth) {
		log(lang::f("script.too_deep", {std::to_wstring(kMaxDepth)}));
		script_.clear();
		return;
	}
	std::string all;
	if (!read_file(file, &all)) {
		log(lang::f("script.cannot_open", {file}));
		return;
	}
	if (all.starts_with("\xEF\xBB\xBF")) all.erase(0, 3);
	std::vector<ScriptLine> lines;
	size_t i = 0;
	while (i < all.size()) {
		size_t e = all.find('\n', i);
		if (e == std::string::npos) e = all.size();
		std::string line = trim(std::string_view(all).substr(i, e - i));
		i = e + 1;
		if (line.empty() || line[0] == '#') continue;
		lines.push_back({depth + 1, std::move(line)});
	}
	// before the lines still to run of the script that called it
	script_.insert(script_.begin(), lines.begin(), lines.end());
	if (depth == 0) {
		script_dir_ = file.substr(0, file.find_last_of(L"\\/"));
		log(lang::f("script.started", {file, std::to_wstring(lines.size())}));
	}
	update_menu();
	pump_script();
}

bool App::script_waiting() const
{
	if ((script_wait_ == ScriptWait::Game || script_wait_ == ScriptWait::Analysis) && std::chrono::steady_clock::now() >= script_until_)
		return false;
	return script_condition();
}

bool App::script_condition() const
{
	switch (script_wait_) {
	case ScriptWait::Task: return task_busy_;
	case ScriptWait::Learn: return learning_ || queue_running_;
	case ScriptWait::Tool: return tool_busy_;
	case ScriptWait::FindLine: return findline_id_ != 0 || task_busy_;
	case ScriptWait::BookLines: return list_book_id_ != 0 || task_busy_;
	case ScriptWait::Game: return play_mode_ != PlayMode::Free && !(game_end_done_ && engine_games_left_ == 0);
	case ScriptWait::Com: return com_thinking_ || com_book_move_ != kNoMove || com_waiting_book_;
	case ScriptWait::Reduce: return reduce_.state != ReduceJob::None || learning_;
	case ScriptWait::Analysis: {
		const Position &p = game_.now();
		if (!show_eval_ || p.game_over()) return false;
		const PosInfo *info = cache_.peek(p);
		if (!info) return true;
		const bool book = !use_book_ || !eng_[0].alive || learning_ || info->book_done || info->book_failed;
		const bool search = !eng_[1].alive || (info->search_done && info->search_level == level_);
		return !(book && search) || (tab_ == Tab::Tree && tree_waiting());
	}
	default: return false;
	}
}

void App::pump_script()
{
	while (!script_.empty() || script_wait_ != ScriptWait::None) {
		if (book_saving_) return;  // the main engine saves its book as it ends: the next line after it (end_save)
		if (script_wait_ != ScriptWait::None) {
			if (script_waiting()) return;
			// a wait that ended by its time (wait, waitgame), not by what it waited for: said in the log
			if ((script_wait_ == ScriptWait::Game || script_wait_ == ScriptWait::Analysis) && script_condition())
				log(lang::t("script.wait_timeout"));
			script_wait_ = ScriptWait::None;
			kill_timer(kTimerScript);
		}
		if (script_.empty()) break;
		const ScriptLine l = script_.front();
		script_.pop_front();
		std::wstring err;
		if (!script_command(l.text, l.depth, &err)) {
			log(lang::f("script.error", {widen(l.text), err}));
			script_.clear();
			script_wait_ = ScriptWait::None;
			break;
		}
	}
	if (script_.empty() && script_wait_ == ScriptWait::None) update_menu();
}

bool App::script_command(const std::string &line, int depth, std::wstring *error)
{
	using lang::t;
	const std::vector<std::string> w = words(line);
	if (w.empty()) return true;
	const std::string c = lower(w[0]);
	const std::string a1 = w.size() > 1 ? lower(w[1]) : std::string();
	auto bad = [&](const char *key) {
		*error = t(key);
		return false;
	};
	auto path = [&](const std::string &p) { return absolute(script_dir_.empty() ? exe_dir_ : script_dir_, p); };
	auto on_off = [&](bool *v) {
		if (a1 == "on") *v = true;
		else if (a1 == "off") *v = false;
		else return false;
		return true;
	};
	// on|off of a setting: its menu command turns it over when it changes
	auto toggle = [&](bool now, int command) {
		bool v = now;
		if (!on_off(&v)) return bad("script.on_off");
		if (v != now) on_command(command);
		return true;
	};
	// a setting given by its name: the menu command of that name (`first` for the first of `names`)
	auto by_name = [&]<size_t N>(const char *const (&names)[N], int first) {
		const int k = name_index(names, a1);
		if (k < 0) return bad("script.bad_word");
		on_command(first + k);
		return true;
	};
	auto wait = [&](ScriptWait sw) { script_wait_ = sw; };

	if (c == "tree") {
		if (!tree_command(w, rest_after(line, 1))) return bad("script.bad_word");
		return true;
	}
	if (c == "log") {
		log(widen(rest_after(line, 1)));
		return true;
	}
	if (c == "moves" || c == "paste") {
		if (!paste_text(rest_after(line, 1))) return bad("script.bad_moves");
		return true;
	}
	if (c == "board") {
		if (!take_position_text(rest_after(line, 1))) return bad("script.bad_board");
		return true;
	}
	if (c == "play") {
		const bool playing = play_mode_ != PlayMode::Free;
		for (const int m : parse_moves(rest_after(line, 1)).moves) {
			const int side = clock_side_;
			if (clock_ran_out()) return bad(side == kBlack ? "play.time_out_black" : "play.time_out_white");
			if (!game_.play(m)) return bad("script.bad_moves");
			if (playing) position_changed();
		}
		if (!playing) position_changed();
		return true;
	}
	if (c == "new") {
		on_command(cmd::NewGame);
		return true;
	}
	if (c == "goto") {
		if (a1 == "end") game_.go_to(game_.size());
		else if (auto n = number(a1)) game_.go_to(*n);
		else return bad("script.bad_number");
		position_changed();
		return true;
	}
	if (c == "back" || c == "forward") {
		const int n = w.size() > 1 ? number(a1).value_or(1) : 1;
		for (int k = 0; k < n; ++k)
			if (!(c == "back" ? game_.back() : game_.forward())) break;
		position_changed();
		return true;
	}
	if (c == "book") {
		if (a1 == "reload") {
			on_command(cmd::ReloadBook);
			return true;
		}
		if (a1 == "reduce") {  // book reduce X Y [here]: counted, then done without asking
			const auto x = w.size() > 2 ? number(w[2]) : std::nullopt, y = w.size() > 3 ? number(w[3]) : std::nullopt;
			if (!x || !y || *x < 0 || *x > 129 || *y < 0 || *y > 254) return bad("script.bad_number");
			const bool here = w.size() > 4 && lower(w[4]) == "here";
			if (w.size() > 5 || (w.size() > 4 && !here)) return bad("script.bad_word");
			if (!eng_[0].alive) return bad("learn.no_engine");
			if (learning_) return bad("learn.busy");
			if (!reduce_book(*x, *y, !here, false)) return bad("reduce.busy");  // (the rest is looked at above)
			wait(ScriptWait::Reduce);
			return true;
		}
		if (a1 == "deviate" || a1 == "deviate2" || a1 == "deviate3") {  // book deviate.. X Y export <file> [leaf|ends|all]
			const auto x = w.size() > 2 ? number(w[2]) : std::nullopt, y = w.size() > 3 ? number(w[3]) : std::nullopt;
			if (!x || !y || *x < 0 || *x > 129 || *y < 0 || *y > (a1 == "deviate" ? 65 : 254)) return bad("script.bad_number");
			if (w.size() < 6 || lower(w[4]) != "export") return bad("script.bad_word");
			std::string file, which;
			if (const char *e = file_then_word(text_after(line, 5), &file, &which)) return bad(e);
			which = which.empty() ? "leaf" : lower(which);
			if (which != "leaf" && which != "ends" && which != "all") return bad("script.bad_word");
			if (!eng_[0].alive) return bad("learn.no_engine");
			if (eng_[0].probe_id) return bad("devexp.loading");  // (not known while the engine reads its book)
			if (!eng_[0].exports) return bad("devexp.old_engine");
			if (learning_) return bad("learn.busy");
			// (the state is the one looked at above: the name of the file could not be given to the engine)
			if (!start_deviate_export(a1, *x, *y, which, path(file))) return bad("script.bad_file");
			wait(ScriptWait::Learn);
			return true;
		}
		return toggle(use_book_, cmd::UseBook);
	}
	if (c == "eval") return toggle(show_eval_, cmd::ShowEval);
	if (c == "numbers") return toggle(move_numbers_, cmd::MoveNumbers);
	if (c == "level") {
		auto n = number(a1);
		if (!n || *n < 0 || *n > 60) return bad("script.bad_number");
		level_ = *n;
		update_menu();
		request_analysis();
		invalidate();  // (the ranges of the level on the graph)
		return true;
	}
	if (c == "umi") {
		if (a1 == "best") on_command(cmd::UmiBest);
		else if (a1 == "loss") on_command(cmd::UmiLoss);
		else if (a1 == "off") on_command(cmd::UmiOff);
		else return bad("script.bad_word");
		return true;
	}
	if (c == "engine") {
		const EngineKind k = a1 == "newedax" ? EngineKind::Newedax : EngineKind::Nikque18;
		if (a1 != "newedax" && a1 != "nikque" && a1 != "nikque18") return bad("script.bad_word");  // (nikque18: up to 0.2.0)
		if (k != kind_) {
			// (before the kind changes: restart_engines would refuse after it, in a box, the engine of the kind before
			// going on)
			if (book_saving_) return bad("save.busy");
			if (learning_) return bad("learn.busy");
			kind_ = k;
			update_menu();
			restart_engines();
		}
		return true;
	}
	if (c == "pv") return toggle(pv_on_board_, cmd::PvOnBoard);
	if (c == "precision") return toggle(precision_, cmd::PrecisionOn);
	if (c == "bookmarks") return toggle(bookmarks_shown_, cmd::BookmarksShow);
	if (c == "openingmarks") return toggle(opening_marks_, cmd::OpeningMarks);
	if (c == "recordbox") return toggle(record_box_, cmd::RecordBoxShow);
	if (c == "graphend") return toggle(graph_end_, cmd::GraphEndShow);
	if (c == "uppermoves") return toggle(upper_moves_, cmd::UpperMoves);
	if (c == "recordtext") {
		if (!record_take(rest_after(line, 1))) return bad("script.bad_moves");
		return true;
	}
	if (c == "analysis") {
		if (a1 == "backward") on_command(cmd::Backward);
		else if (a1 == "forward") on_command(cmd::Forward);
		else if (a1 == "off") on_command(cmd::BackgroundOff);
		else return bad("script.bad_word");
		return true;
	}
	if (c == "stop") {
		on_command(cmd::TaskCancel);
		return true;
	}
	if (c == "bookmark") {
		auto n = number(a1);
		if (!n || *n < 1 || *n > static_cast<int>(kept_.size())) return bad("script.bad_number");
		kept_sel_ = *n - 1;
		on_command(cmd::KeptShow);
		return true;
	}
	if (c == "design") {
		if (a1 != "toggle") return by_name(kDesignNames, cmd::DesignStandard);
		on_command(cmd::DesignToggle);
		return true;
	}
	if (c == "effects") return by_name(kEffectsNames, cmd::EffectsOn);
	if (c == "tab") return by_name(kTabNames, cmd::TabMoves);
	if (c == "rotate") {
		auto n = number(a1);
		if (!n || *n < 0 || *n > 7) return bad("script.bad_number");
		orient_to(*n);
		return true;
	}
	if (c == "cellinfo") return by_name(kCellInfoNames, cmd::CellInfoNone);
	if (c == "estimate") {
		estimate_line();
		wait(ScriptWait::FindLine);
		return true;
	}
	if (c == "keep") {
		keep_now(false);
		return true;
	}
	if (c == "screenshot") {
		const std::string f = rest_after(line, 1);
		screenshot(f.empty() ? std::wstring() : path(f));
		return true;
	}
	if (c == "wait") {
		const int s = w.size() > 1 ? number(a1).value_or(30) : 30;
		script_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(std::clamp(s, 1, 3600));
		wait(ScriptWait::Analysis);
		set_timer(kTimerScript, 250);
		return true;
	}
	if (c == "search") {
		ini_.set_int("list", "search_symmetries", a1 != "exact");
		list_search();
		wait(ScriptWait::Task);
		return true;
	}
	if (c == "list") {
		if (task_busy_) return bad("task.busy");
		if (a1 == "load" || a1 == "add") {
			list_load(path(rest_after(line, 2)), a1 == "add");
			wait(ScriptWait::Task);
			return true;
		}
		if (a1 == "save") {
			const std::wstring f = path(rest_after(line, 2));
			if (!write_list(f, list_)) return bad("status.write_failed");
			log(lang::f("list.saved", {std::to_wstring(list_.size()), f}));
			return true;
		}
		auto job = [&](int id) {
			list_command(id);
			wait(ScriptWait::Task);
			return true;
		};
		if (a1 == "sort") return job(cmd::ListSort);
		if (a1 == "unique") return job(cmd::ListUnique);
		if (a1 == "normalize") return job(cmd::ListNormalize);
		if (a1 == "delete") return job(cmd::ListDelete);
		if (a1 == "next") return job(cmd::ListNext);
		if (a1 == "prev") return job(cmd::ListPrev);
		if (a1 == "addgame") return job(cmd::ListAddGame);
		if (a1 == "clear") {
			clear_list();
			return true;
		}
		const std::optional<int> n = w.size() > 2 ? number(w[2]) : std::nullopt;
		if (a1 == "truncate" && n) {
			const int k = *n;
			list_job_script(t("menu.list.truncate"), [k](GameList &l) { truncate_list(l, k); });
			wait(ScriptWait::Task);
			return true;
		}
		if (a1 == "shuffle") {
			const std::uint64_t seed = n ? static_cast<std::uint64_t>(*n) : 1;
			list_job_script(t("menu.list.shuffle"), [seed](GameList &l) { shuffle_list(l, seed); });
			wait(ScriptWait::Task);
			return true;
		}
		if (a1 == "scatter" && w.size() > 2) {
			const std::string side = lower(w[2]);
			const int sides = side == "black" ? 1 : side == "white" ? 2 : side == "both" ? 3 : 0;
			if (!sides) return bad("script.bad_word");
			const std::uint64_t seed = w.size() > 3 ? static_cast<std::uint64_t>(number(w[3]).value_or(1)) : 1;
			list_job_script(t("menu.list.scatter"), [sides, seed](GameList &l) { scatter_list(l, seed, sides); });
			wait(ScriptWait::Task);
			return true;
		}
		if (a1 == "book") {
			auto arg = [&](size_t k, int def) { return w.size() > k ? number(w[k]).value_or(def) : def; };
			const bool add = w.size() > 5 && lower(w[5]) == "add";
			if (!eng_[0].alive) return bad("learn.no_engine");
			if (learning_) return bad("learn.busy");
			// (the state is the one looked at above: the name of the file in the temporary folder could not be given)
			if (!ask_book_lines(arg(2, 0), arg(3, 60), arg(4, 10000), add)) return bad("script.bad_file");
			wait(ScriptWait::BookLines);
			return true;
		}
		if (a1 == "start" && n) {
			list_start_ply_ = std::clamp(*n, -1, 60);
			return true;
		}
		if (a1 == "select" && n) {
			if (*n < 1 || *n > static_cast<int>(list_.size())) return bad("script.bad_number");
			list_select(*n - 1);
			return true;
		}
		return bad("script.bad_word");
	}
	// the command for the engine in its code page; file names in "..." (also with spaces) are given to it as it can
	// open them: a file there is read (rewritten by base correct and complete), a file not there is written
	auto for_engine = [&](const std::string &cmd, int role, std::string *out) -> const char * {
		const bool rewrites = cmd.starts_with("base correct") || cmd.starts_with("base complete");
		std::string r;
		size_t i = 0;
		for (size_t a; (a = cmd.find('"', i)) != std::string::npos;) {
			const size_t b = cmd.find('"', a + 1);
			if (b == std::string::npos) return "script.bad_file";
			const auto text = engine_command(std::string_view(cmd).substr(i, a - i));
			if (!text) return "err.command_letters";
			const std::wstring f = path(cmd.substr(a + 1, b - a - 1));
			const bool there = path_exists(f);
			const auto p = engine_path_or_say(f, there ? (rewrites ? FileUse::Rewrite : FileUse::Read) : FileUse::Write, role);
			if (!p) return "script.bad_file";
			r += *text + *p;
			i = b + 1;
		}
		const auto text = engine_command(std::string_view(cmd).substr(i));
		if (!text) return "err.command_letters";
		*out = r + *text;
		return nullptr;
	};
	if (c == "learn") {
		if (a1 == "start") {
			if (learn_queue_.empty() || learning_) return true;
			if (!eng_[0].alive) return bad("learn.no_engine");
			queue_running_ = true;
			next_in_queue();
			wait(ScriptWait::Learn);
			return true;
		}
		// the command as it is, in the code page of the engine (names in "..." are not changed: the job runs later)
		LearnJob j;
		const auto command = engine_command(rest_after(line, 1));
		if (!command) return bad("err.command_letters");
		if (command->empty()) return bad("script.bad_word");
		j.command = *command;
		j.start = game_.start();
		j.moves = moves_to_cursor();
		learn_queue_.push_back(std::move(j));
		update_menu();
		return true;
	}
	if (c == "edax") {
		if (!eng_[0].alive) return bad("learn.no_engine");
		if (learning_) return bad("learn.busy");
		std::string cmd;
		if (const char *e = for_engine(rest_after(line, 1), 0, &cmd)) return bad(e);
		start_learning(cmd);
		wait(ScriptWait::Learn);
		return true;
	}
	if (c == "tool") {
		if (tool_busy_) return bad("tool.busy");
		std::string cmd;
		if (const char *e = for_engine(rest_after(line, 1), 2, &cmd)) return bad(e);
		if (!start_tool(cmd, widen(rest_after(line, 1)))) return bad("tool.not_started");  // (busy: looked at above)
		wait(ScriptWait::Tool);
		return true;
	}
	if (c == "run") {
		run_script_file(path(rest_after(line, 1)), depth);
		return true;
	}
	if (c == "game") {
		const char *names[] = {"free", "black", "white", "engines"};
		for (int k = 0; k < 4; ++k)
			if (a1 == names[k]) {
				const auto games = w.size() > 2 ? number(w[2]) : std::optional<int>(1);
				if (!games || *games < 1 || *games > 10000) return bad("script.bad_word");
				set_play_mode(static_cast<PlayMode>(k), *games);
				return true;
			}
		return bad("script.bad_word");
	}
	if (c == "comlevel") {
		auto n = number(a1);
		if (!n || *n < 1 || *n > 60) return bad("script.bad_number");
		com_level_ = *n;
		return true;
	}
	if (c == "opening") {
		const int k = name_index(kOpeningNames, a1);
		if (k < 0) return bad("script.bad_word");
		opening_ = static_cast<Opening>(k);
		if (w.size() > 2) opening_width_ = std::clamp(number(w[2]).value_or(opening_width_), 0, 64);
		if (w.size() > 3) opening_until_ = std::clamp(number(w[3]).value_or(opening_until_), 0, 60);
		update_menu();
		return true;
	}
	if (c == "clock") {
		auto n = number(a1);
		if (!n || *n < 0 || *n > 600) return bad("script.bad_number");
		clock_minutes_ = *n;
		return true;
	}
	if (c == "ponder") return toggle(ponder_, cmd::PlayPonder);
	if (c == "keep-hash") return toggle(keep_hash_, cmd::PlayKeepHash);
	if (c == "go") {
		com_once_ = true;
		com_once_pos_ = key_of(game_.now());
		com_think();
		wait(ScriptWait::Com);
		set_timer(kTimerScript, 250);
		return true;
	}
	if (c == "takeback") {
		if (take_back()) position_changed();
		return true;
	}
	if (c == "waitgame") {
		const int s = w.size() > 1 ? number(a1).value_or(600) : 600;
		script_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(std::clamp(s, 1, 36000));
		wait(ScriptWait::Game);
		set_timer(kTimerScript, 250);
		return true;
	}
	if (c == "compare") {
		bool v = compare_;
		if (!on_off(&v)) return bad("script.on_off");
		if (v != compare_) set_compare(v);
		return true;
	}
	if (c == "savelog") {
		if (!save_log(path(rest_after(line, 1)))) return bad("status.write_failed");
		return true;
	}
	if (c == "lang") {
		const std::string code = w.size() > 1 ? w[1] : std::string();
		if (name_index(kLanguages, code) < 0) return bad("script.bad_word");
		set_language(code);
		return true;
	}
	if (c == "quit") {
		script_.clear();
		script_quit_ = true;
		post(WM_CLOSE);
		return true;
	}
	return bad("script.unknown");
}

void App::schedule_script()
{
	if (!script_.empty() || script_wait_ != ScriptWait::None) post(WM_SCRIPT);
}

}  // namespace ng
