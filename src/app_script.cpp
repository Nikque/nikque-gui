// Command scripts: a text file of commands (one a line, "#" notes) run one after another, also at the start
// (config.ini [script] startup). A command that starts work (reading a list, learning, a tool, the analysis of
// the position) waits for its end before the next line. The commands are listed in readme.txt.
#include <windows.h>

#include <cstdio>

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

std::string lower(std::string s)
{
	for (auto &c : s)
		if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
	return s;
}

// the text after the first n words
std::string rest_after(const std::string &line, int n)
{
	size_t i = 0;
	for (int k = 0; k < n; ++k) {
		while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
		while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
	}
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
	std::string r = line.substr(i);
	if (r.size() >= 2 && r.front() == '"' && r.back() == '"') r = r.substr(1, r.size() - 2);
	return r;
}

std::optional<int> number(const std::string &s)
{
	if (s.empty()) return std::nullopt;
	char *end = nullptr;
	const long v = std::strtol(s.c_str(), &end, 10);
	if (*end) return std::nullopt;
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
	FILE *f = nullptr;
	if (_wfopen_s(&f, file.c_str(), L"rb") != 0 || !f) {
		log(lang::f("script.cannot_open", {file}));
		return;
	}
	std::string all;
	char buf[1 << 14];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0) all.append(buf, n);
	fclose(f);
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
		const bool book = !use_book_ || !eng_[0].alive || learning_ || info->book_done;
		const bool search = !eng_[1].alive || (info->search_done && info->search_level == level_);
		return !(book && search) || (tab_ == Tab::Tree && tree_waiting());
	}
	default: return false;
	}
}

void App::pump_script()
{
	while (!script_.empty() || script_wait_ != ScriptWait::None) {
		if (script_wait_ != ScriptWait::None) {
			if (script_waiting()) return;
			// a wait that ended by its time (wait, waitgame), not by what it waited for: said in the log
			if ((script_wait_ == ScriptWait::Game || script_wait_ == ScriptWait::Analysis) && script_condition())
				log(lang::t("script.wait_timeout"));
			script_wait_ = ScriptWait::None;
			KillTimer(hwnd_, 3);
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
	auto path = [&](const std::string &p) {
		const std::wstring wp = widen(p);
		if (wp.size() >= 2 && (wp[1] == L':' || (wp[0] == L'\\' && wp[1] == L'\\'))) return wp;
		return (script_dir_.empty() ? exe_dir_ : script_dir_) + L"\\" + wp;
	};
	auto on_off = [&](bool *v) {
		if (a1 == "on") *v = true;
		else if (a1 == "off") *v = false;
		else return false;
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
		for (const int m : parse_moves(rest_after(line, 1)).moves)
			if (!game_.play(m)) return bad("script.bad_moves");
		position_changed();
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
			if (w.size() > 4 && !here) return bad("script.bad_word");
			if (!eng_[0].alive) return bad("learn.no_engine");
			if (learning_) return bad("learn.busy");
			if (!reduce_book(*x, *y, !here, false)) return bad("learn.busy");
			wait(ScriptWait::Reduce);
			return true;
		}
		bool v = use_book_;
		if (!on_off(&v)) return bad("script.on_off");
		if (v != use_book_) on_command(cmd::UseBook);
		return true;
	}
	if (c == "eval") {
		bool v = show_eval_;
		if (!on_off(&v)) return bad("script.on_off");
		if (v != show_eval_) on_command(cmd::ShowEval);
		return true;
	}
	if (c == "numbers") {
		bool v = move_numbers_;
		if (!on_off(&v)) return bad("script.on_off");
		if (v != move_numbers_) on_command(cmd::MoveNumbers);
		return true;
	}
	if (c == "level") {
		auto n = number(a1);
		if (!n || *n < 0 || *n > 60) return bad("script.bad_number");
		level_ = *n;
		update_menu();
		request_analysis();
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
			kind_ = k;
			update_menu();
			restart_engines();
		}
		return true;
	}
	if (c == "pv" || c == "precision" || c == "bookmarks") {
		bool *flag = c == "pv" ? &pv_on_board_ : c == "precision" ? &precision_ : &bookmarks_shown_;
		bool v = *flag;
		if (!on_off(&v)) return bad("script.on_off");
		if (v != *flag) on_command(c == "pv" ? cmd::PvOnBoard : c == "precision" ? cmd::PrecisionOn : cmd::BookmarksShow);
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
		if (a1 == "standard") on_command(cmd::DesignStandard);
		else if (a1 == "paw") on_command(cmd::DesignPaw);
		else if (a1 == "toggle") on_command(cmd::DesignToggle);
		else return bad("script.bad_word");
		return true;
	}
	if (c == "effects") {
		if (a1 == "on") on_command(cmd::EffectsOn);
		else if (a1 == "subdued") on_command(cmd::EffectsSubdued);
		else if (a1 == "off") on_command(cmd::EffectsOff);
		else return bad("script.bad_word");
		return true;
	}
	if (c == "tab") {
		const char *names[] = {"moves", "book", "record", "list", "tree"};
		for (int k = 0; k < 5; ++k)
			if (a1 == names[k]) {
				on_command(cmd::TabMoves + k);
				return true;
			}
		return bad("script.bad_word");
	}
	if (c == "rotate") {
		auto n = number(a1);
		if (!n || *n < 0 || *n > 7) return bad("script.bad_number");
		orient_to(*n);
		return true;
	}
	if (c == "cellinfo") {
		const char *names[] = {"none", "positions", "level", "stage", "umi"};
		for (int k = 0; k < 5; ++k)
			if (a1 == names[k]) {
				on_command(cmd::CellInfoNone + k);
				return true;
			}
		return bad("script.bad_word");
	}
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
		SetTimer(hwnd_, 3, 250, nullptr);
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
			list_.clear();
			list_index_ = -1;
			hits_.clear();
			hits_shown_ = false;
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
			if (!ask_book_lines(arg(2, 0), arg(3, 60), arg(4, 10000), add)) return bad("learn.no_engine");
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
			const bool there = GetFileAttributesW(f.c_str()) != INVALID_FILE_ATTRIBUTES;
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
		if (!start_tool(cmd, widen(rest_after(line, 1)))) return bad("tool.busy");
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
				set_play_mode(static_cast<PlayMode>(k), w.size() > 2 ? number(w[2]).value_or(1) : 1);
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
		if (a1 == "search") opening_ = Opening::Search;
		else if (a1 == "best") opening_ = Opening::BookBest;
		else if (a1 == "wide") opening_ = Opening::BookWide;
		else return bad("script.bad_word");
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
	if (c == "ponder") {
		bool v = ponder_;
		if (!on_off(&v)) return bad("script.on_off");
		if (v != ponder_) on_command(cmd::PlayPonder);
		return true;
	}
	if (c == "go") {
		com_once_ = true;
		com_once_pos_ = key_of(game_.now());
		com_think();
		wait(ScriptWait::Com);
		SetTimer(hwnd_, 3, 250, nullptr);
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
		SetTimer(hwnd_, 3, 250, nullptr);
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
		if (code != "ja" && code != "en" && code != "zh-Hans" && code != "zh-Hant") return bad("script.bad_word");
		set_language(code);
		return true;
	}
	if (c == "quit") {
		script_.clear();
		PostMessageW(hwnd_, WM_CLOSE, 0, 0);
		return true;
	}
	return bad("script.unknown");
}

}  // namespace ng

namespace ng {

void App::schedule_script()
{
	if (!script_.empty() || script_wait_ != ScriptWait::None) PostMessageW(hwnd_, WM_SCRIPT, 0, 0);
}

}  // namespace ng
