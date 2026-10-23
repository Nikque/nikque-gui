// Book learning on the main engine: the commands of the menu, the progress, stopping and going on from the
// file saved on the way. The search engine keeps giving values meanwhile.
#include <windows.h>

#include <commdlg.h>

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

std::wstring pick_file(HWND owner)
{
	wchar_t file[MAX_PATH * 4] = L"";
	OPENFILENAMEW of{sizeof of};
	of.hwndOwner = owner;
	of.lpstrFilter = L"*.txt;*.ggf;*.sgf;*.pgn;*.wtb;*.edx\0*.txt;*.ggf;*.sgf;*.pgn;*.wtb;*.edx\0*.*\0*.*\0";
	of.lpstrFile = file;
	of.nMaxFile = static_cast<DWORD>(std::size(file));
	of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	return GetOpenFileNameW(&of) ? std::wstring(file) : std::wstring();
}

}  // namespace

std::wstring App::book_file() const
{
	const char *sec = engine_section(kind_);
	const std::string bk = ini_.get(sec, "book");
	const std::wstring dir = engine_dir();
	if (bk.empty()) return dir + L"\\data\\book.dat";
	const std::wstring w = widen(bk);
	if (w.size() >= 2 && (w[1] == L':' || (w[0] == L'\\' && w[1] == L'\\'))) return w;
	return dir + L"\\" + w;
}

std::wstring App::saved_base() const
{
	return eng_[0].book_arg.empty() ? book_file() : eng_[0].book_arg;
}

void App::learn_command(int id)
{
	if (refuse_while_learning()) return;
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
		const std::wstring f = pick_file(hwnd_);
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
void App::start_learning_at(const std::string &command, const Position &start, const std::string &moves)
{
	// the analyses of the main engine stop here (the engine also stops them before a command changing the book)
	std::string out = stop_main_analyses() + line_commands(start, moves);
	eng_[0].board.reset();
	out += command + "\n";
	learn_end_id_ = new_id();
	out += "echo M" + std::to_string(learn_end_id_) + "\n";  // answered when the command has ended
	learning_ = true;
	learn_cmd_ = command;
	for (auto &ch : learn_cmd_)
		if (ch == '\n') ch = '/';  // several commands on one line
	learn_progress_.clear();
	learn_t0_ = std::chrono::steady_clock::now();
	std::string one_line = narrow(engine_text(command));  // a line of config.ini (UTF-8): the lines joined by '|'
	for (auto &ch : one_line)
		if (ch == '\n') ch = '|';
	ini_.set("learn", "last_command", one_line);
	ini_.set("learn", "last_start", start.to_board_string());
	ini_.set("learn", "last_moves", moves);
	log(lang::f("learn.started", {engine_text(command)}));
	send(0, out);
	update_menu();
	invalidate();
}

void App::learning_line(const std::string &line)
{
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
	log(lang::f("learn.ended", {engine_text(learn_cmd_), std::to_wstring(s / 60), std::to_wstring(s % 60)}));
	finish_moves(0);
	learn_progress_.clear();
	// the book has changed: its analyses again
	cache_.clear_book();
	missing_ = MissingList{};
	eng_[0].board.reset();
	update_menu();
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

}  // namespace ng

namespace ng {

// book reduce: the window of the menu
void App::reduce_command()
{
	using lang::t;
	std::vector<FormField> f(3);
	f[0].kind = FormField::Number;
	f[0].label = t("reduce.x");
	f[0].text = std::to_wstring(std::clamp(loss_x_, 0, 129));
	f[0].lo = 0;
	f[0].hi = 129;
	f[0].note = t("reduce.x.note");
	f[1].kind = FormField::Number;
	f[1].label = t("reduce.y");
	f[1].text = std::to_wstring(std::clamp(loss_y_, 0, 254));
	f[1].lo = 0;
	f[1].hi = 254;
	f[1].note = t("reduce.y.note");
	f[2].kind = FormField::Choice;
	f[2].label = t("reduce.from");
	f[2].choices = {t("reduce.from.initial"), t("reduce.from.here")};
	f[2].choice = 0;
	f[2].note = t("reduce.from.note");
	if (!run_form(hwnd_, t("reduce.window"), f, t("form.ok"), t("form.cancel"), t("form.bad_number"))) return;
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
	board_to(0, out, p);
	reduce_.id = new_id();
	out += "book reducecount id=" + std::to_string(reduce_.id) + " x=" + std::to_string(x) + " y=" + std::to_string(y) + "\n";
	requests_[reduce_.id] = {Request::ReduceCount, key_of(p)};
	reduce_.state = ReduceJob::Counting;
	log(lang::f("reduce.counting", {lang::t(initial ? "reduce.at.initial" : "reduce.at.here"), std::to_wstring(x), std::to_wstring(y)}));
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
	const std::wstring from = t(r.initial ? "reduce.at.initial" : "reduce.at.here");
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
	if (learning_ || !eng_[0].alive) {
		schedule_script();
		return;
	}
	start_learning_at("book reduce " + std::to_string(r.x) + " " + std::to_string(r.y), r.start, r.moves);
}

// Every missing line of the book tab becomes a job: play up to its position, book store, then the command of
// the stage base with X and Y.
void App::queue_missing()
{
	if (missing_.lines.empty()) {
		set_status(lang::t("status.missing_none"));
		return;
	}
	const std::string head = moves_to_cursor();
	const std::string command = "book store\nbook " + base_ + " " + std::to_string(loss_x_) + " " + std::to_string(loss_y_);
	for (const auto &ml : missing_.lines) {
		LearnJob j;
		j.start = game_.start();
		j.moves = head;
		for (int mv : ml.moves) j.moves += square_name(mv);
		j.command = command;
		learn_queue_.push_back(std::move(j));
	}
	log(lang::f("queue.added", {std::to_wstring(missing_.lines.size()), std::to_wstring(learn_queue_.size()),
	                            widen(command.substr(command.find('\n') + 1))}));
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
	LearnJob j = std::move(learn_queue_.front());
	learn_queue_.pop_front();
	start_learning_at(j.command, j.start, j.moves);
}

}  // namespace ng
