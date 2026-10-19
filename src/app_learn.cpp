// Book learning on the main engine: the commands of the menu, the progress, stopping and going on from the
// file saved on the way. The search engine keeps giving values meanwhile.
#include <windows.h>

#include <commdlg.h>

#include <filesystem>

#include "app.h"
#include "lang.h"

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
	const char *sec = kind_ == EngineKind::Newedax ? "newedax" : "nikque18";
	const std::string bk = ini_.get(sec, "book");
	const std::wstring dir = engine_dir();
	if (bk.empty()) return dir + L"\\data\\book.dat";
	const std::wstring w = widen(bk);
	if (w.size() >= 2 && (w[1] == L':' || (w[0] == L'\\' && w[1] == L'\\'))) return w;
	return dir + L"\\" + w;
}

void App::learn_command(int id)
{
	if (learning_) {
		MessageBoxW(hwnd_, lang::t("learn.busy").c_str(), lang::t("app.title").c_str(), MB_ICONINFORMATION);
		return;
	}
	if (!eng_[0].alive) {
		set_status(lang::t("learn.no_engine"));
		return;
	}
	const std::string xy = " " + std::to_string(loss_x_) + " " + std::to_string(loss_y_);
	std::string c;
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
	case cmd::LearnAdd:
	case cmd::LearnLearn:
	case cmd::LearnCheck: {
		const std::wstring f = pick_file(hwnd_);
		if (f.empty()) return;
		if (f.find(L' ') != std::wstring::npos) {  // the engine reads the name up to a space
			MessageBoxW(hwnd_, lang::t("learn.space_in_path").c_str(), lang::t("app.title").c_str(), MB_ICONWARNING);
			return;
		}
		c = std::string(id == cmd::LearnAdd ? "book add " : id == cmd::LearnLearn ? "book learn " : "book check ") + narrow(f);
		break;
	}
	default: return;
	}
	if (MessageBoxW(hwnd_, lang::f("learn.ask", {widen(c), book_file()}).c_str(), lang::t("app.title").c_str(),
	                MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
		return;
	start_learning(c);
}

// the engine plays the game up to the place shown (the commands start from there; book store keeps the game)
void App::start_learning(const std::string &command)
{
	std::string moves;
	for (int i = 0; i < game_.cursor(); ++i) moves += square_name(game_.moves()[i]);
	std::string out;
	// the analyses of the main engine stop here (the engine also stops them before a command changing the book)
	for (int *id : {&book_id_, &count_id_, &levels_id_, &graph_id_, &missing_id_})
		if (*id) {
			out += "stop id=" + std::to_string(*id) + "\n";
			requests_.erase(*id);
			*id = 0;
		}
	out += "setboard " + game_.start().to_board_string() + "\n";
	if (!moves.empty()) out += "play " + moves + "\n";
	eng_[0].board.reset();
	out += command + "\n";
	learn_end_id_ = next_id_++;
	out += "echo M" + std::to_string(learn_end_id_) + "\n";  // answered when the command has ended
	learning_ = true;
	learn_cmd_ = command;
	learn_progress_.clear();
	learn_t0_ = std::chrono::steady_clock::now();
	ini_.set("learn", "last_command", command);
	ini_.set("learn", "last_start", game_.start().to_board_string());
	ini_.set("learn", "last_moves", moves);
	log(lang::f("learn.started", {widen(command)}));
	send(0, out);
	update_menu();
	invalidate();
}

void App::learning_line(const std::string &line)
{
	// the progress ("Book deviate2...11/26 done: ...") is shown, the rest goes to the log
	if (line.starts_with("Book ") || line.find("...") != std::string::npos) {
		learn_progress_ = widen(line);
		if (line.find(" done:") == std::string::npos) log(widen(line));
		else invalidate();
		return;
	}
	log(lang::t("engine.main") + L": " + widen(line));
}

void App::learning_ended()
{
	const auto s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - learn_t0_).count();
	learning_ = false;
	learn_end_id_ = 0;
	log(lang::f("learn.ended", {widen(learn_cmd_), std::to_wstring(s / 60), std::to_wstring(s % 60)}));
	learn_progress_.clear();
	// the book has changed: its analyses again
	cache_.clear_book();
	missing_ = MissingList{};
	eng_[0].board.reset();
	update_menu();
	request_analysis();
	invalidate();
}

void App::stop_learning()
{
	if (!learning_) return;
	const SavedFile saved = newest_saved(book_file());
	const std::wstring when = saved.path.empty() ? lang::t("learn.never_saved")
	                                             : lang::f("learn.saved_at", {local_time(saved.time), saved.path});
	if (MessageBoxW(hwnd_, lang::f("learn.ask_stop", {when}).c_str(), lang::t("app.title").c_str(),
	                MB_OKCANCEL | MB_ICONWARNING) != IDOK)
		return;
	// a book command takes no input to stop: the engine itself is ended, then started again
	eng_[0].proc.close(0);
	eng_[0].alive = false;
	learning_ = false;
	learn_end_id_ = 0;
	learn_progress_.clear();
	for (auto it = requests_.begin(); it != requests_.end();) it = it->second.engine == 0 ? requests_.erase(it) : std::next(it);
	book_id_ = count_id_ = levels_id_ = graph_id_ = missing_id_ = 0;
	missing_ = MissingList{};
	cache_.clear_book();
	log(lang::t("learn.stopped"));
	start_engines();
	update_menu();
	position_changed();
}

void App::continue_learning()
{
	if (learning_ || !eng_[0].alive) return;
	const std::wstring book = book_file();
	const SavedFile saved = newest_saved(book);
	std::error_code ec;
	const auto book_time = std::filesystem::last_write_time(book, ec);
	if (saved.path.empty() || (!ec && saved.time <= book_time)) {
		MessageBoxW(hwnd_, lang::t("learn.nothing_newer").c_str(), lang::t("app.title").c_str(), MB_ICONINFORMATION);
		return;
	}
	const std::string last = ini_.get("learn", "last_command");
	if (MessageBoxW(hwnd_, lang::f("learn.ask_continue", {saved.path, local_time(saved.time), widen(last)}).c_str(),
	                lang::t("app.title").c_str(), MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
		return;
	if (saved.path.find(L' ') != std::wstring::npos) {  // the engine reads the name up to a space
		MessageBoxW(hwnd_, lang::t("learn.space_in_path").c_str(), lang::t("app.title").c_str(), MB_ICONWARNING);
		return;
	}
	send(0, "book load " + narrow(saved.path) + "\n");
	cache_.clear_book();
	if (last.empty()) {
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
	}
	start_learning(last);
}

}  // namespace ng
