// The windows of settings: the range of the analyses (stages, umigame numbers, level, threads) and the
// settings of book learning, sent to the main engine as commands.
#include <windows.h>

#include <algorithm>
#include <cwchar>

#include "app.h"
#include "form.h"
#include "lang.h"
#include "paths.h"

namespace ng {

namespace {

// "x/y" with x 0..129 and y 0..254
bool stage_pair(const std::wstring &s, std::string *out)
{
	int x, y;
	wchar_t extra;
	if (swscanf_s(s.c_str(), L"%d/%d%lc", &x, &y, &extra, 1) != 2) return false;
	if (x < 0 || x > 129 || y < 0 || y > 254) return false;
	*out = std::to_string(x) + "/" + std::to_string(y);
	return true;
}

std::vector<std::wstring> split_stages(const std::string &s)
{
	std::vector<std::wstring> out;
	size_t i = 0;
	while (i <= s.size()) {
		size_t e = s.find(',', i);
		if (e == std::string::npos) e = s.size();
		out.push_back(widen(s.substr(i, e - i)));
		i = e + 1;
	}
	return out;
}

// the settings of book learning: config.ini [learn_options] key, the label and the hint
struct LearnOption {
	const char *key;
	const char *label;
	const char *note;
};
constexpr LearnOption kLearnOptions[] = {
	{"book-expand-tasks", "learnopt.expand", "learnopt.expand.note"},
	{"book-store-tasks", "learnopt.store", "learnopt.store.note"},
	{"book-save-interval", "learnopt.interval", "learnopt.interval.note"},
	{"book-deviate-save-rounds", "learnopt.rounds", "learnopt.rounds.note"},
	{"book-seed", "learnopt.seed", "learnopt.seed.note"},
	{"book-reproducible", "learnopt.repro", "learnopt.repro.note"},
	{"book-leaf-recalculate-rounds", "learnopt.leaf_rounds", "learnopt.leaf_rounds.note"},
	{"book-merge-auto-save", "learnopt.merge_save", "learnopt.onoff.note"},
	{"book-store-auto-save", "learnopt.store_save", "learnopt.onoff.note"},
};

// a value is a number, "auto", "on" or "off" (empty: the engine's own setting)
bool learn_value_ok(const std::string &v)
{
	if (v.empty() || v == "auto" || v == "on" || v == "off") return true;
	return v.find_first_not_of("0123456789") == std::string::npos && v.size() <= 9;
}

// the settings of book learning that config.ini gives a value (sent to the engine): a bit for each, in the order above
unsigned learn_options_with_value(const Ini &ini)
{
	unsigned bits = 0;
	for (size_t k = 0; k < std::size(kLearnOptions); ++k) {
		const std::string v = ini.get("learn_options", kLearnOptions[k].key);
		if (!v.empty() && learn_value_ok(v)) bits |= 1u << k;
	}
	return bits;
}

}  // namespace

void App::open_range_settings()
{
	using lang::t;
	std::vector<FormField> f;
	auto number = [&](const char *label, int v, int lo, int hi, const char *note = nullptr) {
		f.push_back(number_field(t(label), v, lo, hi, note ? t(note) : std::wstring()));
	};
	auto heading = [&](const char *label) {
		FormField x;
		x.kind = FormField::Heading;
		x.label = t(label);
		f.push_back(std::move(x));
	};
	heading("range.h.loss");
	number("range.x.long", loss_x_, 0, 64, "range.x.note");
	number("range.y.long", loss_y_, 0, 128);
	heading("range.h.stage");
	{
		FormField x;
		x.kind = FormField::Choice;
		x.label = t("range.base");
		x.choices = {L"book deviate", L"book deviate2", L"book deviate3"};
		x.choice = base_ == "deviate2" ? 1 : base_ == "deviate3" ? 2 : 0;
		f.push_back(std::move(x));
	}
	{
		FormField x;
		x.kind = FormField::Check;
		x.label = t("range.precision");
		x.checked = precision_;
		x.note = t("range.precision.note");
		f.push_back(std::move(x));
	}
	const auto pairs = split_stages(stages_);
	const size_t first_stage = f.size();
	for (int k = 0; k < 5; ++k) {
		FormField x;
		x.kind = FormField::Text;
		x.label = lang::f("range.stage", {std::to_wstring(k + 3)});
		x.text = k < static_cast<int>(pairs.size()) ? pairs[k] : L"";
		if (k == 0) x.note = t("range.stage.note");
		f.push_back(std::move(x));
	}
	heading("range.h.umi");
	const size_t umi_at = f.size();
	{
		FormField x;
		x.kind = FormField::Choice;
		x.label = t("range.umi");
		x.choices = {t("menu.analysis.umi.best"), t("menu.analysis.umi.loss"), t("range.umi.range"), t("menu.analysis.umi.off")};
		x.choice = umi_ == UmiMode::Best ? 0 : umi_ == UmiMode::Loss ? 1 : umi_ == UmiMode::Range ? 2 : 3;
		f.push_back(std::move(x));
	}
	number("range.lo", umi_lo_, -64, 64, "range.lo.note");
	number("range.hi", umi_hi_, -64, 64);
	number("range.depth", umi_depth_, 1, 60, "range.depth.note");
	heading("range.h.search");
	{
		FormField x;
		x.kind = FormField::Slider;
		x.label = t("range.level");
		x.text = std::to_wstring(level_);
		x.lo = 0;
		x.hi = 60;
		f.push_back(std::move(x));
	}
	number("range.threads", threads_, 0, 1024, "range.threads.note");
	number("range.hash", hash_bits_, 0, 30, "range.hash.note");

	if (!run_form(hwnd_, t("range.window"), f)) return;

	size_t i = 1;  // after the heading
	loss_x_ = std::stoi(f[i++].text);
	loss_y_ = std::stoi(f[i++].text);
	++i;  // heading
	const int b = f[i++].choice;
	base_ = b == 1 ? "deviate2" : b == 2 ? "deviate3" : "deviate";
	precision_ = f[i++].checked;
	std::string st;
	bool stages_ok = true;
	for (size_t k = first_stage; k < first_stage + 5; ++k) {
		if (f[k].text.empty()) continue;
		std::string p;
		if (!stage_pair(f[k].text, &p)) stages_ok = false;
		else st += (st.empty() ? "" : ",") + p;
	}
	if (stages_ok && !st.empty()) stages_ = st;
	else log(lang::t("range.stage.bad"));
	i = umi_at;
	const int u = f[i++].choice;
	umi_ = u == 0 ? UmiMode::Best : u == 1 ? UmiMode::Loss : u == 2 ? UmiMode::Range : UmiMode::Off;
	umi_lo_ = std::stoi(f[i++].text);
	umi_hi_ = std::stoi(f[i++].text);
	if (umi_lo_ > umi_hi_) std::swap(umi_lo_, umi_hi_);
	umi_depth_ = std::stoi(f[i++].text);
	++i;  // heading
	level_ = std::stoi(f[i++].text);
	const int threads = std::stoi(f[i++].text);
	int hash = std::stoi(f[i++].text);
	if (hash > 0 && hash < 10) hash = 10;
	save_config();
	update_menu();
	if (threads != threads_ || hash != hash_bits_) {
		threads_ = threads;
		hash_bits_ = hash;
		save_config();
		restart_engines();  // the threads are given when an engine starts
		return;
	}
	request_analysis();
	invalidate();
}

// the level of the evaluation: a slider of the levels of Edax (0 to 60)
void App::choose_level()
{
	using lang::t;
	std::vector<FormField> f(1);
	f[0] = slider_field(t("range.level"), level_, 0, 60, t("level.note"));
	if (!run_form(hwnd_, t("level.window"), f)) return;
	level_ = std::stoi(f[0].text);
	save_config();
	update_menu();
	request_analysis();
	invalidate();
}

std::string App::learn_options_commands() const
{
	std::string out;
	for (const auto &o : kLearnOptions) {
		const std::string v = ini_.get("learn_options", o.key);
		if (!v.empty() && learn_value_ok(v)) out += std::string(o.key) + " " + v + "\n";
	}
	return out;
}

void App::send_learn_options()
{
	learn_options_sent_ |= learn_options_with_value(ini_);
	send(0, learn_options_commands());
}

void App::open_learn_settings()
{
	using lang::t;
	std::vector<FormField> f;
	for (const auto &o : kLearnOptions)
		f.push_back(text_field(t(o.label), widen(ini_.get("learn_options", o.key)), widen(o.key) + L"：" + t(o.note)));
	if (!run_form(hwnd_, t("learnopt.window"), f)) return;
	std::vector<std::wstring> texts;
	for (const FormField &x : f) texts.push_back(x.text);
	apply_learn_options(texts);
}

void App::apply_learn_options(const std::vector<std::wstring> &texts)
{
	using lang::t;
	bool bad = false;
	for (size_t k = 0; k < std::size(kLearnOptions) && k < texts.size(); ++k) {
		const std::string v = trim(narrow(texts[k]));
		if (!learn_value_ok(v)) {
			bad = true;
			continue;
		}
		ini_.set("learn_options", kLearnOptions[k].key, v);
	}
	if (bad) log(t("learnopt.bad"));
	save_config();
	// the engine takes them at once (between two commands); a learning in progress keeps its own, so they go when it
	// has ended (learning_ended), and an engine not running takes them when it starts (start_engines)
	if (!eng_[0].alive) {
		log(t("learnopt.at_start"));
		return;
	}
	// a setting emptied (the engine's own) cannot be sent: the engine reads no option without a value and has no word for
	// its own setting, so the value sent before stays in it until it starts again
	const unsigned emptied = learn_options_sent_ & ~learn_options_with_value(ini_);
	if (learning_) {
		learn_options_waiting_ = true;
		log(t("learnopt.after_learning"));
	} else {
		send_learn_options();
		log(t("learnopt.sent"));
	}
	if (emptied) {
		std::wstring names;
		for (size_t k = 0; k < std::size(kLearnOptions); ++k)
			if (emptied >> k & 1) names += (names.empty() ? L"" : L", ") + widen(kLearnOptions[k].key);
		log(lang::f("learnopt.emptied", {names}));
	}
}

bool App::export_ready(bool deviate)
{
	using lang::t;
	if (refuse_while_book_busy()) return false;  // (first: the engine is not running while it saves its book)
	if (!eng_[0].alive) {
		message(t("learn.no_engine"), MB_ICONINFORMATION);
		return false;
	}
	if (!deviate) return true;
	if (eng_[0].probe_id) {  // (its answer to the first "stop id=" comes after its book: whether it has the export is not known)
		message(t("devexp.loading"), MB_ICONINFORMATION);
		return false;
	}
	if (!eng_[0].exports) {
		message(t("devexp.old_engine"), MB_ICONWARNING);
		return false;
	}
	return true;
}

// The lines that book deviate, deviate2, deviate3 walks from the position shown, to a text file without expanding the
// book (book deviate ... export): the command, its numbers and the positions written, then the file.
void App::export_deviate()
{
	using lang::t;
	if (!export_ready(true)) return;
	static const char *const bases[] = {"deviate", "deviate2", "deviate3"}, *const whiches[] = {"leaf", "ends", "all"};
	std::vector<FormField> f(4);
	// (the first choice when config.ini names none of them)
	f[0] = choice_field(t("devexp.base"), {L"book deviate", L"book deviate2", L"book deviate3"},
	                    std::max(0, name_index(bases, ini_.get("devexport", "base", base_))));
	f[1] = number_field(t("devexp.x"), ini_.get_int("devexport", "x", loss_x_, 0, 129), 0, 129, t("devexp.x.note"));
	f[2] = number_field(t("devexp.y"), ini_.get_int("devexport", "y", loss_y_, 0, 254), 0, 254, t("devexp.y.note"));
	f[3] = choice_field(t("devexp.which"), {t("devexp.which.leaf"), t("devexp.which.ends"), t("devexp.which.all")},
	                    std::max(0, name_index(whiches, ini_.get("devexport", "which", "leaf"))));
	if (!run_form(hwnd_, t("devexp.window"), f)) return;
	const int x = std::stoi(f[1].text), y = std::stoi(f[2].text);
	if (f[0].choice == 0 && y > 65) {  // (the absolute error of book deviate)
		message(t("devexp.y_deviate"), MB_ICONWARNING);
		return;
	}
	const char *const base = bases[f[0].choice], *const which = whiches[f[3].choice];
	ini_.set("devexport", "base", base);
	ini_.set("devexport", "x", std::to_string(x));
	ini_.set("devexport", "y", std::to_string(y));
	ini_.set("devexport", "which", which);
	save_config();

	const std::wstring file = pick_save(file_filter("filter.text", L"*.txt").c_str(), L"txt", L"deviate-lines.txt");
	if (file.empty() || !export_ready(true)) return;  // (the engine may have gone or started to learn meanwhile)
	start_deviate_export(base, x, y, which, file);
}

bool App::start_deviate_export(const std::string &base, int x, int y, const std::string &which, const std::wstring &file)
{
	if (!eng_[0].alive || learning_ || !eng_[0].exports) return false;  // (an engine without it would expand the book)
	auto p = engine_path_or_say(file, FileUse::Write, 0);  // (a name the engine cannot open goes through the temporary folder)
	if (!p) return false;
	devexp_file_ = file;
	start_learning_at("book " + base + " " + std::to_string(x) + " " + std::to_string(y) + " export " + *p + " " + which,
	                  game_.start(), moves_to_cursor(), true);
	return true;
}

// The lines of best moves from the position shown (book bestlines): the conditions, then the file. The engine
// writes the file itself (one line a end position, the moves from the start of the game).
void App::export_best()
{
	using lang::t;
	if (!export_ready(false)) return;
	std::vector<FormField> f(4);
	f[0] = number_field(t("best.loss"), ini_.get_int("bestlines", "loss", 0, 0, 129), 0, 129, t("best.loss.note"));
	f[1] = number_field(t("best.depth"), ini_.get_int("bestlines", "depth", 60, 1, 60), 1, 60, t("best.depth.note"));
	f[2] = check_field(t("best.follow"), ini_.get("bestlines", "solved") == "follow", t("best.follow.note"));
	f[3] = choice_field(t("best.which"), {t("best.which.all"), t("best.which.incomplete")},
	                    ini_.get("bestlines", "which") == "incomplete" ? 1 : 0);
	if (!run_form(hwnd_, t("best.window"), f)) return;
	const int loss = std::stoi(f[0].text), depth = std::stoi(f[1].text);
	const std::string solved = f[2].checked ? "follow" : "stop", which = f[3].choice ? "incomplete" : "all";
	ini_.set("bestlines", "loss", std::to_string(loss));
	ini_.set("bestlines", "depth", std::to_string(depth));
	ini_.set("bestlines", "solved", solved);
	ini_.set("bestlines", "which", which);
	save_config();

	const std::wstring file = pick_save(file_filter("filter.text", L"*.txt").c_str(), L"txt", L"bestlines.txt");
	if (file.empty() || !export_ready(false)) return;  // (the engine may have gone or started to learn meanwhile)
	start_best_export(loss, depth, solved, which, file);
}

bool App::start_best_export(int loss, int depth, const std::string &solved, const std::string &which, const std::wstring &file)
{
	if (!eng_[0].alive || learning_) return false;  // (an engine that learns would read it only when the learning has ended)
	// the engine opens the file with the code page of the system
	const auto name = acp(file);
	if (!name) {
		message(lang::t("best.bad_path"), MB_ICONWARNING);
		return false;
	}
	const std::string &path = *name;
	best_file_ = file;
	// the game up to the place shown (the lines start from the start of the game), then the command
	std::string out = line_commands(game_.start(), moves_to_cursor());
	eng_[0].board = key_of(game_.now());
	const int id = new_id();
	out += "book bestlines id=" + std::to_string(id) + " loss=" + std::to_string(loss) + " depth=" +
	       std::to_string(depth) + " solved=" + solved + " which=" + which + " file=\"" + path + "\"\n";
	requests_[id] = {Request::BestExport, key_of(game_.now())};
	send(0, out);
	log(lang::f("best.started", {best_file_}));
	return true;
}

}  // namespace ng
