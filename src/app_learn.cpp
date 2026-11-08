// Book learning on the main engine: the commands of the menu, the progress, stopping and going on from the
// file saved on the way. The search engine keeps giving values meanwhile.
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "app.h"
#include "form.h"
#include "lang.h"
#include "paths.h"

namespace ng {

namespace {

// the files a book command saves on the way, next to the book (".dev2" etc.)
constexpr const wchar_t *kSaveSuffixes[] = {L".dev",   L".dev2",  L".dev3",  L".enh",   L".fill",  L".fill2",
                                            L".lexp",  L".play",  L".store", L".gam",   L".leaf",  L".leaf2",
                                            L".leaf3", L".leaf4", L".mrg",   L".dep"};

struct SavedFile {
	std::wstring path;
	std::filesystem::file_time_type time{};
};

// the newest of the files saved on the way (path empty when there is none)
SavedFile newest_saved(const std::wstring &book)
{
	SavedFile best;
	std::error_code ec;
	for (const wchar_t *s : kSaveSuffixes) {
		const std::wstring p = book + s;
		const auto t = std::filesystem::last_write_time(p, ec);
		if (ec) continue;
		if (best.path.empty() || t > best.time) best = {p, t};
	}
	return best;
}

std::wstring local_time(std::filesystem::file_time_type t)
{
	const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(t);
	const std::time_t tt = std::chrono::system_clock::to_time_t(sys);
	std::tm tm{};
	localtime_s(&tm, &tt);
	wchar_t b[32];
	wcsftime(b, 32, L"%m/%d %H:%M", &tm);
	return b;
}

// where book reduce counts from, in the words of the log
std::wstring reduce_place(bool initial)
{
	return lang::t(initial ? "reduce.at.initial" : "reduce.at.here");
}

}  // namespace

bool replaces_book_file(std::string_view commands)
{
	for (size_t at = 0; at <= commands.size();) {
		const size_t end = std::min(commands.find('\n', at), commands.size());
		std::string_view line = commands.substr(at, end - at);
		at = end + 1;
		// the first two words of the line, in small letters as the engine reads them
		std::string w[2];
		for (std::string &word : w) {
			const size_t b = line.find_first_not_of(" \t\r");
			if (b == std::string_view::npos) break;
			const size_t e = std::min(line.find_first_of(" \t\r", b), line.size());
			word = lower(std::string(line.substr(b, e - b)));
			line.remove_prefix(e);
		}
		if (((w[0] == "book" || w[0] == "b") && w[1] == "save") || w[0] == "quit" || w[0] == "q" || w[0] == "exit" || w[0] == "eof")
			return true;
	}
	return false;
}

std::wstring App::book_file() const
{
	const char *sec = engine_section(kind_);
	const std::string bk = ini_.get(sec, "book");
	const std::wstring dir = engine_dir();
	return bk.empty() ? dir + L"\\data\\book.dat" : absolute(dir, bk);
}

std::wstring App::saved_base() const
{
	return eng_[0].book_arg.empty() ? book_file() : eng_[0].book_arg;
}

void App::learn_command(int id)
{
	if (refuse_while_book_busy()) return;
	if (!eng_[0].alive) {
		set_status(lang::t("learn.no_engine"));
		return;
	}
	const std::string xy = " " + std::to_string(loss_x_) + " " + std::to_string(loss_y_);
	std::string c;
	std::wstring warning;
	switch (id) {
	case cmd::LearnDeviate: c = "book deviate" + xy; break;
	case cmd::LearnDeviate2: c = "book deviate2" + xy; break;
	case cmd::LearnDeviate3: c = "book deviate3" + xy; break;
	case cmd::LearnEnhance: c = "book enhance" + xy; break;
	case cmd::LearnLeafExpand: c = "book leaf expand" + xy; break;
	case cmd::LearnFill: c = "book fill 2" + xy; break;
	case cmd::LearnFill2: c = "book fill2 2" + xy; break;
	case cmd::LearnPlay: c = "book play"; break;
	case cmd::LearnStore: c = "book store"; break;
	case cmd::LearnNegamax: c = "book negamax"; break;
	case cmd::LearnFix: c = "book fix"; break;
	case cmd::LearnReduce: reduce_command(); return;
	case cmd::LearnAdd:
	case cmd::LearnLearn:
	case cmd::LearnCheck: {
		const std::wstring f = pick_open(L"*.txt;*.ggf;*.sgf;*.pgn;*.wtb;*.edx\0*.txt;*.ggf;*.sgf;*.pgn;*.wtb;*.edx\0*.*\0*.*\0");
		if (f.empty()) return;
		const auto p = engine_path_or_say(f, FileUse::Read, 0);  // the engine reads the name up to a space
		if (!p) return;
		c = std::string(id == cmd::LearnAdd ? "book add " : id == cmd::LearnLearn ? "book learn " : "book check ") + *p;
		break;
	}
	default:
		if (!book_job(id, &c, &warning)) return;
		break;
	}
	std::wstring ask = lang::f("learn.ask", {engine_text(c), book_file()});
	if (!warning.empty()) ask += L"\n\n" + warning;
	if (message(ask, MB_OKCANCEL | (warning.empty() ? MB_ICONQUESTION : MB_ICONWARNING)) != IDOK)
		return;
	start_learning(c);
}

// the engine plays the game up to the place shown (the commands start from there; book store keeps the game)
void App::start_learning(const std::string &command)
{
	start_learning_at(command, game_.start(), moves_to_cursor());
}

// the commands (one or more lines) from the position after `moves` from `start`
void App::start_learning_at(const std::string &command, const Position &start, const std::string &moves, bool keeps_book)
{
	// (set for each command: a command that ended another way than by its end marker leaves nothing for the next one)
	learn_keeps_book_ = keeps_book;
	// the analyses of the main engine stop here (the engine also stops them before a command changing the book)
	std::string out = stop_main_analyses() + line_commands(start, moves);
	eng_[0].board.reset();
	out += command + "\n";
	learn_end_id_ = new_id();
	out += "echo M" + std::to_string(learn_end_id_) + "\n";  // answered when the command has ended
	learning_ = true;
	// a command that may replace the book file: the file is closed before the engine reads it (a handle on the file, even
	// one that shares it all, makes the replacing fail), and it is not looked up again until the command has ended
	learn_replaces_book_ = replaces_book_file(command);
	if (learn_replaces_book_) book_peek_.stop();
	if (!keeps_book) eng_[0].book_changed = true;  // (it saves the book as it ends: closing it waits for that)
	learn_cmd_ = command;
	for (auto &ch : learn_cmd_)
		if (ch == '\n') ch = '/';  // several commands on one line
	learn_progress_.clear();
	learn_t0_ = std::chrono::steady_clock::now();
	if (!learn_keeps_book_) {  // (a command that changes the book: the learning to go on with)
		std::string one_line = narrow(engine_text(command));  // a line of config.ini (UTF-8): the lines joined by '|'
		for (auto &ch : one_line)
			if (ch == '\n') ch = '|';
		ini_.set("learn", "last_command", one_line);
		ini_.set("learn", "last_start", start.to_board_string());
		ini_.set("learn", "last_moves", moves);
	}
	log(lang::f(learn_keeps_book_ ? "devexp.started" : "learn.started", {engine_text(command)}));
	send(0, out);
	update_menu();
	invalidate();
}

void App::learning_line(const std::string &line)
{
	// the result of book deviate ... export: "Book deviate2 2 4 export ends...finished: 7 lines written (lines.txt)"
	if (line.starts_with("Book deviate") && line.find(" export ") != std::string::npos && line.find("finished:") != std::string::npos) {
		long long n = 0;
		const size_t at = line.find("finished:");
		learn_progress_.clear();
		if (line.find("not in the book", at) != std::string::npos) log(lang::f("devexp.notinbook", {devexp_file_}));
		else if (sscanf_s(line.c_str() + at, "finished: %lld lines written", &n) == 1)
			log(lang::f("devexp.written", {lang::number(n), devexp_file_}));
		else log(engine_text(line));
		return;
	}
	if (line.starts_with("ERROR: book deviate") && line.find(" export") != std::string::npos) {
		log(lang::f("devexp.failed", {devexp_file_}) + L"（" + engine_text(line) + L"）");
		return;
	}
	// the result of book reduce: "Book reduce 2 4: 19 positions kept, 25184 removed"
	{
		int x = 0, y = 0;
		long long kept = 0, removed = 0;
		if (line.starts_with("Book reduce ") &&
		    sscanf_s(line.c_str(), "Book reduce %d %d: %lld positions kept, %lld removed", &x, &y, &kept, &removed) == 4) {
			learn_progress_.clear();
			log(lang::f("reduce.done", {lang::number(kept), lang::number(removed), std::to_wstring(x), std::to_wstring(y)}));
			return;
		}
	}
	// the progress ("Book deviate2...11/26 done: ...", "Book deviate 34 todo", one or more lines a round) is shown
	// under the board; only the lines with a result ("...finished: ...", "Fixing book...0 done", "...1 positions
	// added", warnings and errors; not "...402270 (0 error found)", which is progress) go to the log as well
	if (line.starts_with("Book ") || line.find("...") != std::string::npos) {
		learn_progress_ = engine_text(line);
		const bool result = line.find("finished") != std::string::npos || line.ends_with("done") ||
		                    line.find("done (") != std::string::npos || line.find("added") != std::string::npos ||
		                    line.starts_with("WARNING") || line.find("ERROR") != std::string::npos;
		if (result) log(engine_text(line));
		else invalidate();
		return;
	}
	log(lang::t("engine.main") + L": " + engine_text(line));
}

void App::learning_ended()
{
	const auto s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - learn_t0_).count();
	learning_ = false;
	learn_end_id_ = 0;
	log(lang::f(learn_keeps_book_ ? "devexp.ended" : "learn.ended", {engine_text(learn_cmd_), std::to_wstring(s / 60), std::to_wstring(s % 60)}));
	finish_moves(0);
	learn_progress_.clear();
	if (!learn_keeps_book_) {  // the book has changed: its analyses again
		cache_.clear_book();
		missing_ = MissingList{};
	}
	learn_keeps_book_ = false;
	eng_[0].board.reset();
	update_menu();
	if (learn_options_waiting_) {  // the learning settings changed while it learned (before the next job of the queue)
		learn_options_waiting_ = false;
		send_learn_options();
		log(lang::t("learnopt.sent"));
	}
	if (queue_running_) {
		next_in_queue();  // the next job of the queue (or its end)
		if (learning_) return;
	}
	request_analysis();
	invalidate();
	schedule_script();
}

void App::stop_learning()
{
	if (!learning_) return;
	const SavedFile saved = newest_saved(saved_base());
	const std::wstring when = saved.path.empty() ? lang::t("learn.never_saved")
	                                             : lang::f("learn.saved_at", {local_time(saved.time), saved.path});
	if (message(lang::f("learn.ask_stop", {when}), MB_OKCANCEL | MB_ICONWARNING) != IDOK)
		return;
	// a book command takes no input to stop: the engine itself is ended, then started again
	eng_[0].proc.close(0);
	eng_[0].alive = false;
	learning_ = false;
	queue_running_ = false;  // the jobs left stay in the queue
	learn_end_id_ = 0;
	learn_progress_.clear();
	forget_main_requests();
	cache_.clear_book();
	log(lang::t("learn.stopped"));
	finish_moves(0);
	start_engines();
	update_menu();
	position_changed();
	schedule_script();
}

void App::continue_learning()
{
	if (learning_ || !eng_[0].alive) return;
	const SavedFile saved = newest_saved(saved_base());
	std::error_code ec;
	const auto book_time = std::filesystem::last_write_time(book_file(), ec);
	if (saved.path.empty() || (!ec && saved.time <= book_time)) {
		message(lang::t("learn.nothing_newer"), MB_ICONINFORMATION);
		return;
	}
	std::string kept = ini_.get("learn", "last_command");
	for (auto &ch : kept)
		if (ch == '|') ch = '\n';
	const auto last = engine_command(kept);
	if (!last) {
		message(lang::t("err.command_letters"), MB_ICONWARNING);
		return;
	}
	if (message(lang::f("learn.ask_continue", {saved.path, local_time(saved.time), widen(kept)}), MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
		return;
	const auto path = engine_path_or_say(saved.path, FileUse::Read, 0);  // the engine reads the name up to a space
	if (!path) return;
	send(0, "book load " + *path + "\n");
	cache_.clear_book();
	if (last->empty()) {
		request_analysis();
		return;
	}
	// the same command from the same place
	if (auto start = parse_board_text(ini_.get("learn", "last_start"))) {
		const ParsedMoves pm = parse_moves(ini_.get("learn", "last_moves"));
		Game g;
		replay(g, *start, pm.moves);
		game_ = g;
		game_.go_to(game_.size());
		orient_ = 0;  // the game as it was learned from
	}
	start_learning(*last);
}

// book reduce: the window of the menu
void App::reduce_command()
{
	using lang::t;
	std::vector<FormField> f(3);
	f[0] = number_field(t("reduce.x"), std::clamp(loss_x_, 0, 129), 0, 129, t("reduce.x.note"));
	f[1] = number_field(t("reduce.y"), std::clamp(loss_y_, 0, 254), 0, 254, t("reduce.y.note"));
	f[2] = choice_field(t("reduce.from"), {t("reduce.from.initial"), t("reduce.from.here")}, 0, t("reduce.from.note"));
	if (!run_form(hwnd_, t("reduce.window"), f)) return;
	reduce_book(std::stoi(f[0].text), std::stoi(f[1].text), f[2].choice == 0, true);
}

bool App::reduce_book(int x, int y, bool initial, bool ask)
{
	if (learning_) {
		if (ask) message(lang::t("learn.busy"), MB_ICONINFORMATION);
		return false;
	}
	if (!eng_[0].alive) {
		set_status(lang::t("learn.no_engine"));
		return false;
	}
	if (reduce_.state != ReduceJob::None || x < 0 || x > 129 || y < 0 || y > 254) return false;
	reduce_ = ReduceJob{};
	reduce_.x = x;
	reduce_.y = y;
	reduce_.initial = initial;
	reduce_.ask = ask;
	reduce_.start = initial ? Position{} : game_.start();
	reduce_.moves = initial ? std::string() : moves_to_cursor();
	const Position p = initial ? Position{} : game_.now();
	std::string out;
	reduce_.id = ask_main(out, p, Request::ReduceCount, "book reducecount", "x=" + std::to_string(x) + " y=" + std::to_string(y));
	reduce_.state = ReduceJob::Counting;
	log(lang::f("reduce.counting", {reduce_place(initial), std::to_wstring(x), std::to_wstring(y)}));
	send(0, out);
	update_menu();
	return true;
}

void App::reduce_answer(const Fields &f, bool done)
{
	reduce_.id = 0;
	if (!done || f.num("error").value_or(0)) reduce_.state = done && reduce_.damaged ? ReduceJob::Damaged : ReduceJob::Stopped;
	else if (f.num("notinbook").value_or(0)) reduce_.state = ReduceJob::NotInBook;
	else if (f.num("deep").value_or(0)) reduce_.state = ReduceJob::Deep;
	else reduce_.state = ReduceJob::Counted;
	reduce_.kept = f.num("kept").value_or(0);
	reduce_.removed = f.num("removed").value_or(0);
	reduce_.positions = f.num("positions").value_or(0);
	PostMessageW(hwnd_, WM_REDUCE, 0, 0);  // (a message box is not shown while the lines of the engine are read)
}

void App::reduce_counted()
{
	using lang::t;
	const ReduceJob r = reduce_;
	if (r.state == ReduceJob::None || r.state == ReduceJob::Counting) return;
	reduce_.state = ReduceJob::None;
	update_menu();
	auto say = [&](const std::wstring &text) {
		log(text);
		if (r.ask) message(text, MB_ICONINFORMATION);
		schedule_script();
	};
	const std::wstring from = reduce_place(r.initial);
	switch (r.state) {
	case ReduceJob::Unknown: say(t("reduce.unknown")); return;
	case ReduceJob::NotInBook: say(lang::f("reduce.notinbook", {from})); return;
	case ReduceJob::Deep: say(lang::f("reduce.deep", {from})); return;
	case ReduceJob::Damaged: say(lang::f("reduce.damaged", {from})); return;
	case ReduceJob::Stopped: log(t("reduce.stopped")); schedule_script(); return;
	default: break;
	}
	const std::wstring xs = std::to_wstring(r.x), ys = std::to_wstring(r.y);
	if (r.removed <= 0) {
		say(lang::f("reduce.nothing", {from, xs, ys, lang::number(r.positions)}));
		return;
	}
	if (r.ask) {
		const int epoch = reduce_epoch_;
		const std::wstring text = lang::f("reduce.ask", {from, xs, ys, lang::number(r.kept), lang::number(r.removed),
		                                                 lang::number(r.positions), book_file()}) +
		                          L"\n\n" + t("warn.book_replaced");
		if (message(text, MB_OKCANCEL | MB_ICONWARNING) != IDOK) return;
		if (epoch != reduce_epoch_) {  // the engine closed (another book, engine) while it was asked: the count is old
			log(t("reduce.stopped"));
			return;
		}
	}
	if (learning_ || !eng_[0].alive) {  // (started meanwhile, or the engine has gone)
		say(lang::f("reduce.not_run", {t(!eng_[0].alive ? "learn.no_engine" : "learn.busy")}));
		return;
	}
	start_learning_at("book reduce " + std::to_string(r.x) + " " + std::to_string(r.y), r.start, r.moves);
}

// Every missing line of the book tab becomes a job: play up to its position, book store, then the command of
// the stage base with X and Y.
void App::queue_missing()
{
	if (!missing_here()) {
		set_status(lang::t("status.missing_none"));
		return;
	}
	const std::string head = moves_to_cursor();
	const std::string stage = "book " + base_ + " " + std::to_string(loss_x_) + " " + std::to_string(loss_y_);
	const std::string command = "book store\n" + stage;
	for (const auto &ml : missing_.lines) {
		LearnJob j{game_.start(), head, command};
		for (int mv : ml.moves) j.moves += square_name(mv);
		learn_queue_.push_back(std::move(j));
	}
	log(lang::f("queue.added", {std::to_wstring(missing_.lines.size()), std::to_wstring(learn_queue_.size()), widen(stage)}));
	invalidate();
}

void App::next_in_queue()
{
	if (!queue_running_ || learning_) return;
	if (learn_queue_.empty()) {
		queue_running_ = false;
		log(lang::t("queue.done"));
		return;
	}
	// (the commands could not be sent: it would be learning without an end, the job gone; the jobs stay)
	if (!eng_[0].alive) {
		queue_running_ = false;
		log(lang::t("learn.no_engine"));
		return;
	}
	LearnJob j = std::move(learn_queue_.front());
	learn_queue_.pop_front();
	start_learning_at(j.command, j.start, j.moves);
}

}  // namespace ng
