// The tool engine (solve, bench, count, perft, estimate and the base commands: a third engine started for one
// job and closed after it, reading no book), the book commands of the menus that run on the main engine like
// book learning, and the file dialogs.
#include "platform.h"

#include <cwctype>

#include "app.h"
#include "form.h"
#include "lang.h"
#include "paths.h"

namespace ng {

std::wstring file_name(const std::wstring &p)
{
	const size_t s = p.find_last_of(L"\\/");
	return s == std::wstring::npos ? p : p.substr(s + 1);
}

std::wstring absolute(const std::wstring &base, const std::string &name)
{
	const std::wstring w = widen(name);
	if (is_full_path(w)) return w;
	return path_join(base, w);
}

std::wstring file_filter(const char *key, const wchar_t *patterns)
{
	std::wstring f = lang::t(key) + L" (" + patterns + L")";
	f += L'\0';
	f += patterns;
	f += L'\0';
	f += lang::t("filter.all");
	f += L'\0';
	f += L"*.*";
	f += L'\0';
	return f;
}

std::optional<int> ask_number(HWND owner, const std::wstring &title, const std::wstring &label, int value, int lo, int hi,
                              const std::wstring &note)
{
	std::vector<FormField> f(1);
	f[0] = number_field(label, value, lo, hi, note);
	if (!run_form(owner, title, f)) return std::nullopt;
	return std::stoi(f[0].text);
}

namespace {

constexpr const wchar_t *kGames = L"*.txt;*.ggf;*.sgf;*.pgn;*.wtb;*.edx";
constexpr const wchar_t *kBooks = L"*.dat;*.dev*;*.store;*.mrg;*.enh;*.fill*;*.lexp;*.leaf*;*.play;*.gam";

}  // namespace

std::optional<std::string> App::engine_path_or_say(const std::wstring &path, FileUse use, int role)
{
	if (use == FileUse::Replace) return replace_path_or_say(path, role);
	if (auto p = engine_path(path, use != FileUse::Write)) return p;
	// a drive without short names: the file goes through the temporary folder (the extension is kept: it
	// tells the engine the form of the file)
	const size_t slash = path.find_last_of(L"\\/"), dot = path.find_last_of(L'.');
	const std::wstring ext = dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash) ? path.substr(dot) : L"";
	const std::wstring tmp = path_join(temp_dir_, L"f" + std::to_wstring(++temp_files_) + ext);
	const auto tp = engine_path(tmp, false);
	if (!tp) {
		message(lang::f("err.engine_path", {path}), MB_ICONWARNING);
		return std::nullopt;
	}
	if (use != FileUse::Write) {
		if (!copy_file(path, tmp)) {
			message(lang::f("err.engine_path", {path}), MB_ICONWARNING);
			return std::nullopt;
		}
		log(lang::f("log.file_via_temp", {path}));
	}
	if (use != FileUse::Read) moves_after_job_.push_back({tmp, path, role});  // written there, then moved
	return tp;
}

// A file the job writes whole: the engine writes it in a folder of its own in the temporary folder (a new file: the
// engine adds to a file already there), and it takes the place of the file only when the job has ended well
// (finish_moves); the file there stays as it was when the job is not started, is stopped or says an error. A WThor
// file goes with the players of its folder (WTHOR.JOU), which the engine reads and adds to.
std::optional<std::string> App::replace_path_or_say(const std::wstring &path, int role)
{
	const size_t slash = path.find_last_of(L"\\/"), dot = path.find_last_of(L'.');
	std::wstring ext = dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash) ? path.substr(dot) : L"";
	const std::wstring dir = path_join(temp_dir_, L"r" + std::to_wstring(++temp_files_));
	const std::wstring tmp = path_join(dir, L"out" + ext);
	std::optional<std::string> tp;
	if (make_dir(dir) || dir_exists(dir)) tp = engine_path(tmp, false);
	if (!tp) {
		message(lang::f("err.engine_path", {path}), MB_ICONWARNING);
		return std::nullopt;
	}
	PendingMove m{tmp, path, role, true};
	for (auto &c : ext) c = static_cast<wchar_t>(std::towlower(c));
	if (ext == L".wtb") {
		const std::wstring players = path_join(slash == std::wstring::npos ? std::wstring() : path.substr(0, slash), L"WTHOR.JOU");
		if (file_exists(players)) {
			const std::wstring here = path_join(dir, L"WTHOR.JOU");
			if (!copy_file(players, here)) {
				message(lang::f("err.engine_path", {path}), MB_ICONWARNING);
				return std::nullopt;
			}
			m.also_from = here;
			m.also_to = players;
		}
	}
	moves_after_job_.push_back(std::move(m));
	return tp;
}

void App::job_error(int role)
{
	for (auto &m : moves_after_job_)
		if (m.role == role && m.replace) m.failed = true;
}

namespace {

// `from` put in the place of `to`: renamed when both are on the same drive, else copied next to `to` and that copy
// renamed over it (`to` is never left half written). Taken away from its place when it could be.
bool put_in_place(const std::wstring &from, const std::wstring &to)
{
	if (move_file(from, to, false)) return true;
	const std::wstring part = to + L".part" + std::to_wstring(process_id());
	if (!copy_file(from, part) || !move_file(part, to, false)) {
		delete_file(part);
		return false;
	}
	delete_file(from);  // (one left in the temporary folder goes with it when the program ends)
	return true;
}

}  // namespace

// after a job of the engine: the files it wrote in the temporary folder go to their places (a file that replaces
// another only when the job has ended well and said no error)
void App::finish_moves(int role, bool ended_well)
{
	for (auto it = moves_after_job_.begin(); it != moves_after_job_.end();) {
		if (it->role != role) {
			++it;
			continue;
		}
		if (it->replace && (!ended_well || it->failed)) {
			if (path_exists(it->from)) log(lang::f("log.file_not_replaced", {it->to}));
			delete_file(it->from);
			if (!it->also_from.empty()) delete_file(it->also_from);
		} else if (path_exists(it->from)) {
			// (the players first: their list only grows, so it also suits the games that are there)
			bool ok = it->also_from.empty() || !path_exists(it->also_from) || put_in_place(it->also_from, it->also_to);
			ok = ok && put_in_place(it->from, it->to);
			log(lang::f(ok ? "log.file_moved" : "log.file_not_moved", {it->to, it->from}));
		}
		it = moves_after_job_.erase(it);
	}
}

// the lines of the log as a text file (UTF-8)
bool App::save_log(const std::wstring &path) const
{
	std::string out;
	for (const auto &l : log_) out += narrow(l) + "\r\n";
	FILE *f = open_file(path, L"wb");
	if (!f) return false;
	const bool ok = fwrite(out.data(), 1, out.size(), f) == out.size();
	return fclose(f) == 0 && ok;
}

void App::open_log()
{
	if (log_open_) return;
	log_open_ = true;
	update_menu();
	layout();
	invalidate();
}

// ---- the tool engine

bool App::start_tool(const std::string &commands, const std::wstring &label)
{
	if (tool_busy_) {
		message(lang::t("tool.busy"), MB_ICONINFORMATION);
		return false;
	}
	if (!engine_files_ok(kind_)) return false;
	const std::wstring exe = engine_exe();
	EngineSlot &s = eng_[2];
	s.proc.close(0);
	const auto nb = nobook_args(L"nobook-tool.dat");
	if (!nb) {
		log(lang::f("err.engine_name", {temp_dir_}));
		return false;
	}
	const auto common = engine_args();
	if (!common) return false;
	const std::wstring args = *common + *nb;
	++s.generation;
	s.lost = false;
	std::wstring err;
	s.alive = s.proc.start(exe, args, engine_dir(), hwnd_, WM_ENGINE, engine_tag(2, s.generation), &err);
	if (!s.alive) {
		log(lang::f("log.start_failed", {exe, err}));
		return false;
	}
	tool_end_id_ = new_id();
	std::string out = "mode 3\nsetboard " + game_.now().to_board_string() + "\n" + commands;
	if (!out.ends_with('\n')) out += '\n';
	out += "echo M" + std::to_string(tool_end_id_) + "\n";  // answered when the commands have ended
	tool_busy_ = true;
	tool_label_ = label;
	tool_t0_ = std::chrono::steady_clock::now();
	send(2, out);
	log(lang::f("tool.started", {label, std::to_wstring(level_)}));
	open_log();
	update_menu();
	return true;
}

void App::tool_line(const std::string &line)
{
	if (const int m = echo_marker(line); m >= 0) {
		if (tool_busy_ && m == tool_end_id_) tool_ended(false);
		return;
	}
	if (line.starts_with("ERROR")) job_error(2);
	if (line.starts_with("New book")) return;  // the empty book the tool engine makes for itself
	log(lang::t("engine.tool") + L": " + engine_text(line));
}

void App::tool_ended(bool stopped)
{
	const auto s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - tool_t0_).count();
	tool_busy_ = false;
	tool_end_id_ = 0;
	log(lang::f(stopped ? "tool.stopped" : "tool.ended", {tool_label_, std::to_wstring(s / 60), std::to_wstring(s % 60)}));
	eng_[2].proc.close(stopped ? 0 : 2000);
	eng_[2].alive = false;
	finish_moves(2, !stopped);
	delete_file(path_join(temp_dir_, L"nobook-tool.dat"));
	update_menu();
	invalidate();
	schedule_script();
}

void App::stop_tool()
{
	if (tool_busy_) tool_ended(true);
}

void App::tool_command(int id)
{
	using lang::t;
	if (id == cmd::ToolStop) {
		stop_tool();
		return;
	}
	std::string c;
	std::wstring label;
	switch (id) {
	case cmd::ToolSolve: {
		const std::wstring f = pick_open(file_filter("filter.obf", L"*.obf").c_str(), t("menu.tool.solve"));
		if (f.empty()) return;
		const auto p = engine_path_or_say(f, FileUse::Read, 2);
		if (!p) return;
		c = "solve " + *p;
		label = L"solve " + file_name(f);
		break;
	}
	case cmd::ToolBench: {
		const auto n = ask_number(hwnd_, t("menu.tool.bench"), t("tool.bench.n"), ini_.get_int("tools", "bench", 10, 1, 100000), 1, 100000, t("tool.bench.note"));
		if (!n) return;
		ini_.set_int("tools", "bench", *n);
		c = "bench " + std::to_string(*n);
		label = engine_text(c);
		break;
	}
	case cmd::ToolCount: {
		std::vector<FormField> f(2);
		f[0].kind = FormField::Choice;
		f[0].label = t("tool.count.kind");
		f[0].choices = {t("tool.count.games"), t("tool.count.positions"), t("tool.count.shapes")};
		f[0].choice = ini_.get_int("tools", "count_kind", 0, 0, 2);
		f[1] = number_field(t("tool.depth"), ini_.get_int("tools", "count_depth", 8, 1, 60), 1, 60, t("tool.depth.note"));
		if (!run_form(hwnd_, t("menu.tool.count"), f)) return;
		ini_.set_int("tools", "count_kind", f[0].choice);
		ini_.set("tools", "count_depth", narrow(f[1].text));
		c = std::string("count ") + (f[0].choice == 1 ? "positions " : f[0].choice == 2 ? "shapes " : "games ") + narrow(f[1].text);
		label = engine_text(c);
		break;
	}
	case cmd::ToolPerft: {
		const auto n = ask_number(hwnd_, t("menu.tool.perft"), t("tool.depth"), ini_.get_int("tools", "perft", 8, 1, 60), 1, 60, t("tool.depth.note"));
		if (!n) return;
		ini_.set_int("tools", "perft", *n);
		c = "perft " + std::to_string(*n);
		label = engine_text(c);
		break;
	}
	case cmd::ToolEstimate: {
		const auto n = ask_number(hwnd_, t("menu.tool.estimate"), t("tool.estimate.n"), ini_.get_int("tools", "estimate", 1000, 1, 100000000), 1, 100000000);
		if (!n) return;
		ini_.set_int("tools", "estimate", *n);
		c = "estimate " + std::to_string(*n);
		label = engine_text(c);
		break;
	}
	default: return;
	}
	start_tool(c, label);
}

// ---- the base commands (files of games), on the tool engine

void App::base_command(int id)
{
	using lang::t;
	const std::wstring games = file_filter("filter.games", kGames);
	auto in_file = [&](const std::wstring &title, bool rewrite = false) -> std::optional<std::pair<std::wstring, std::string>> {
		const std::wstring f = pick_open(games.c_str(), title);
		if (f.empty()) return std::nullopt;
		auto p = engine_path_or_say(f, rewrite ? FileUse::Rewrite : FileUse::Read, 2);
		if (!p) return std::nullopt;
		return std::pair{f, *p};
	};
	// the engine adds to a file already there: it writes a new one, which takes the place of the file the dialog
	// confirmed to replace when the job has ended well
	auto out_file = [&](const std::wstring &title, const std::wstring &in, const wchar_t *pat, const char *fkey,
	                    const wchar_t *ext) -> std::optional<std::string> {
		const std::wstring flt = file_filter(fkey, pat);
		const std::wstring f = pick_save(flt.c_str(), ext, L"", title);
		if (f.empty()) return std::nullopt;
		if (same_file(f, in)) {
			message(t("base.same_file"), MB_ICONWARNING);
			return std::nullopt;
		}
		return engine_path_or_say(f, FileUse::Replace, 2);
	};
	auto empties = [&](const std::wstring &title) {
		return ask_number(hwnd_, title, t("base.empties"), ini_.get_int("tools", "base_empties", 24, 0, 60), 0, 60, t("base.empties.note"));
	};
	auto rewrites = [&](const std::wstring &f) {
		return message(lang::f("base.rewrites", {f}), MB_OKCANCEL | MB_ICONWARNING) == IDOK;
	};
	std::string c;
	switch (id) {
	case cmd::BaseConvert:
	case cmd::BaseUnique: {
		const std::wstring title = t(id == cmd::BaseConvert ? "menu.base.convert" : "menu.base.unique");
		auto in = in_file(title);
		if (!in) return;
		auto out = out_file(title, in->first, kGames, "filter.games", L"txt");
		if (!out) return;
		c = std::string(id == cmd::BaseConvert ? "base convert " : "base unique ") + in->second + " " + *out;
		break;
	}
	case cmd::BaseCompare: {
		auto a = in_file(t("menu.base.compare"));
		if (!a) return;
		auto b = in_file(t("menu.base.compare"));
		if (!b) return;
		c = "base compare " + a->second + " " + b->second;
		break;
	}
	case cmd::BaseCheck:
	case cmd::BaseCorrect: {
		const std::wstring title = t(id == cmd::BaseCheck ? "menu.base.check" : "menu.base.correct");
		auto in = in_file(title, id == cmd::BaseCorrect);
		if (!in) return;
		const auto n = empties(title);
		if (!n) return;
		ini_.set_int("tools", "base_empties", *n);
		if (id == cmd::BaseCorrect && !rewrites(in->first)) return;
		c = std::string(id == cmd::BaseCheck ? "base check " : "base correct ") + in->second + " " + std::to_string(*n);
		break;
	}
	case cmd::BaseComplete: {
		auto in = in_file(t("menu.base.complete"), true);
		if (!in || !rewrites(in->first)) return;
		c = "base complete " + in->second;
		break;
	}
	case cmd::BaseProblem:
	case cmd::BaseTofen: {
		const std::wstring title = t(id == cmd::BaseProblem ? "menu.base.problem" : "menu.base.tofen");
		auto in = in_file(title);
		if (!in) return;
		const auto n = empties(title);
		if (!n) return;
		ini_.set_int("tools", "base_empties", *n);
		auto out = id == cmd::BaseProblem ? out_file(title, in->first, L"*.obf", "filter.obf", L"obf")
		                                  : out_file(title, in->first, L"*.fen;*.txt", "filter.fen", L"fen");
		if (!out) return;
		c = std::string(id == cmd::BaseProblem ? "base problem " : "base tofen ") + in->second + " " + std::to_string(*n) + " " + *out;
		break;
	}
	default: return;
	}
	start_tool(c, engine_text(c));
}

// ---- the book commands of the menus that run on the main engine (like book learning)

bool App::book_job(int id, std::string *command, std::wstring *warning)
{
	using lang::t;
	const std::string xy = " " + std::to_string(loss_x_) + " " + std::to_string(loss_y_);
	const std::wstring books = file_filter("filter.books", kBooks), texts = file_filter("filter.text", L"*.txt");
	switch (id) {
	case cmd::LearnCorrect: *command = "book correct"; return true;
	case cmd::LearnLeafRecalc1: *command = "book leaf recalculate" + xy; return true;
	case cmd::LearnLeafRecalc2: *command = "book leaf recalculate2" + xy; return true;
	case cmd::LearnLeafRecalc3: *command = "book leaf recalculate3" + xy; return true;
	case cmd::LearnLeafRecalc4: *command = "book leaf recalculate4" + xy; return true;
	case cmd::LearnMerge: {
		const std::wstring f = pick_open(books.c_str(), t("menu.learn.merge"));
		if (f.empty()) return false;
		auto p = engine_path_or_say(f, FileUse::Read, 0);
		if (!p) return false;
		*command = "book merge " + *p;
		*warning = t("warn.merge");
		return true;
	}
	case cmd::LearnSubtree:
		*command = "book subtree";
		*warning = t("warn.subtree") + L"\n" + t("warn.book_replaced");
		return true;
	case cmd::LearnPrune:
		*command = "book prune";
		*warning = t("warn.prune") + L"\n" + t("warn.book_replaced");
		return true;
	case cmd::LearnExport:
	case cmd::LearnExtract: {
		const bool ex = id == cmd::LearnExport;
		const std::wstring flt = ex ? texts : file_filter("filter.games", kGames);
		const std::wstring f = pick_save(flt.c_str(), L"txt", ex ? L"book-export.txt" : L"book-lines.txt",
		                                 t(ex ? "menu.learn.export" : "menu.learn.extract"));
		if (f.empty()) return false;
		// (the file confirmed to be replaced stays until the job has ended well: the engine writes a new one)
		auto p = engine_path_or_say(f, FileUse::Replace, 0);
		if (!p) return false;
		*command = std::string(ex ? "book export " : "book extract ") + *p;
		return true;
	}
	case cmd::LearnImport: {
		const std::wstring f = pick_open(texts.c_str(), t("menu.learn.import"));
		if (f.empty()) return false;
		auto p = engine_path_or_say(f, FileUse::Read, 0);
		if (!p) return false;
		*command = "book import " + *p;
		*warning = t("warn.import") + L"\n" + t("warn.book_replaced");
		return true;
	}
	case cmd::BookNewEmpty: {
		std::vector<FormField> f(2);
		f[0] = number_field(t("booknew.level"), level_, 1, 60);
		f[1] = number_field(t("booknew.depth"), 36, 1, 60);
		if (!run_form(hwnd_, t("menu.book.new"), f)) return false;
		*command = "book new " + narrow(f[0].text) + " " + narrow(f[1].text);
		*warning = t("warn.new") + L"\n" + t("warn.book_replaced");
		return true;
	}
	case cmd::BookSave: *command = "book save"; return true;
	case cmd::BookSaveAs: {
		const std::wstring f = pick_save(books.c_str(), L"dat", L"book-copy.dat", t("menu.book.save_as"));
		if (f.empty()) return false;
		if (same_file(f, book_file())) {
			*command = "book save";
			return true;
		}
		auto p = engine_path_or_say(f, FileUse::Write, 0);
		if (!p) return false;
		*command = "book save " + *p;
		return true;
	}
	case cmd::BookDepth: {
		const auto n = ask_number(hwnd_, t("menu.book.depth"), t("booknew.depth"), 36, 1, 60, t("bookdepth.note"));
		if (!n) return false;
		*command = "book depth " + std::to_string(*n);
		return true;
	}
	case cmd::BookProblem: {
		std::vector<FormField> f(2);
		f[0] = number_field(t("base.empties"), 24, 0, 60);
		f[1] = number_field(t("bookproblem.count"), 10, 1, 100000);
		if (!run_form(hwnd_, t("menu.book.problem"), f)) return false;
		*command = "book problem " + narrow(f[0].text) + " " + narrow(f[1].text);
		return true;
	}
	case cmd::EngineCommand: {
		std::vector<FormField> f(1);
		f[0].kind = FormField::Text;
		f[0].label = t("command.label");
		f[0].text = widen(ini_.get("tools", "last_command"));
		f[0].note = t("command.note");
		if (!run_form(hwnd_, t("menu.engine.command"), f)) return false;
		std::string c = trim(narrow(f[0].text));
		if (c.empty()) return false;
		ini_.set("tools", "last_command", c);
		for (auto &ch : c)
			if (ch == '|') ch = '\n';  // several commands
		const auto e = engine_command(c);  // (typed in UTF-8)
		if (!e) {
			message(t("err.command_letters"), MB_ICONWARNING);
			return false;
		}
		*command = *e;
		return true;
	}
	default: return false;
	}
}

}  // namespace ng
