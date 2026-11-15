#include "app.h"

#include "platform.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>

#include "lang.h"
#include "measure.h"
#include "resource.h"

namespace ng {

namespace {

// 974, 12.3K, 974K, 1.2M, 3.4G
std::wstring compact(long long n)
{
	wchar_t b[32];
	if (n < 1000) return std::to_wstring(n);
	if (n < 10'000) swprintf(b, 32, L"%.1fK", n / 1e3);
	else if (n < 1'000'000) swprintf(b, 32, L"%lldK", n / 1000);
	else if (n < 10'000'000) swprintf(b, 32, L"%.1fM", n / 1e6);
	else if (n < 1'000'000'000) swprintf(b, 32, L"%lldM", n / 1'000'000);
	else swprintf(b, 32, L"%.1fG", n / 1e9);
	return b;
}

// the small number of a move's square chosen in the view menu
std::wstring cell_info_text(const MoveEval &m, CellInfo what, bool use_book)
{
	const bool book = use_book && m.in_book;
	switch (what) {
	case CellInfo::Positions:
		if (!book) return {};
		if (m.positions >= 0) return compact(m.positions);
		return m.lines >= 0 ? L"\x2248" + compact(m.lines) : std::wstring();  // the lines of the book until counted
	case CellInfo::Level: {
		const int lv = book ? m.book_level : shown_level(m, use_book);
		return lv > 0 ? L"L" + std::to_wstring(lv) : std::wstring();
	}
	case CellInfo::Stage:
		return use_book && m.book_row && m.stage >= 0 ? L"S" + std::to_wstring(m.stage) : std::wstring();
	case CellInfo::Umi:
		if (!m.umi_known) return {};
		return m.umi_none ? L"-" : std::to_wstring(m.umi_black) + L":" + std::to_wstring(m.umi_white);
	default: return {};
	}
}

}  // namespace

int Layout::square_at(float x, float y) const
{
	const int ox = bx + frame + line, oy = by + frame + line;
	const int step = cell + line;
	if (x < ox || y < oy) return kNoMove;
	const int cx = static_cast<int>(x - ox) / step, cy = static_cast<int>(y - oy) / step;
	if (cx > 7 || cy > 7) return kNoMove;
	if (static_cast<int>(x - ox) % step >= cell || static_cast<int>(y - oy) % step >= cell) return kNoMove;
	return cy * 8 + cx;
}

// ---- the start (App::run of each system calls these around the making of its window)

void App::prepare()
{
	measure::start();
	exe_dir_ = exe_path();
	exe_dir_.resize(exe_dir_.find_last_of(L"\\/"));
	ini_path_ = path_join(exe_dir_, L"config.ini");
	temp_dir_ = temp_folder() + L"nikque-gui-" + std::to_wstring(process_id());
	make_dir(temp_dir_);

	load_config();
	// the numbers of the requests from near the end of their range (measuring build only: a test of their turn)
	if constexpr (measure::kOn) next_id_ = ini_.get_int("measure", "first_id", 0, 0, kMaxRequestId);
}

void App::started()
{
	if (!openings_.empty()) log(lang::f("log.openings", {std::to_wstring(openings_.positions() / 8)}));
	load_kept();
	start_engines();
	if (compare_) set_compare(true);
	position_changed();
	set_timer(kTimerWatch, 5000);
	// the commands to run at the start (config.ini [script] startup)
	if (const std::string st = ini_.get("script", "startup"); !st.empty())
		run_script_file(from_exe_dir(st), 0);
}

// The window closes (its button, Alt+F4, "quit" of a script). The main engine may save its book as it ends: the window
// then closes after it and answers meanwhile; closed again then, it asks whether to end the engine now (not when a
// script closes it).
void App::close_window()
{
	if (closing_) {
		if (!script_quit_) ask_cut_save();
		return;
	}
	closing_ = true;
	// (the log goes with the window: said in a box, but not to a script that ends the program, which nobody may watch)
	const bool saved = save_config();
	if (!saved && !script_quit_) message(lang::f("config.save_failed", {ini_path_}), MB_ICONWARNING);
	kill_timer(kTimerAnim);
	kill_timer(kTimerWatch);
	kill_timer(kTimerScript);
	script_.clear();
	auto stop_task = [this] {
		if (task_.joinable()) {
			task_.request_stop();
			task_.join();
		}
	};
	stop_task();
	stop_engines([this, saved, stop_task] {
		// what was started while the book was saved (the window answers meanwhile) goes before the window, and what was
		// changed is saved (a failure said once)
		stop_task();
		for (int role = 1; role < 5; ++role) eng_[role].proc.close();
		if (!save_config() && saved && !script_quit_) message(lang::f("config.save_failed", {ini_path_}), MB_ICONWARNING);
		destroy_window(hwnd_);
	});
}

// ---- config

std::wstring App::from_exe_dir(const std::string &name) const
{
	return absolute(exe_dir_, name);
}

void App::load_config()
{
	ini_.load(ini_path_);
	// a setting given by its name (in any case); another name gives `def`
	auto named = [&]<typename E, size_t N>(const char *section, const char *key, const char *const (&names)[N], E def) {
		const int k = name_index(names, lower(ini_.get(section, key, names[static_cast<int>(def)])));
		return k >= 0 ? static_cast<E>(k) : def;
	};
	design_ = named("view", "design", kDesignNames, Design::Standard);
	effects_ = named("view", "effects", kEffectsNames, Effects::On);
	top_effect_ = named("view", "top_effect", kTopEffectNames, TopEffect::Normal);
	show_eval_ = ini_.get_bool("analysis", "show", true);
	use_book_ = ini_.get_bool("analysis", "book", true);
	level_ = ini_.get_int("analysis", "level", 21, 0, 60);
	threads_ = ini_.get_int("analysis", "threads", 0, 0, 1024);
	umi_ = named("analysis", "umi", kUmiNames, UmiMode::Best);
	umi_lo_ = ini_.get_int("analysis", "umi_lo", -64, -64, 64);
	umi_hi_ = ini_.get_int("analysis", "umi_hi", 64, -64, 64);
	umi_depth_ = ini_.get_int("analysis", "umi_depth", 60, 1, 60);
	loss_x_ = ini_.get_int("analysis", "x", 2, 0, 64);
	loss_y_ = ini_.get_int("analysis", "y", 4, 0, 128);
	base_ = lower(ini_.get("analysis", "base", "deviate"));
	if (base_ != "deviate" && base_ != "deviate2" && base_ != "deviate3") base_ = "deviate";
	stages_ = ini_.get("analysis", "stages", "1/1,2/2,3/3,4/4,5/5");
	if (stages_.empty() || stages_.find_first_not_of("0123456789/,") != std::string::npos)
		stages_ = "1/1,2/2,3/3,4/4,5/5";
	count_ = ini_.get_bool("analysis", "count", true);
	backward_ = ini_.get_bool("analysis", "backward", true);
	forward_ = ini_.get_bool("analysis", "forward", false);
	if (forward_) backward_ = false;
	precision_ = ini_.get_bool("analysis", "precision", true);
	pv_on_board_ = ini_.get_bool("view", "pv", false);
	opening_marks_ = ini_.get_bool("view", "opening_marks", true);
	record_box_ = ini_.get_bool("view", "record_box", true);
	upper_moves_ = ini_.get_bool("view", "upper_moves", false);
	graph_end_ = ini_.get_bool("view", "graph_end", true);
	bookmarks_shown_ = ini_.get_bool("window", "bookmarks", true);
	anim_fps_ = ini_.get_int("view", "fps", 20, 5, 60);
	font_px_ = ini_.get_int("view", "font", 0, 0, 72);
	ini_.rename_section("nikque18", "nikque");  // (a config.ini of 0.2.0 or before)
	// the engine: newedax unless nikque is written (nikque18: a config.ini of 0.2.0 or before); newedax by default from 0.4.1
	const std::string kind = lower(ini_.get("engine", "kind", "newedax"));
	kind_ = kind == "nikque" || kind == "nikque18" ? EngineKind::Nikque18 : EngineKind::Newedax;
	search_engine_ = ini_.get_bool("engine", "search", true);
	panel_w_ = static_cast<float>(ini_.get_int("window", "panel_w", 420, 200, 4000));
	graph_ratio_ = ini_.get_int("window", "graph", 34, 10, 90) / 100.0f;
	log_open_ = ini_.get_bool("window", "log", false);
	panel_hidden_ = ini_.get_bool("window", "panel_hidden", false);
	tab_ = named("window", "tab", kTabNames, Tab::Moves);
	hash_bits_ = ini_.get_int("analysis", "hash", 0, 0, 30);
	if (hash_bits_ > 0 && hash_bits_ < 10) hash_bits_ = 10;
	move_numbers_ = ini_.get_bool("view", "numbers", false);
	cell_info_ = named("view", "cell_info", kCellInfoNames, CellInfo::None);
	list_start_ply_ = ini_.get_int("list", "start_ply", -1, -1, 60);
	com_level_ = ini_.get_int("play", "level", 21, 1, 60);
	opening_ = named("play", "opening", kOpeningNames, Opening::BookBest);
	opening_width_ = ini_.get_int("play", "width", 2, 0, 64);
	opening_until_ = ini_.get_int("play", "until", 60, 0, 60);
	clock_minutes_ = ini_.get_int("play", "clock", 0, 0, 600);
	ponder_ = ini_.get_bool("play", "ponder", false);
	keep_hash_ = ini_.get_bool("play", "keep_hash", false);
	end_keep_ = ini_.get_bool("play", "end_keep", true);
	end_list_ = ini_.get_bool("play", "end_list", false);
	compare_ = ini_.get_bool("analysis", "compare", false);
	lang_ = ini_.get("view", "language", "ja");
	if (name_index(kLanguages, lang_) < 0) lang_ = "ja";
	// listed even when empty, so that config.ini shows them
	if (ini_.section("script").empty()) ini_.set("script", "startup", "");
	keys_.load(ini_);
	// the engine folders are listed even when empty (empty: next to the exe, engines\nikque and engines\newedax)
	for (const char *sec : {"nikque", "newedax"}) {
		const auto have = ini_.section(sec);
		for (const char *k : {"dir", "exe", "eval", "book"}) {
			const bool listed = std::any_of(have.begin(), have.end(), [&](const auto &kv) { return iequals(kv.first, k); });
			if (!listed) ini_.set(sec, k, std::strcmp(k, "exe") ? "" : "auto");
		}
	}
	lang::load(exe_dir_, lang_);
	// opening names: a file next to the exe unless set
	const std::string of = ini_.get("view", "opening_file");
	openings_.load(of.empty() ? exe_dir_ + L"\\opening.ini" : from_exe_dir(of));
	show_opening_ = ini_.get_bool("view", "opening", true);
}

bool App::save_config()
{
	ini_.set("view", "design", kDesignNames[static_cast<int>(design_)]);
	ini_.set("view", "effects", kEffectsNames[static_cast<int>(effects_)]);
	ini_.set("view", "top_effect", kTopEffectNames[static_cast<int>(top_effect_)]);
	ini_.set("view", "language", lang_);
	ini_.set_int("view", "numbers", move_numbers_);
	ini_.set("view", "cell_info", kCellInfoNames[static_cast<int>(cell_info_)]);
	ini_.set_int("analysis", "hash", hash_bits_);
	ini_.set_int("analysis", "compare", compare_);
	ini_.set_int("play", "level", com_level_);
	ini_.set("play", "opening", kOpeningNames[static_cast<int>(opening_)]);
	ini_.set_int("play", "width", opening_width_);
	ini_.set_int("play", "until", opening_until_);
	ini_.set_int("play", "clock", clock_minutes_);
	ini_.set_int("play", "ponder", ponder_);
	ini_.set_int("play", "keep_hash", keep_hash_);
	ini_.set_int("play", "end_keep", end_keep_);
	ini_.set_int("play", "end_list", end_list_);
	ini_.set_int("list", "start_ply", list_start_ply_);
	ini_.set_int("view", "fps", anim_fps_);
	ini_.set_int("view", "opening", show_opening_);
	if (ini_.get("view", "opening_file").empty() && ini_.section("view").size() > 0) ini_.set("view", "opening_file", "");
	ini_.set_int("view", "font", font_px_);
	ini_.set_int("analysis", "show", show_eval_);
	ini_.set_int("analysis", "book", use_book_);
	ini_.set_int("analysis", "level", level_);
	ini_.set_int("analysis", "threads", threads_);
	ini_.set("analysis", "umi", kUmiNames[static_cast<int>(umi_)]);
	ini_.set_int("analysis", "umi_lo", umi_lo_);
	ini_.set_int("analysis", "umi_hi", umi_hi_);
	ini_.set_int("analysis", "umi_depth", umi_depth_);
	ini_.set_int("analysis", "x", loss_x_);
	ini_.set_int("analysis", "y", loss_y_);
	ini_.set("analysis", "base", base_);
	ini_.set("analysis", "stages", stages_);
	ini_.set_int("analysis", "count", count_);
	ini_.set_int("analysis", "backward", backward_);
	ini_.set_int("analysis", "forward", forward_);
	ini_.set_int("analysis", "precision", precision_);
	ini_.set_int("view", "pv", pv_on_board_);
	ini_.set_int("view", "opening_marks", opening_marks_);
	ini_.set_int("view", "record_box", record_box_);
	ini_.set_int("view", "graph_end", graph_end_);
	ini_.set_int("view", "upper_moves", upper_moves_);
	ini_.set_int("window", "bookmarks", bookmarks_shown_);
	ini_.set("engine", "kind", engine_section(kind_));
	ini_.set_int("engine", "search", search_engine_);
	int x = 0, y = 0, w = 0, h = 0;
	if (bool max = false; window_placement(hwnd_, &x, &y, &w, &h, &max)) {
		ini_.set_int("window", "x", x);
		ini_.set_int("window", "y", y);
		ini_.set_int("window", "w", w);
		ini_.set_int("window", "h", h);
		ini_.set_int("window", "max", max);
	}
	ini_.set_int("window", "panel_w", static_cast<int>(panel_w_));
	ini_.set_int("window", "graph", static_cast<int>(std::lround(graph_ratio_ * 100)));
	ini_.set_int("window", "log", log_open_);
	ini_.set_int("window", "panel_hidden", panel_hidden_);
	ini_.set("window", "tab", kTabNames[static_cast<int>(tab_)]);
	if (!ini_.dirty() || ini_.save(ini_path_)) return true;
	log(lang::f("config.save_failed", {ini_path_}));  // (config.ini stays as it was; the settings are kept to be saved)
	return false;
}

// ---- menu

void App::build_menu()
{
	using lang::t;
	auto item = [](MenuRef m, int id, const std::wstring &text, const std::wstring &key = {}) {
		std::wstring s = text;
		if (!key.empty()) s += L"\t" + key;
		menu_item(m, id, s);
	};
	auto sub = [](MenuRef parent, MenuRef m, const std::wstring &text) { menu_sub(parent, m, text); };
	auto sep = [](MenuRef m) { menu_sep(m); };
	MenuRef bar = menu_new(true);

	MenuRef game = menu_new();
	item(game, cmd::NewGame, t("menu.game.new"));
	MenuRef pm = menu_new();
	item(pm, cmd::PlayFree, t("menu.play.free"));
	item(pm, cmd::PlayHumanBlack, t("menu.play.human_black"));
	item(pm, cmd::PlayHumanWhite, t("menu.play.human_white"));
	item(pm, cmd::PlayEngines, t("menu.play.engines"));
	sub(game, pm, t("menu.play"));
	item(game, cmd::PlayEngineMove, t("menu.play.engine_move"));
	item(game, cmd::PlayTakeBack, t("menu.play.take_back"));
	item(game, cmd::PlayStop, t("menu.play.stop"));
	sep(game);
	item(game, cmd::PlayLevel, t("menu.play.level"));
	MenuRef op = menu_new();
	item(op, cmd::OpeningSearch, t("menu.opening.search"));
	item(op, cmd::OpeningBest, t("menu.opening.best"));
	item(op, cmd::OpeningWide, t("menu.opening.wide"));
	sep(op);
	item(op, cmd::OpeningSettings, t("menu.opening.settings"));
	sub(game, op, t("menu.opening"));
	item(game, cmd::PlayClock, t("menu.play.clock"));
	item(game, cmd::PlayPonder, t("menu.play.ponder"));
	item(game, cmd::PlayKeepHash, t("menu.play.keep_hash"));
	item(game, cmd::PlayEndSettings, t("menu.play.end"));
	sep(game);
	item(game, cmd::Undo, t("menu.game.undo"), keys_.describe(Action::Back) + L" / " + t("key.right_click"));
	item(game, cmd::Redo, t("menu.game.redo"), keys_.describe(Action::Forward));
	item(game, cmd::ToStart, t("menu.game.start"), keys_.describe(Action::Start));
	item(game, cmd::ToEnd, t("menu.game.end"), keys_.describe(Action::End));
	sep(game);
	item(game, cmd::KeepNow, t("menu.kept.keep"), keys_.describe(Action::Keep));
	sub(bar, game, t("menu.game"));

	MenuRef an = menu_new();
	item(an, cmd::ShowEval, t("menu.analysis.show"), keys_.describe(Action::HideEval));
	item(an, cmd::Recompute, t("menu.analysis.recompute"));
	item(an, cmd::LevelChoose, lang::f("menu.analysis.level", {std::to_wstring(level_)}));
	sep(an);
	item(an, cmd::Backward, t("menu.analysis.backward"));
	item(an, cmd::Forward, t("menu.analysis.forward"));
	item(an, cmd::BackgroundOff, t("menu.analysis.background_off"));
	sep(an);
	item(an, cmd::PrecisionOn, t("menu.analysis.precision"));
	MenuRef umi = menu_new();
	item(umi, cmd::UmiBest, t("menu.analysis.umi.best"));
	item(umi, cmd::UmiLoss, t("menu.analysis.umi.loss"));
	item(umi, cmd::UmiOff, t("menu.analysis.umi.off"));
	sub(an, umi, t("menu.analysis.umi"));
	item(an, cmd::CountOn, t("menu.analysis.count"));
	item(an, cmd::CompareOn, t("menu.analysis.compare"));
	sep(an);
	item(an, cmd::RangeSettings, t("menu.settings.range"));
	sub(bar, an, t("menu.analysis"));

	MenuRef book = menu_new();
	item(book, cmd::UseBook, t("menu.book.use"), keys_.describe(Action::ToggleBook));
	item(book, cmd::ReloadBook, t("menu.book.reload"));
	item(book, cmd::OpenBook, t("menu.book.open"));
	{
		MenuRef rec = menu_new();
		int shown = 0;
		for (int i = 1; i <= 8; ++i) {
			const std::string r = ini_.get("books", "recent" + std::to_string(i));
			if (r.empty()) continue;
			item(rec, cmd::RecentBook0 + i - 1, widen(r));
			++shown;
		}
		if (!shown) menu_item(rec, cmd::RecentBook0, t("menu.book.recent.none"), true);
		sub(book, rec, t("menu.book.recent"));
	}
	MenuRef base = menu_new();
	item(base, cmd::BaseDeviate, L"book deviate");
	item(base, cmd::BaseDeviate2, L"book deviate2");
	item(base, cmd::BaseDeviate3, L"book deviate3");
	sub(book, base, t("menu.book.base"));
	sep(book);
	item(book, cmd::BookInfo, t("menu.book.info"));
	item(book, cmd::TabTree, t("menu.book.tree"));
	MenuRef show = menu_new();
	item(show, cmd::BookShowInfo, t("menu.book.show_info"));
	item(show, cmd::BookShowStats, t("menu.book.show_stats"));
	item(show, cmd::BookShowPosition, t("menu.book.show_position"));
	sub(book, show, t("menu.book.show"));
	item(book, cmd::ExportMissing, t("menu.book.export_missing"));
	item(book, cmd::ExportBest, t("menu.book.export_best"));
	item(book, cmd::ExportDeviate, t("menu.book.export_deviate"));
	sep(book);
	MenuRef keep = menu_new();
	item(keep, cmd::BookSave, t("menu.book.save"));
	item(keep, cmd::BookSaveAs, t("menu.book.save_as"));
	item(keep, cmd::BookNewEmpty, t("menu.book.new"));
	item(keep, cmd::BookDepth, t("menu.book.depth"));
	sep(keep);
	item(keep, cmd::LearnMerge, t("menu.learn.merge"));
	item(keep, cmd::LearnSubtree, t("menu.learn.subtree"));
	item(keep, cmd::LearnPrune, t("menu.learn.prune"));
	item(keep, cmd::LearnReduce, t("menu.learn.reduce"));
	sep(keep);
	item(keep, cmd::LearnExport, t("menu.learn.export"));
	item(keep, cmd::LearnImport, t("menu.learn.import"));
	item(keep, cmd::LearnExtract, t("menu.learn.extract"));
	item(keep, cmd::BookProblem, t("menu.book.problem"));
	sub(book, keep, t("menu.book.keep"));
	book_menu_ = book;
	keep_menu_ = keep;
	sub(bar, book, t("menu.book"));

	MenuRef learn = menu_new();
	item(learn, cmd::LearnDeviate, t("menu.learn.deviate"));
	item(learn, cmd::LearnDeviate2, t("menu.learn.deviate2"));
	item(learn, cmd::LearnDeviate3, t("menu.learn.deviate3"));
	item(learn, cmd::LearnEnhance, t("menu.learn.enhance"));
	item(learn, cmd::LearnLeafExpand, t("menu.learn.leaf_expand"));
	item(learn, cmd::LearnFill, t("menu.learn.fill"));
	item(learn, cmd::LearnFill2, t("menu.learn.fill2"));
	item(learn, cmd::LearnPlay, t("menu.learn.play"));
	sep(learn);
	item(learn, cmd::LearnStore, t("menu.learn.store"));
	item(learn, cmd::LearnAdd, t("menu.learn.add"));
	item(learn, cmd::LearnLearn, t("menu.learn.learn"));
	item(learn, cmd::LearnCheck, t("menu.learn.check"));
	sep(learn);
	item(learn, cmd::LearnNegamax, t("menu.learn.negamax"));
	item(learn, cmd::LearnFix, t("menu.learn.fix"));
	item(learn, cmd::LearnCorrect, t("menu.learn.correct"));
	MenuRef leaf = menu_new();
	item(leaf, cmd::LearnLeafRecalc1, t("menu.learn.leaf1"));
	item(leaf, cmd::LearnLeafRecalc2, t("menu.learn.leaf2"));
	item(leaf, cmd::LearnLeafRecalc3, t("menu.learn.leaf3"));
	item(leaf, cmd::LearnLeafRecalc4, t("menu.learn.leaf4"));
	sub(learn, leaf, t("menu.learn.leaf"));
	learn_menu_ = learn;
	leaf_menu_ = leaf;
	sep(learn);
	item(learn, cmd::LearnStop, t("menu.learn.stop"));
	item(learn, cmd::LearnContinue, t("menu.learn.continue"));
	sep(learn);
	item(learn, cmd::LearnSettings, t("menu.learn.settings"));
	sep(learn);
	item(learn, cmd::QueueMissing, t("menu.queue.add"));
	item(learn, cmd::QueueStart, t("menu.queue.start"));
	item(learn, cmd::QueueClear, t("menu.queue.clear"));
	sub(bar, learn, t("menu.learn"));

	MenuRef eng = menu_new();
	item(eng, cmd::EngineNikque, t("menu.engine.nikque"));
	item(eng, cmd::EngineNewedax, t("menu.engine.newedax"));
	sep(eng);
	item(eng, cmd::RestartEngines, t("menu.engine.restart"));
	item(eng, cmd::EngineVersion, t("menu.engine.version"));
	sep(eng);
	item(eng, cmd::ToolSolve, t("menu.tool.solve"));
	item(eng, cmd::ToolBench, t("menu.tool.bench"));
	item(eng, cmd::ToolCount, t("menu.tool.count"));
	item(eng, cmd::ToolPerft, t("menu.tool.perft"));
	item(eng, cmd::ToolEstimate, t("menu.tool.estimate"));
	item(eng, cmd::ToolStop, t("menu.tool.stop"));
	sep(eng);
	item(eng, cmd::EngineCommand, t("menu.engine.command"));
	sub(bar, eng, t("menu.engine"));

	MenuRef io = menu_new();
	item(io, cmd::CopyMoves, t("menu.io.copy"), keys_.describe(Action::CopyMoves));
	item(io, cmd::PasteMoves, t("menu.io.paste"), keys_.describe(Action::PasteMoves));
	item(io, cmd::UpperMoves, t("menu.io.upper"));
	item(io, cmd::FixRecord, t("menu.io.fix"));
	MenuRef pos = menu_new();
	item(pos, cmd::CopyBoardText, t("menu.io.copy_board"));
	item(pos, cmd::CopyObf, t("menu.io.copy_obf"));
	item(pos, cmd::CopyBitDecimal, t("menu.io.copy_dec"));
	item(pos, cmd::CopyBitHex, t("menu.io.copy_hex"));
	item(pos, cmd::CopyBitNtest, t("menu.io.copy_ntest"));
	sep(pos);
	item(pos, cmd::ImportPosition, t("menu.io.import"));
	sub(io, pos, t("menu.io.position"));
	MenuRef ed = menu_new();
	item(ed, cmd::EditBoard, t("menu.edit.start"), keys_.describe(Action::EditBoard));
	item(ed, cmd::EditDone, t("menu.edit.done"), L"Enter");
	item(ed, cmd::EditCancel, t("menu.edit.cancel"), L"Esc");
	item(ed, cmd::EditSide, t("menu.edit.side"), L"T");
	item(ed, cmd::EditClear, t("menu.edit.clear"), L"Delete");
	item(ed, cmd::EditInitial, t("menu.edit.initial"), L"I");
	sub(io, ed, t("menu.edit"));
	item(io, cmd::EstimateLine, t("menu.io.estimate"));
	sep(io);
	item(io, cmd::Screenshot, t("menu.io.screenshot"), keys_.describe(Action::Screenshot));
	item(io, cmd::SaveBoardImage, t("menu.io.board_image"));
	sep(io);
	MenuRef lst = menu_new();
	item(lst, cmd::ListOpen, t("menu.list.open"));
	item(lst, cmd::ListAdd, t("menu.list.add"));
	item(lst, cmd::ListSave, t("menu.list.save"));
	item(lst, cmd::ListAddGame, t("menu.list.add_game"));
	item(lst, cmd::ListFromBook, t("menu.list.book"));
	sep(lst);
	item(lst, cmd::ListSort, t("menu.list.sort"));
	item(lst, cmd::ListUnique, t("menu.list.unique"));
	item(lst, cmd::ListTruncate, t("menu.list.truncate"));
	item(lst, cmd::ListShuffle, t("menu.list.shuffle"));
	item(lst, cmd::ListNormalize, t("menu.list.normalize"));
	item(lst, cmd::ListScatter, t("menu.list.scatter"));
	sep(lst);
	item(lst, cmd::ListPrev, t("menu.list.prev"), keys_.describe(Action::ListPrev));
	item(lst, cmd::ListNext, t("menu.list.next"), keys_.describe(Action::ListNext));
	item(lst, cmd::ListReload, t("menu.list.reload"), keys_.describe(Action::ListReload));
	item(lst, cmd::ListDelete, t("menu.list.delete"), keys_.describe(Action::ListDelete));
	item(lst, cmd::ListClear, t("menu.list.clear"));
	item(lst, cmd::ListStartPly, t("menu.list.start"));
	sub(io, lst, t("menu.list"));
	item(io, cmd::ListSearch, t("menu.list.search"));
	item(io, cmd::ListSearchClear, t("menu.list.search_clear"));
	item(io, cmd::TaskCancel, t("menu.task.cancel"));
	sep(io);
	MenuRef bs = menu_new();
	item(bs, cmd::BaseConvert, t("menu.base.convert"));
	item(bs, cmd::BaseUnique, t("menu.base.unique"));
	item(bs, cmd::BaseCompare, t("menu.base.compare"));
	item(bs, cmd::BaseCheck, t("menu.base.check"));
	item(bs, cmd::BaseCorrect, t("menu.base.correct"));
	item(bs, cmd::BaseComplete, t("menu.base.complete"));
	item(bs, cmd::BaseProblem, t("menu.base.problem"));
	item(bs, cmd::BaseTofen, t("menu.base.tofen"));
	sub(io, bs, t("menu.base"));
	sep(io);
	item(io, cmd::RunScript, t("menu.io.script"));
	item(io, cmd::StopScript, t("menu.io.script_stop"));
	sub(bar, io, t("menu.io"));

	MenuRef view = menu_new();
	item(view, cmd::DesignToggle, t("menu.view.design.toggle"), keys_.describe(Action::ToggleDesign));
	MenuRef des = menu_new();
	item(des, cmd::DesignStandard, t("menu.view.design.standard"));
	item(des, cmd::DesignPaw, t("menu.view.design.paw"));
	sub(view, des, t("menu.view.design"));
	MenuRef eff = menu_new();
	item(eff, cmd::EffectsOn, t("menu.view.effects.on"));
	item(eff, cmd::EffectsSubdued, t("menu.view.effects.subdued"));
	item(eff, cmd::EffectsOff, t("menu.view.effects.off"));
	sep(eff);
	menu_item(eff, 0, t("menu.view.top"), true);  // (a heading)
	item(eff, cmd::TopStrong, t("menu.view.top.strong"));
	item(eff, cmd::TopNormal, t("menu.view.top.normal"));
	item(eff, cmd::TopWeak, t("menu.view.top.weak"));
	sub(view, eff, t("menu.view.effects"));
	sep(view);
	MenuRef ori = menu_new();
	for (int s = 0; s < 8; ++s) item(ori, cmd::Rotate0 + s, t("orient." + std::to_string(s)));
	sep(ori);
	item(ori, cmd::RotateNext, t("menu.view.orient.next"), keys_.describe(Action::Rotate));
	sub(view, ori, t("menu.view.orient"));
	item(view, cmd::MoveNumbers, t("menu.view.numbers"));
	item(view, cmd::PvOnBoard, t("menu.view.pv"));
	item(view, cmd::OpeningMarks, t("menu.view.opening_marks"));
	MenuRef ci = menu_new();
	item(ci, cmd::CellInfoNone, t("menu.view.cell.none"));
	item(ci, cmd::CellInfoPositions, t("menu.view.cell.positions"));
	item(ci, cmd::CellInfoLevel, t("menu.view.cell.level"));
	item(ci, cmd::CellInfoStage, t("menu.view.cell.stage"));
	item(ci, cmd::CellInfoUmi, t("menu.view.cell.umi"));
	sub(view, ci, t("menu.view.cell"));
	sep(view);
	item(view, cmd::TabMoves, t("menu.view.tab.moves"));
	item(view, cmd::TabBook, t("menu.view.tab.book"));
	item(view, cmd::TabRecord, t("menu.view.tab.record"));
	item(view, cmd::TabList, t("menu.view.tab.list"));
	item(view, cmd::TabTree, t("menu.view.tab.tree"));
	item(view, cmd::RecordBoxShow, t("menu.view.record_box"));
	item(view, cmd::GraphEndShow, t("menu.view.graph_end"));
	MenuRef kp = menu_new();
	item(kp, cmd::KeepNow, t("menu.kept.keep"), keys_.describe(Action::Keep));
	item(kp, cmd::BookmarksShow, t("menu.kept.strip"));
	sep(kp);
	item(kp, cmd::KeptShow, t("menu.kept.show"));
	item(kp, cmd::KeptCompare, t("menu.kept.compare"));
	item(kp, cmd::KeptDelete, t("menu.kept.delete"), L"Delete");
	item(kp, cmd::KeptClear, t("menu.kept.clear"));
	sub(view, kp, t("menu.kept"));
	sep(view);
	item(view, cmd::TogglePanel, t("menu.view.panel"), keys_.describe(Action::TogglePanel));
	item(view, cmd::ToggleLog, t("menu.view.log"), keys_.describe(Action::ToggleLog));
	item(view, cmd::SaveLog, t("menu.view.save_log"));
	sub(bar, view, t("menu.view"));

	MenuRef set = menu_new();
	item(set, cmd::RangeSettings, t("menu.settings.range"));
	item(set, cmd::LearnSettings, t("menu.learn.settings"));
	item(set, cmd::OpenConfig, t("menu.settings.config"));
	MenuRef lg = menu_new();
	item(lg, cmd::LangJa, L"日本語");
	item(lg, cmd::LangEn, L"English");
	item(lg, cmd::LangZhHans, L"简体中文");
	item(lg, cmd::LangZhHant, L"繁體中文");
	sub(set, lg, t("menu.settings.lang"));
	sub(bar, set, t("menu.settings"));

	MenuRef help = menu_new();
	item(help, cmd::HelpKeys, t("menu.help.keys"));
	item(help, cmd::HelpAbout, t("menu.help.about"));
	sub(bar, help, t("menu.help"));

	menu_set_bar(hwnd_, bar);
	update_menu();
}

void App::update_menu()
{
	MenuRef m = menu_bar(hwnd_);
	if (!m) return;
	auto check = [&](int id, bool on) { menu_check(m, id, on); };
	auto radio = [&](int first, int last, int on) { menu_radio(m, first, last, on); };
	auto enable = [&](int id, bool on) { menu_enable(m, id, on); };
	// a small menu as a whole
	auto enable_popup = [&](MenuRef parent, MenuRef child, bool on) { menu_enable_sub(parent, child, on); };
	// the book commands of the main engine: not while it learns, nor while it saves its book as it ends
	const bool book_free = !learning_ && !book_saving_;
	enable_popup(book_menu_, keep_menu_, book_free);
	enable_popup(learn_menu_, leaf_menu_, book_free);
	check(cmd::ShowEval, show_eval_);
	radio(cmd::Backward, cmd::BackgroundOff, backward_ ? cmd::Backward : forward_ ? cmd::Forward : cmd::BackgroundOff);
	check(cmd::PrecisionOn, precision_);
	check(cmd::PvOnBoard, pv_on_board_);
	check(cmd::OpeningMarks, opening_marks_);
	check(cmd::BookmarksShow, bookmarks_shown_);
	check(cmd::RecordBoxShow, record_box_);
	check(cmd::GraphEndShow, graph_end_);
	check(cmd::UpperMoves, upper_moves_);
	check(cmd::FixRecord, fixed_on_);
	enable(cmd::LearnStop, learning_);
	for (int id = cmd::LearnDeviate; id <= cmd::LearnJobLast; ++id) enable(id, book_free);
	enable(cmd::LearnReduce, book_free && reduce_.state == ReduceJob::None);
	// these close the main engine (a learning command takes no input to stop)
	for (int id : {cmd::ReloadBook, cmd::OpenBook, cmd::EngineNikque, cmd::EngineNewedax, cmd::RestartEngines}) enable(id, book_free);
	enable(cmd::LearnContinue, book_free);
	enable(cmd::ExportBest, book_free);
	enable(cmd::ExportDeviate, book_free);
	enable(cmd::ListFromBook, book_free && !task_busy_);
	enable(cmd::QueueStart, book_free && !learn_queue_.empty());
	enable(cmd::QueueClear, !learn_queue_.empty());
	for (int id = cmd::ToolSolve; id < cmd::ToolStop; ++id) enable(id, !tool_busy_);
	for (int id = cmd::BaseConvert; id <= cmd::BaseTofen; ++id) enable(id, !tool_busy_);
	enable(cmd::ToolStop, tool_busy_);
	// the list waits for the job in progress
	for (int id = cmd::ListOpen; id < cmd::TaskCancel; ++id) enable(id, !task_busy_);
	const bool have_list = !list_.empty() && !task_busy_;
	for (int id : {cmd::ListSave, cmd::ListSort, cmd::ListUnique, cmd::ListTruncate, cmd::ListShuffle, cmd::ListNormalize,
	               cmd::ListScatter, cmd::ListClear, cmd::ListPrev, cmd::ListNext, cmd::ListSearch})
		enable(id, have_list);
	enable(cmd::ListDelete, have_list && list_index_ >= 0);
	enable(cmd::ListReload, have_list && list_index_ >= 0);
	enable(cmd::ListSearchClear, hits_shown_);
	enable(cmd::TaskCancel, true);  // (also the book analyses and the searches of the game)
	enable(cmd::StopScript, !script_.empty() || script_wait_ != ScriptWait::None);
	for (int id : {cmd::KeptShow, cmd::KeptCompare, cmd::KeptDelete}) enable(id, kept_sel_ >= 0 && kept_sel_ < static_cast<int>(kept_.size()));
	enable(cmd::KeptClear, !kept_.empty());
	for (int id = cmd::EditDone; id <= cmd::EditInitial; ++id) enable(id, editing_);
	check(cmd::EditBoard, editing_);
	enable(cmd::EstimateLine, !editing_ && !findline_id_ && !task_busy_);
	check(cmd::UseBook, use_book_);
	check(cmd::CountOn, count_);
	check(cmd::TogglePanel, panel_hidden_);
	check(cmd::ToggleLog, log_open_);
	check(cmd::MoveNumbers, move_numbers_);
	radio(cmd::PlayFree, cmd::PlayEngines, cmd::PlayFree + static_cast<int>(play_mode_));
	radio(cmd::OpeningSearch, cmd::OpeningWide, cmd::OpeningSearch + static_cast<int>(opening_));
	check(cmd::PlayPonder, ponder_);
	check(cmd::PlayKeepHash, keep_hash_);
	check(cmd::CompareOn, compare_);
	enable(cmd::PlayTakeBack, play_mode_ == PlayMode::HumanBlack || play_mode_ == PlayMode::HumanWhite);
	enable(cmd::PlayStop, play_mode_ != PlayMode::Free);
	{
		const std::wstring lv = lang::f("menu.analysis.level", {std::to_wstring(level_)});
		menu_rename(m, cmd::LevelChoose, lv);
	}
	radio(cmd::UmiBest, cmd::UmiOff, umi_ == UmiMode::Best ? cmd::UmiBest : umi_ == UmiMode::Loss ? cmd::UmiLoss : cmd::UmiOff);
	radio(cmd::BaseDeviate, cmd::BaseDeviate3,
	      base_ == "deviate2" ? cmd::BaseDeviate2 : base_ == "deviate3" ? cmd::BaseDeviate3 : cmd::BaseDeviate);
	radio(cmd::EngineNikque, cmd::EngineNewedax, kind_ == EngineKind::Newedax ? cmd::EngineNewedax : cmd::EngineNikque);
	radio(cmd::DesignStandard, cmd::DesignPaw, design_ == Design::Paw ? cmd::DesignPaw : cmd::DesignStandard);
	radio(cmd::EffectsOn, cmd::EffectsOff,
	      effects_ == Effects::On ? cmd::EffectsOn : effects_ == Effects::Subdued ? cmd::EffectsSubdued : cmd::EffectsOff);
	radio(cmd::TopStrong, cmd::TopWeak, cmd::TopStrong + static_cast<int>(top_effect_));
	radio(cmd::TabMoves, cmd::TabTree, cmd::TabMoves + static_cast<int>(tab_));
	radio(cmd::Rotate0, cmd::Rotate0 + 7, cmd::Rotate0 + orient_);
	radio(cmd::CellInfoNone, cmd::CellInfoUmi, cmd::CellInfoNone + static_cast<int>(cell_info_));
	radio(cmd::LangJa, cmd::LangZhHant, cmd::LangJa + std::max(0, name_index(kLanguages, lang_)));
}

void App::set_language(const std::string &code)
{
	lang_ = code;
	ini_.set("view", "language", code);
	lang::load(exe_dir_, code);
	gfx_.clear_fonts();
	set_window_title(hwnd_, lang::t("app.title"));
	build_menu();
	layout();
	invalidate();
}

// ---- layout

void App::layout()
{
	if (!gfx_.ready()) return;  // (no window drawn yet)
	Layout &L = lay_;
	const float W = static_cast<float>(gfx_.width()), H = static_cast<float>(gfx_.height());
	const Theme &th = theme(design_);
	L.scale = window_scale(hwnd_);
	// the text grows with a tall window (14 to 22 pixels at 96 DPI), or is the size set in config.ini
	const float base = font_px_ > 0 ? static_cast<float>(font_px_) : std::clamp(H / L.scale / 60.0f, 14.0f, 22.0f);
	L.font = std::round(base * L.scale);
	L.font_s = std::round(base * 0.82f * L.scale);
	L.row = std::round(L.font * 1.8f);
	L.bar = {0, 0, W, L.row};
	const float log_h = log_open_ ? std::max(L.row * 8, std::round(H * 0.28f)) : L.row;
	L.log = {0, H - log_h, W, H};
	const float top = L.row, bottom = std::max(top + 1, H - log_h);
	const float ch = bottom - top;
	const float coord = std::round(L.font * 1.2f);
	const float pad = std::round(6 * L.scale);
	const float want_panel = panel_hidden_ ? 0 : std::max(300 * L.scale, panel_w_ * L.scale);
	L.panel_below = !panel_hidden_ && (W - want_panel < ch * 0.7f) && H > W;
	float area_w = W, area_h = ch;
	if (!panel_hidden_) {
		if (L.panel_below) area_h = std::round(ch * 0.62f);
		else area_w = std::max(120.0f, W - want_panel);
	}
	// the row of the opening name is kept all the time: the board stays where it is when a name comes
	const float name_h = openings_.empty() ? 0 : std::round(L.row * 1.4f);
	const float avail = std::max(40.0f, std::min(area_w - coord - 2 * pad, area_h - coord - name_h - 2 * pad));
	int cell = std::max(4, static_cast<int>(avail / 8));
	int lw = 1, fw = 2;
	for (; cell > 4; --cell) {
		lw = std::max(1, static_cast<int>(std::lround(cell / 44.0)));
		fw = std::max(lw * 2, static_cast<int>(std::lround(cell * th.frame_ratio)));
		if (8 * cell + 9 * lw + 2 * fw <= avail) break;
	}
	L.cell = cell;
	L.line = lw;
	L.frame = fw;
	L.board_px = 8 * cell + 9 * lw + 2 * fw;
	const float block = L.board_px + coord;  // the board with the coordinates
	const float block_h = block + name_h;
	if (panel_hidden_) {
		L.board_area = {0, top, W, bottom};
		L.panel = {W, top, W, bottom};
	} else if (!L.panel_below) {
		const float used = std::round(block + 2 * pad);
		L.board_area = {0, top, used, bottom};
		L.panel = {used, top, W, bottom};
	} else {
		const float used = std::round(block_h + 2 * pad);
		L.board_area = {0, top, W, top + used};
		L.panel = {0, top + used, W, bottom};
	}
	const float aw = L.board_area.right - L.board_area.left, ah = L.board_area.bottom - L.board_area.top;
	L.bx = static_cast<int>(L.board_area.left + coord + std::floor((aw - block) / 2));
	L.by = static_cast<int>(L.board_area.top + coord + std::floor((ah - block_h) / 2));
	const float board_bottom = static_cast<float>(L.by + L.board_px);
	L.opening = {static_cast<float>(L.bx), board_bottom, static_cast<float>(L.bx + L.board_px), board_bottom + name_h};
	L.panel_split = {L.panel.left - 4 * L.scale, top, L.panel.left + 4 * L.scale, bottom};

	// the panel
	const RectF P{L.panel.left + pad, L.panel.top + pad, L.panel.right - pad, L.panel.bottom - pad};
	L.tabs = {P.left, P.top, P.right, P.top + L.row};
	L.range = {P.left, L.tabs.bottom + 4 * L.scale, P.right, L.tabs.bottom + 4 * L.scale + L.row};
	const float rest_top = L.range.bottom + 4 * L.scale;
	const float rest_h = std::max(0.0f, P.bottom - rest_top);
	const float split_h = std::round(7 * L.scale);
	// the row of the bookmarks, right above the graph
	const float bm_h = bookmarks_shown_ ? std::round(L.row * 2.7f) : 0;
	// the row of the moves as text, right on the graph
	const float rec_h = record_box_ && !panel_hidden_ ? std::round(L.row * 1.25f) : 0;
	const float avail_h = std::max(0.0f, rest_h - bm_h - rec_h);
	L.graph_room = avail_h;
	float gh = std::round(avail_h * graph_ratio_);
	gh = std::clamp(gh, std::min(L.row * 3, avail_h * 0.5f), std::max(L.row * 3, avail_h - L.row * 4));
	L.table = {P.left, rest_top, P.right, std::max(rest_top, P.bottom - gh - split_h - bm_h - rec_h)};
	L.bookmarks = {P.left, L.table.bottom, P.right, L.table.bottom + bm_h};
	L.splitter = {P.left, L.bookmarks.bottom, P.right, L.bookmarks.bottom + split_h};
	L.record = {P.left, L.splitter.bottom, P.right, L.splitter.bottom + rec_h};
	L.graph = {P.left, L.record.bottom, P.right, P.bottom};

	// the range row: "label  X [ 2 ] ---o---  Y [ 4 ] ---o---"
	TextFormat *f = gfx_.text(L.font, false);
	const float gap = std::round(8 * L.scale);
	float x = L.range.left + gfx_.text_width(lang::t("range.title"), f) + gap * 1.5f;
	const float box_w = std::round(L.font * 2.8f);
	const float lx = gfx_.text_width(lang::t("range.x"), f), ly = gfx_.text_width(lang::t("range.y"), f);
	const float free = L.range.right - x - lx - ly - 2 * box_w - 6 * gap;
	const float track_w = free >= 60 * L.scale ? std::min(160 * L.scale, free / 2) : 0;
	const float bt = L.range.top + 3 * L.scale, bb = L.range.bottom - 3 * L.scale;
	x += lx + gap * 0.5f;
	L.fx.box = {x, bt, x + box_w, bb};
	L.fx.track = {x + box_w + gap, bt, x + box_w + gap + track_w, bb};
	x = L.fx.track.right + gap * 2 + ly + gap * 0.5f;
	L.fy.box = {x, bt, x + box_w, bb};
	L.fy.track = {x + box_w + gap, bt, x + box_w + gap + track_w, bb};

	// the row of the moves: the text box, then the buttons
	L.record_text = L.record_copy = L.record_paste = {};
	if (rec_h > 0) {
		TextFormat *fb = gfx_.text(L.font_s, false);
		const float rgap = std::round(4 * L.scale), rb = L.record.bottom - rgap;
		const float paste_w = gfx_.text_width(lang::t("record.paste"), fb) + 2 * L.font_s;
		const float copy_w = gfx_.text_width(lang::t("record.copy"), fb) + 2 * L.font_s;
		L.record_paste = {L.record.right - paste_w, L.record.top, L.record.right, rb};
		L.record_copy = {L.record_paste.left - rgap - copy_w, L.record.top, L.record_paste.left - rgap, rb};
		L.record_text = {L.record.left, L.record.top, std::max(L.record.left, L.record_copy.left - rgap), rb};
	}
	place_record_box();
}

// ---- drawing

double App::now_s() const
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0_).count();
}

void App::paint()
{
	measure::Frame mf(true);
	sync_record_box();
	compute_line();
	prepare_glows();
	if (!gfx_.begin_frame()) return;
	const Theme &th = theme(design_);
	gfx_.dc()->clear(th.bg);
	draw_board(*this);
	draw_panel(*this);
	draw_log(*this);
	draw_bar(*this);
	gfx_.end_frame();
	measure::first_frame();
}

void App::paint_animation()
{
	const std::vector<int> cells = animated_cells();
	if (cells.empty() || !active_ || window_minimized(hwnd_)) {
		update_animation_timer();
		return;
	}
	measure::Frame mf(false);
	if (!gfx_.begin_frame()) return;
	std::vector<RectF> dirty;
	dirty.reserve(cells.size());
	const double t = now_s();
	for (const int sq : cells) {
		const RectF r = lay_.cell_rect(sq);
		gfx_.dc()->push_clip(r);
		draw_cell(*this, sq, t);
		gfx_.dc()->pop_clip();
		dirty.push_back(r);
	}
	gfx_.end_frame(&dirty);
}

void App::paint_bar()
{
	// the meaning of a stage is drawn below the bar: then the whole window
	if (window_minimized(hwnd_) || hover_stage_ >= 0) {
		invalidate();
		return;
	}
	if (!gfx_.begin_frame()) return;
	std::vector<RectF> dirty{lay_.bar};
	gfx_.dc()->push_clip(lay_.bar);
	draw_bar(*this);
	gfx_.dc()->pop_clip();
	gfx_.end_frame(&dirty);
}

void App::update_animation_timer()
{
	const bool want = effects_ == Effects::On && active_ && hwnd_ && !window_minimized(hwnd_) && !animated_cells().empty();
	if (want == animating_) return;
	animating_ = want;
	if (want) set_timer(kTimerAnim, 1000 / anim_fps_);
	else kill_timer(kTimerAnim);
}

CellView App::cell_view(int sq) const
{
	CellView v;
	if (editing_) {  // a position being set up: its discs only
		if ((edit_pos_.black >> sq) & 1) v.stone = 1;
		else if ((edit_pos_.white >> sq) & 1) v.stone = 2;
		return v;
	}
	const Position &p = game_.now();
	if ((p.black >> sq) & 1) v.stone = 1;
	else if ((p.white >> sq) & 1) v.stone = 2;
	// the last move (a pass belongs to the move before)
	for (int i = game_.cursor() - 1; i >= 0; --i) {
		if (game_.moves()[i] == kPass) continue;
		v.last = game_.moves()[i] == sq;
		break;
	}
	if (v.stone && move_numbers_) v.number = move_number_at(sq);
	if (opening_marks_ && !v.stone && p.can_play(sq)) v.opening = openings_.mark(p.play(sq));
	if (!show_eval_ || v.stone) return v;
	const PosInfo *info = cache_.peek(p);

	// the expected line of the move under the mouse, from the reply on: numbers on the squares (when it is set so)
	if (pv_on_board_ && info && hover_sq_ != kNoMove && hover_sq_ != sq) {
		if (const MoveEval *h = info->find(hover_sq_); h && h->pv.size() > 1) {
			int side = p.side ^ 1, num = 0;
			for (size_t k = 1; k < h->pv.size() && num < 12; ++k) {
				if (h->pv[k] == kPass) {
					side ^= 1;
					continue;
				}
				++num;
				if (h->pv[k] == sq) {
					v.pv_num = num;
					v.pv_side = side;
					break;
				}
				side ^= 1;
			}
		}
	}
	if (!p.can_play(sq)) return v;
	v.selected = sq == selected_sq_;
	const MoveEval *m = info ? info->find(sq) : nullptr;
	if (!m) {
		v.pending = eng_[0].alive || eng_[1].alive;
		return v;
	}
	// the move played next in the game takes the value that came back from the next position
	const int cur = game_.cursor();
	const LineEval *le = (line_.size() == static_cast<size_t>(game_.size()) + 1 && cur < game_.size()) ? &line_[cur] : nullptr;
	const int played_sq = cur < game_.size() ? game_.moves()[cur] : kNoMove;
	auto value_of = [&](const MoveEval &o, int *x) {
		if (le && o.sq == played_sq && le->played_improved) {
			*x = le->played;
			return true;
		}
		return shown_value(o, use_book_, x);
	};
	v.has_value = value_of(*m, &v.value);
	v.stage = (use_book_ && m->book_row && m->stage > 0) ? m->stage : 0;
	// a value of the book whose stage is not known (the precision stages not asked, or not come yet): no level of the search
	if (v.has_value && v.stage == 0 && !(use_book_ && m->in_book && !(le && sq == played_sq && le->played_improved))) {
		v.level = shown_level(*m, use_book_);
		v.exact = move_exact(*m, p);
		if (le && sq == played_sq && le->played_improved) {
			v.improved = true;
			v.exact = le->played_exact;
		}
	}
	v.pending = !v.has_value && (eng_[1].alive || (use_book_ && eng_[0].alive && !info->book_done));
	if (cell_info_ != CellInfo::None && v.has_value) v.info = cell_info_text(*m, cell_info_, use_book_);
	int best = -999;
	for (const auto &o : info->moves) {
		int x;
		if (value_of(o, &x)) best = std::max(best, x);
	}
	if (v.has_value) {
		v.best = v.value == best;
		v.good = !v.best && v.value >= best - 2;
	}
	// the fixed record, when the game has followed it up to here
	if (fixed_on_ && fixed_start_ == game_.start() && cur < static_cast<int>(fixed_.size()) && fixed_[cur] == sq &&
	    cur <= game_.size() && std::equal(fixed_.begin(), fixed_.begin() + cur, game_.moves().begin())) {
		v.fixed_mark = cur >= game_.size() || game_.moves()[cur] != sq;
		v.fixed_bad = v.has_value && v.value < best;
	}
	return v;
}

std::vector<int> App::animated_cells() const
{
	std::vector<int> out;
	if (effects_ != Effects::On || !show_eval_) return out;
	u64 m = game_.now().moves();
	while (m) {
		const int sq = std::countr_zero(m);
		m &= m - 1;
		const CellView v = cell_view(sq);
		if ((v.has_value && v.stage >= 3) || v.pending) out.push_back(sq);
	}
	return out;
}

void App::log(const std::wstring &line)
{
	log_.push_back(line);
	while (log_.size() > 500) log_.pop_front();
	status_ = line;
	invalidate_window(hwnd_);
}

int App::message(const std::wstring &text, UINT flags)
{
	if (message_hook_) return message_hook_(text, flags);
	return message_box(hwnd_, text, lang::t("app.title"), flags);
}

void App::set_status(const std::wstring &s)
{
	if (s == status_) return;
	status_ = s;
	invalidate();
}

void App::compute_line()
{
	line_ = backward_line(game_, cache_, use_book_, backward_);
}

}  // namespace ng
