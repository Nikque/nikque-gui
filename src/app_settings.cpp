// The windows of settings: the range of the analyses (stages, umigame numbers, level, threads) and the
// settings of book learning, sent to the main engine as commands.
#include <windows.h>

#include <commdlg.h>

#include <cwchar>

#include "app.h"
#include "form.h"
#include "lang.h"

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

}  // namespace

void App::open_range_settings()
{
	using lang::t;
	std::vector<FormField> f;
	auto number = [&](const char *label, int v, int lo, int hi, const char *note = nullptr) {
		FormField x;
		x.kind = FormField::Number;
		x.label = t(label);
		x.text = std::to_wstring(v);
		x.lo = lo;
		x.hi = hi;
		if (note) x.note = t(note);
		f.push_back(std::move(x));
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
	number("range.level", level_, 1, 60);
	number("range.threads", threads_, 0, 1024, "range.threads.note");
	number("range.hash", hash_bits_, 0, 30, "range.hash.note");

	if (!run_form(hwnd_, t("range.window"), f, t("form.ok"), t("form.cancel"), t("form.bad_number"))) return;

	size_t i = 1;  // after the heading
	loss_x_ = std::stoi(f[i++].text);
	loss_y_ = std::stoi(f[i++].text);
	++i;  // heading
	const int b = f[i++].choice;
	base_ = b == 1 ? "deviate2" : b == 2 ? "deviate3" : "deviate";
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

std::string App::learn_options_commands() const
{
	std::string out;
	for (const auto &o : kLearnOptions) {
		const std::string v = ini_.get("learn_options", o.key);
		if (!v.empty() && learn_value_ok(v)) out += std::string(o.key) + " " + v + "\n";
	}
	return out;
}

void App::open_learn_settings()
{
	using lang::t;
	std::vector<FormField> f;
	for (const auto &o : kLearnOptions) {
		FormField x;
		x.kind = FormField::Text;
		x.label = t(o.label);
		x.text = widen(ini_.get("learn_options", o.key));
		x.note = widen(o.key) + L"：" + t(o.note);
		f.push_back(std::move(x));
	}
	if (!run_form(hwnd_, t("learnopt.window"), f, t("form.ok"), t("form.cancel"), t("form.bad_number"))) return;
	bool bad = false;
	for (size_t k = 0; k < std::size(kLearnOptions); ++k) {
		std::string v = narrow(f[k].text);
		v = trim(v);
		if (!learn_value_ok(v)) {
			bad = true;
			continue;
		}
		ini_.set("learn_options", kLearnOptions[k].key, v);
	}
	if (bad) log(t("learnopt.bad"));
	save_config();
	// the engine takes them at once (between two commands; a learning in progress keeps its own)
	if (!learning_) send(0, learn_options_commands());
	log(t("learnopt.sent"));
}

// The lines of best moves from the position shown (book bestlines): the conditions, then the file. The engine
// writes the file itself (one line a end position, the moves from the start of the game).
void App::export_best()
{
	using lang::t;
	if (!eng_[0].alive || learning_) return;
	std::vector<FormField> f(4);
	f[0].kind = FormField::Number;
	f[0].label = t("best.loss");
	f[0].text = std::to_wstring(ini_.get_int("bestlines", "loss", 0, 0, 129));
	f[0].lo = 0;
	f[0].hi = 129;
	f[0].note = t("best.loss.note");
	f[1].kind = FormField::Number;
	f[1].label = t("best.depth");
	f[1].text = std::to_wstring(ini_.get_int("bestlines", "depth", 60, 1, 60));
	f[1].lo = 1;
	f[1].hi = 60;
	f[1].note = t("best.depth.note");
	f[2].kind = FormField::Check;
	f[2].label = t("best.follow");
	f[2].checked = ini_.get("bestlines", "solved") == "follow";
	f[2].note = t("best.follow.note");
	f[3].kind = FormField::Choice;
	f[3].label = t("best.which");
	f[3].choices = {t("best.which.all"), t("best.which.incomplete")};
	f[3].choice = ini_.get("bestlines", "which") == "incomplete" ? 1 : 0;
	if (!run_form(hwnd_, t("best.window"), f, t("form.ok"), t("form.cancel"), t("form.bad_number"))) return;
	const int loss = std::stoi(f[0].text), depth = std::stoi(f[1].text);
	const std::string solved = f[2].checked ? "follow" : "stop", which = f[3].choice ? "incomplete" : "all";
	ini_.set("bestlines", "loss", std::to_string(loss));
	ini_.set("bestlines", "depth", std::to_string(depth));
	ini_.set("bestlines", "solved", solved);
	ini_.set("bestlines", "which", which);
	save_config();

	wchar_t file[MAX_PATH] = L"bestlines.txt";
	OPENFILENAMEW of{sizeof of};
	of.hwndOwner = hwnd_;
	of.lpstrFilter = L"*.txt\0*.txt\0*.*\0*.*\0";
	of.lpstrFile = file;
	of.nMaxFile = MAX_PATH;
	of.lpstrDefExt = L"txt";
	of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	if (!GetSaveFileNameW(&of)) return;
	// the engine opens the file with the code page of the system
	BOOL lost = FALSE;
	const int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, file, -1, nullptr, 0, nullptr, &lost);
	std::string path(n > 0 ? n - 1 : 0, '\0');
	if (n > 0) WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, file, -1, path.data(), n, nullptr, &lost);
	if (n <= 0 || lost) {
		MessageBoxW(hwnd_, t("best.bad_path").c_str(), t("app.title").c_str(), MB_ICONWARNING);
		return;
	}
	best_file_ = file;
	// the game up to the place shown (the lines start from the start of the game), then the command
	std::string moves;
	for (int i = 0; i < game_.cursor(); ++i) moves += square_name(game_.moves()[i]);
	std::string out = "setboard " + game_.start().to_board_string() + "\n";
	if (!moves.empty()) out += "play " + moves + "\n";
	eng_[0].board = key_of(game_.now());
	const int id = new_id();
	out += "book bestlines id=" + std::to_string(id) + " loss=" + std::to_string(loss) + " depth=" +
	       std::to_string(depth) + " solved=" + solved + " which=" + which + " file=\"" + path + "\"\n";
	requests_[id] = {Request::BestExport, key_of(game_.now()), 0};
	send(0, out);
	log(lang::f("best.started", {best_file_}));
}

}  // namespace ng
