// The main window: menu, the information bar, the board, the right panel (tabs, the range row, the table of
// moves, the graph) and the log. Split over app.cpp (window, layout, drawing), app_input.cpp (keys, mouse,
// menu commands) and app_engine.cpp (the engines and their answers).
#pragma once

#include <windows.h>

#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "analysis.h"
#include "engine.h"
#include "game.h"
#include "gamelist.h"
#include "gfx.h"
#include "ini.h"
#include "keys.h"
#include "openings.h"
#include "theme.h"

namespace ng {

inline constexpr UINT WM_ENGINE = WM_APP + 1;
// the wParam of WM_ENGINE: the role of the engine (3 bits) and the number of its start
inline WPARAM engine_tag(int role, unsigned generation) { return static_cast<WPARAM>(role) | (static_cast<WPARAM>(generation) << 3); }
inline constexpr UINT WM_TASK = WM_APP + 2;  // a background job has ended
inline constexpr UINT WM_SCRIPT = WM_APP + 3;  // the command script goes on (after the work that it waited for)

// menu commands
namespace cmd {
enum : int {
	NewGame = 1000,
	Undo,
	Redo,
	ToStart,
	ToEnd,
	PlayFree = 1010,
	PlayHumanBlack,
	PlayHumanWhite,
	PlayEngines,
	PlayEngineMove,
	PlayTakeBack,
	PlayStop,
	PlayLevel,
	OpeningSearch,
	OpeningBest,
	OpeningWide,
	OpeningSettings,
	PlayClock,
	PlayPonder,
	PlayEndSettings,
	ShowEval = 1100,
	Recompute,
	Backward,
	UmiBest,
	UmiLoss,
	UmiOff,
	CountOn,
	CompareOn = 1120,
	Level0 = 1150,  // + index into kLevels
	UseBook = 1200,
	ReloadBook,
	BaseDeviate,
	BaseDeviate2,
	BaseDeviate3,
	BookInfo,
	OpenBook,
	ExportMissing,
	ExportBest,
	RecentBook0 = 1250,  // + index of the recent books (up to 8)
	LearnDeviate = 1300,
	LearnDeviate2,
	LearnDeviate3,
	LearnEnhance,
	LearnLeafExpand,
	LearnFill,
	LearnFill2,
	LearnPlay,
	LearnStore,
	LearnAdd,
	LearnLearn,
	LearnCheck,
	LearnNegamax,
	LearnFix,
	LearnCorrect,
	LearnLeafRecalc1,
	LearnLeafRecalc2,
	LearnLeafRecalc3,
	LearnLeafRecalc4,
	LearnMerge,
	LearnSubtree,
	LearnPrune,
	LearnExport,
	LearnImport,
	LearnExtract,
	BookNewEmpty,
	BookSave,
	BookSaveAs,
	BookDepth,
	BookProblem,
	EngineCommand,  // the command box: any command for the main engine
	LearnJobLast = EngineCommand,
	LearnStop = 1350,
	LearnContinue,
	LearnSettings,
	QueueMissing,
	QueueStart,
	QueueClear,
	BookShowInfo = 1270,  // answered at once, beside the analyses
	BookShowStats,
	BookShowPosition,
	EngineNikque = 1400,
	EngineNewedax,
	RestartEngines,
	EngineVersion,
	ToolSolve = 1420,
	ToolBench,
	ToolCount,
	ToolPerft,
	ToolEstimate,
	ToolStop,
	BaseConvert = 1440,
	BaseUnique,
	BaseCompare,
	BaseCheck,
	BaseCorrect,
	BaseComplete,
	BaseProblem,
	BaseTofen,
	CopyMoves = 1500,
	PasteMoves,
	FixRecord,
	CopyBoardText,
	CopyObf,
	CopyBitDecimal,
	CopyBitHex,
	CopyBitNtest,
	ImportPosition,
	EditBoard,
	EditDone,
	EditCancel,
	EditSide,
	EditClear,
	EditInitial,
	EstimateLine,
	Screenshot,
	SaveBoardImage,
	RunScript,
	StopScript,
	DesignStandard = 1600,
	DesignPaw,
	EffectsOn,
	EffectsSubdued,
	EffectsOff,
	TopStrong,
	TopNormal,
	TopWeak,
	TogglePanel,
	ToggleLog,
	TabMoves,
	TabBook,
	TabRecord,
	TabList,
	TabKept,
	TabTree,
	MoveNumbers,
	RotateNext,
	SaveLog,
	Rotate0 = 1640,  // + symmetry 0..7
	CellInfoNone = 1650,
	CellInfoPositions,
	CellInfoLevel,
	CellInfoStage,
	CellInfoUmi,
	OpenConfig = 1700,
	RangeSettings,
	LangJa,
	LangEn,
	LangZhHans,
	LangZhHant,
	HelpKeys = 1800,
	HelpAbout,
	ListOpen = 1900,
	ListAdd,
	ListSave,
	ListAddGame,
	ListSort,
	ListUnique,
	ListTruncate,
	ListShuffle,
	ListNormalize,
	ListScatter,
	ListDelete,
	ListClear,
	ListStartPly,
	ListFromBook,
	ListPrev,
	ListNext,
	ListReload,
	ListSearch,
	ListSearchClear,
	TaskCancel,
	KeepNow = 1950,
	KeptShow,
	KeptCompare,
	KeptDelete,
	KeptClear,
};
}
inline constexpr int kLevels[] = {1, 5, 10, 15, 18, 21, 24, 27, 30, 35, 40, 60};

enum class Effects : int { On = 0, Subdued = 1, Off = 2 };
// how much of its effects the top stage (7) shows
enum class TopEffect : int { Strong = 0, Normal = 1, Weak = 2 };
enum class Tab : int { Moves = 0, Book = 1, Record = 2, List = 3, Kept = 4, Tree = 5, Count };
// the small number at the lower left of a move's square
enum class CellInfo : int { None = 0, Positions, Level, Stage, Umi };
enum class EngineKind : int { Nikque18 = 0, Newedax = 1 };
enum class UmiMode : int { Best = 0, Loss = 1, Off = 2, Range = 3 };
enum class Column : int { Move, Value, Stage, Missing, Positions, Level, Umi, Compare, Count };
// who plays in a game: people only (free), a person and the computer, the computer on both sides
enum class PlayMode : int { Free = 0, HumanBlack, HumanWhite, Engines };
// the opening moves of the computer: searched, the best book move, a book move drawn within a width
enum class Opening : int { Search = 0, BookBest, BookWide };

struct Layout {
	float scale = 1;
	float font = 14, font_s = 11, row = 24;
	D2D1_RECT_F bar{}, board_area{}, panel{}, log{};
	// board: whole pixels
	int cell = 40, line = 1, frame = 2;
	int bx = 0, by = 0, board_px = 0;  // top left of the frame, size with the frame
	bool panel_below = false;
	D2D1_RECT_F tabs{}, range{}, table{}, splitter{}, graph{}, panel_split{};
	struct Field {
		D2D1_RECT_F box{}, track{};
	} fx, fy;

	D2D1_RECT_F cell_rect(int sq) const
	{
		const float x = static_cast<float>(bx + frame + line + (sq & 7) * (cell + line));
		const float y = static_cast<float>(by + frame + line + (sq >> 3) * (cell + line));
		return {x, y, x + cell, y + cell};
	}
	int square_at(float x, float y) const;
};

// what one square shows
struct CellView {
	int stone = 0;          // 0 empty, 1 black, 2 white
	bool last = false;
	bool has_value = false;
	int value = 0;
	int stage = 0;
	int level = 0;          // the small "Lv" of stage 0 (0: none)
	bool exact = false;     // a search to the end
	bool pending = false;   // the value is being computed
	bool best = false, good = false, selected = false;
	bool fixed_bad = false;   // the move of the fixed record here loses against the best move
	bool fixed_mark = false;  // the fixed record goes another way here than the game
	bool improved = false;  // the value came back from the next position (backward analysis): a star by the level
	int pv_num = 0;          // 1.. : this square is in the expected line of the move under the mouse
	int pv_side = 0;         // the side who plays it there (0 black, 1 white)
	int number = 0;          // the number of the move that put the disc here (numbers shown)
	std::wstring info;       // the small number at the lower left (the setting of the squares)
};

struct EngineSlot {
	EngineProcess proc;
	bool alive = false;
	bool loaded = false;               // the first answer came (the book is read)
	std::optional<PosKey> board;       // the position the engine has (setboard)
	unsigned generation = 0;           // counts the starts: messages of a process closed before are ignored
	int ping_id = 0;                   // the echo marker the engine has not answered yet (watch)
	std::chrono::steady_clock::time_point ping_sent{};
	std::wstring version;              // from "Edax version ..."
	int probe_id = 0;                  // "stop id=<n>" sent first, not answered yet ("@cancel id=<n>": the book analyses are known)
	bool old = false;                  // the version came before that answer: an engine without the book analyses
	std::wstring error;
};

struct MissingList {
	PosKey pos{};
	int sq = kNoMove;
	std::string params;
	bool asked = false, done = false;
	int count = 0, stage = -1;
	std::vector<MissingLine> lines;
};

// a request to the main engine, by id
struct Request {
	enum Kind { Book, Count, Levels, Marker, Graph, Missing, Best, BestExport, FindLine, ListBook, ListBookAdd } kind;
	PosKey pos;
	int engine;
};

class App {
public:
	int run(HINSTANCE inst, int show);
	LRESULT proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

	// ---- state (shared by the app_*.cpp files)
	HWND hwnd_ = nullptr;
	HINSTANCE inst_ = nullptr;
	std::wstring exe_dir_, ini_path_, temp_dir_;
	Ini ini_;
	KeyMap keys_;
	Gfx gfx_;
	Layout lay_;
	Game game_;
	AnalysisCache cache_;
	Openings openings_;
	bool show_opening_ = true;
	D2D1_RECT_F opening_rect_{};   // the opening name in the bar (a click hides it)
	// the small menus of book commands (greyed as a whole while the book is learning)
	HMENU book_menu_ = nullptr, keep_menu_ = nullptr, learn_menu_ = nullptr, leaf_menu_ = nullptr;

	Design design_ = Design::Standard;
	Effects effects_ = Effects::On;
	TopEffect top_effect_ = TopEffect::Normal;
	Tab tab_ = Tab::Moves;
	bool show_eval_ = true;
	bool use_book_ = true;
	bool panel_hidden_ = false;
	bool log_open_ = false;
	float panel_w_ = 420;      // at 96 DPI
	float graph_ratio_ = 0.34f;
	int level_ = 21;
	int threads_ = 0;          // 0: all logical CPUs
	UmiMode umi_ = UmiMode::Best;
	int loss_x_ = 2, loss_y_ = 4;
	int umi_lo_ = -64, umi_hi_ = 64;  // the range of values of the umigame numbers (UmiMode::Range)
	int umi_depth_ = 60;             // the last ply the umigame numbers go to
	std::string base_ = "deviate";
	std::string stages_ = "1/1,2/2,3/3,4/4,5/5";
	bool count_ = true;        // ask the number of positions (heavier)
	EngineKind kind_ = EngineKind::Nikque18;
	bool search_engine_ = true;
	bool backward_ = true;     // backward analysis of the game (and searching its positions in the background)

	// engines: [0] main (reads the book), [1] search only (no book), [2] tools (started for one job, no book),
	// [3] play (the moves of the computer in a game, no book), [4] compare (the other engine, search only)
	EngineSlot eng_[5];
	bool tool_busy_ = false;
	int tool_end_id_ = 0;
	std::wstring tool_label_;
	std::chrono::steady_clock::time_point tool_t0_{};
	int hash_bits_ = 0;        // hash-table-size given to the engines (0: their own)
	int next_id_ = 1;
	std::unordered_map<int, Request> requests_;
	int book_id_ = 0, count_id_ = 0, levels_id_ = 0, best_id_ = 0;   // running on the main engine
	std::wstring best_file_;   // the file the best lines are written to (book bestlines file=)
	int graph_id_ = 0;         // the book analysis of another position of the game (for the graph), one at a time
	int missing_id_ = 0;
	MissingList missing_;
	int missing_scroll_ = 0;
	std::vector<std::pair<int, D2D1_RECT_F>> missing_rects_;  // book tab: index of the line
	// the fixed record (compared with the game)
	std::vector<int> fixed_;
	Position fixed_start_{};
	bool fixed_on_ = false;
	// book learning on the main engine
	bool learning_ = false;
	int learn_end_id_ = 0;
	std::string learn_cmd_;
	std::wstring learn_progress_;
	std::chrono::steady_clock::time_point learn_t0_{};
	// the learning queue: jobs done one after another
	struct LearnJob {
		Position start;
		std::string moves;    // from the start, passes as "pa"
		std::string command;  // lines
	};
	std::deque<LearnJob> learn_queue_;
	bool queue_running_ = false;
	// search engine: the hint in progress
	int marker_id_ = 0;
	bool hint_busy_ = false, hint_accept_ = false;
	PosKey hint_pos_{};
	int hint_level_ = 0;
	bool hint_background_ = false;  // the hint in progress is for another position of the game
	int hint_end_id_ = 0;            // the marker sent after the hint
	std::vector<LineEval> line_;    // computed with each full drawing
	int engine_level_[5] = {0, 0, 0, 0, 0};

	// the board
	int orient_ = 0;               // the symmetry the game is shown in (from the game as it came)
	bool move_numbers_ = false;    // the numbers of the moves on the discs
	CellInfo cell_info_ = CellInfo::None;
	bool editing_ = false;         // setting up a position
	Position edit_pos_{};
	int findline_id_ = 0;          // book findline asked of the main engine
	Position findline_target_{};
	int findline_sym_ = 0;         // the symmetry of the target asked (0, or 1 for the other start position)
	bool findline_found_ = false;

	// the game list
	GameList list_;
	int list_index_ = -1;          // the game of the list on the board
	int list_start_ply_ = -1;      // a game from the list opens after this many moves (-1: at its end)
	std::wstring list_file_;
	int list_scroll_ = 0;
	std::vector<ListHit> hits_;    // the games where the position searched appears
	bool hits_shown_ = false;
	int list_book_limit_ = 10000;     // the lines of the book read into the list, at most
	Position list_book_start_{};
	int list_book_id_ = 0;            // the book lines asked of the main engine
	std::vector<std::pair<int, D2D1_RECT_F>> list_rects_;
	// the kept lines (no limit), saved next to the exe
	struct Kept {
		Position start;
		std::vector<int> moves;    // with the passes
		int cursor = 0;
		long long time = 0;        // seconds since 1970
	};
	std::vector<Kept> kept_;
	int kept_sel_ = -1;
	int kept_scroll_ = 0;
	std::vector<std::pair<int, D2D1_RECT_F>> kept_rects_;

	// the tree of the book lines from a position (the root): the game up to the root, and the nodes open, each
	// as its moves from the root (no passes)
	Game tree_base_;
	bool tree_set_ = false;
	std::set<std::vector<int>> tree_open_;
	int tree_scroll_ = 0;
	struct TreeRow {
		std::vector<int> path;  // the moves from the root to the position after this row's move
		D2D1_RECT_F rect{}, arrow{};
		bool can_open = false;
	};
	std::vector<TreeRow> tree_rows_;  // as drawn

	// one background job at a time (work on the list, the search of a line); its end runs on the window's thread
	std::jthread task_;
	bool task_busy_ = false;
	std::wstring task_label_;
	std::chrono::steady_clock::time_point task_t0_{};
	std::mutex task_mutex_;
	std::function<void()> task_done_;

	// the command script: lines run one after another; a line that starts work waits for its end
	struct ScriptLine {
		int depth;
		std::string text;
	};
	std::deque<ScriptLine> script_;
	enum class ScriptWait { None, Task, Learn, Tool, Analysis, FindLine, BookLines, Game, Com } script_wait_ = ScriptWait::None;
	std::chrono::steady_clock::time_point script_until_{};
	std::wstring script_dir_;

	// a game against the computer (the play engine searches its moves; book moves come from the book analyses)
	PlayMode play_mode_ = PlayMode::Free;
	int com_level_ = 21;
	Opening opening_ = Opening::BookBest;
	int opening_width_ = 2;          // BookWide: the loss of a book move drawn at most
	int opening_until_ = 60;         // book moves up to this move (number of discs - 4)
	int clock_minutes_ = 0;          // time of a side in a game (0: none)
	long long clock_left_ms_[2] = {0, 0};
	std::chrono::steady_clock::time_point clock_t0_{};
	int clock_side_ = -1;            // the side whose time runs (-1: none)
	bool ponder_ = false;            // the play engine searches the person's position meanwhile
	bool end_keep_ = true, end_list_ = false;
	int engine_games_left_ = 0;      // the computer on both sides: more games after this one
	bool com_once_ = false;          // one move of the computer asked (free play)
	bool com_thinking_ = false;      // the play engine searches a move
	bool com_pondering_ = false;
	PosKey com_pos_{};
	int com_book_move_ = kNoMove;    // a book move to play when the timer comes
	bool game_end_done_ = false;     // the end of this game handled
	bool com_waiting_book_ = false;   // the book analysis of the position is waited for
	PosKey com_no_book_{};           // the position whose book analysis did not come in time: searched
	bool com_no_book_set_ = false;
	// the compare mode: the other engine (search only) beside the table
	bool compare_ = false;
	int cmp_marker_id_ = 0, cmp_end_id_ = 0, cmp_level_ = 0;
	bool cmp_busy_ = false, cmp_accept_ = false;
	PosKey cmp_pos_{};     // relative names in a script are from its folder
	std::string lang_ = "ja";

	// interaction
	Column sort_col_ = Column::Value;
	bool sort_desc_ = true;
	int selected_sq_ = kNoMove;
	int hover_sq_ = kNoMove;
	int hover_stage_ = -1;         // the legend dot under the mouse
	POINT mouse_{};
	enum class Drag { None, Panel, Graph, SliderX, SliderY } drag_ = Drag::None;
	int edit_field_ = 0;           // 1 X, 2 Y being typed
	std::wstring edit_text_;
	std::chrono::steady_clock::time_point paste_armed_{};
	std::deque<std::wstring> log_;
	std::wstring status_;          // the line shown in the log row (hover, messages)

	// places found while drawing, for the mouse
	std::vector<std::pair<Column, D2D1_RECT_F>> col_rects_;    // table header cells
	std::vector<std::pair<int, D2D1_RECT_F>> row_rects_;       // table rows: square
	std::vector<std::pair<int, D2D1_RECT_F>> record_rects_;    // record tab: number of moves
	std::vector<std::pair<int, D2D1_RECT_F>> tab_rects_;
	D2D1_RECT_F legend_rects_[8]{};
	D2D1_RECT_F graph_plot_{};
	int table_scroll_ = 0;

	// animation
	bool animating_ = false;
	std::chrono::steady_clock::time_point t0_ = std::chrono::steady_clock::now();
	bool active_ = true;
	int anim_fps_ = 20;        // frames a second of the moving effects (config.ini [view] fps, 5..60)
	int font_px_ = 0;          // text size at 96 DPI (config.ini [view] font; 0: by the window height)

	// ---- app.cpp
	void load_config();
	void save_config();
	void build_menu();
	void update_menu();
	void layout();
	void paint();
	void paint_animation();
	void paint_bar();  // the bar at the top alone (the clock of a game each second)
	void invalidate() { InvalidateRect(hwnd_, nullptr, FALSE); }
	void update_animation_timer();
	void prepare_glows();
	double now_s() const;
	CellView cell_view(int sq) const;
	std::vector<int> animated_cells() const;
	void log(const std::wstring &line);
	void set_status(const std::wstring &s);
	void set_window_icon();
	void set_language(const std::string &code);

	// ---- app_input.cpp
	void on_key(WPARAM vk);
	void on_char(wchar_t c);
	void on_mouse_down(int x, int y, bool right, bool dbl);
	void on_mouse_move(int x, int y);
	void on_mouse_up(int x, int y);
	void on_wheel(int x, int y, int delta);
	void on_command(int id);
	void do_action(Action a);
	void position_changed();
	void copy_moves();
	void paste_moves();
	bool paste_text(const std::string &utf8);  // moves, a board or bitboards; false when none
	void commit_edit();
	void set_loss(int x, int y);
	std::vector<const MoveEval *> sorted_moves(const PosInfo &info) const;

	// ---- app_engine.cpp
	void start_engines();
	void stop_engines();
	void restart_engines();
	void on_engine(WPARAM tag, bool ended);
	void request_analysis();
	void request_levels();
	void pump_graph();
	void pump_search();
	void request_missing(std::string &out, const Position &p);
	int missing_target() const;
	void switch_book(const std::wstring &path);
	void export_missing();
	void export_best();
	// app_settings.cpp
	void open_range_settings();
	void open_learn_settings();
	std::string learn_options_commands() const;
	// app_learn.cpp
	std::wstring book_file() const;
	void learn_command(int id);
	void start_learning(const std::string &command);
	void start_learning_at(const std::string &command, const Position &start, const std::string &moves);
	void queue_missing();
	void next_in_queue();
	void watch_engines();
	void stop_learning();
	void continue_learning();
	void learning_line(const std::string &line);
	void learning_ended();
	void forget_engine_state(int role);  // after "stop": the position and level sent before are unknown
	void send_hint(const Position &p, bool background);
	void compute_line();
	std::string book_params() const;
	std::wstring engine_dir() const { return engine_dir(kind_); }
	std::wstring engine_name() const;
	std::wstring engine_exe() const { return engine_exe(kind_); }  // empty when there is none
	std::wstring engine_eval() const { return engine_eval(kind_); }
	std::wstring engine_args() const { return engine_args(kind_); }  // eval file, threads, level, hash
	std::wstring engine_dir(EngineKind k) const;
	std::wstring engine_exe(EngineKind k) const;
	std::wstring engine_eval(EngineKind k) const;
	std::wstring engine_args(EngineKind k) const;
	void send(int role, const std::string &text);

	// ---- app_tools.cpp: the tool engine, the book commands of the menus, the base commands, file dialogs
	bool start_tool(const std::string &commands, const std::wstring &label);
	void tool_line(const std::string &line);
	void tool_ended(bool stopped);
	void stop_tool();
	void tool_command(int id);
	void base_command(int id);
	bool book_job(int id, std::string *command, std::wstring *warning);  // the command of a book menu item (false: nothing to do)
	std::wstring pick_open(const wchar_t *filter, const std::wstring &title = {});
	std::wstring pick_save(const wchar_t *filter, const wchar_t *ext, const std::wstring &name, const std::wstring &title = {});
	// a name for the engine (role 0 main, 2 tools); a name the engine cannot take goes through the temporary
	// folder: a file read is copied there, a file written is moved to its place when the job ends
	enum class FileUse { Read, Write, Rewrite };
	std::optional<std::string> engine_path_or_say(const std::wstring &path, FileUse use, int role);
	void finish_moves(int role);
	struct PendingMove {
		std::wstring from, to;
		int role;
	};
	std::vector<PendingMove> moves_after_job_;
	int temp_files_ = 0;
	void open_log();
	bool save_log(const std::wstring &path) const;

	// ---- app_board.cpp: orientation, board setup, the line of a position, bitboards, images
	void orient_to(int s);
	void begin_edit();
	void end_edit(bool keep);
	void edit_click(int sq, bool right);
	bool edit_key(WPARAM vk);
	void estimate_line();
	void findline_answer(const Fields &f, bool end);
	void send_findline();
	void estimate_without_book(const Position &target);
	void show_found_line(const Game &g, int sym);
	void copy_position(int id);
	void import_position();
	bool take_position_text(const std::string &text);
	void set_game(const Game &g, bool keep_old);
	void screenshot(const std::wstring &file = {});
	void save_board_image();
	void copy_text(const std::wstring &text);
	std::wstring clipboard_text();
	int move_number_at(int sq) const;

	// ---- app_list.cpp: the game list, the search, the kept lines, background jobs
	void run_task(const std::wstring &label, std::function<std::function<void()>(std::stop_token)> work);
	void task_ended();
	void cancel_task();
	void list_command(int id);
	void list_load(const std::wstring &file, bool add);
	void list_select(int index, bool from_hit = false, int hit = -1);
	void list_search();
	void keep_now(bool quiet = false);
	void kept_command(int id);
	void tree_sync();
	Position tree_position(const std::vector<int> &path) const;
	bool tree_visible(const std::vector<int> &path) const;
	std::optional<Position> tree_next_wanted();
	bool tree_waiting() const;
	void tree_click(const TreeRow &row, bool on_arrow);
	void tree_go(const std::vector<int> &path);
	bool tree_command(const std::vector<std::string> &w, const std::string &rest);
	void load_kept();
	void save_kept() const;
	void list_from_book();
	void list_book_written(bool add, bool ok);
	bool ask_book_lines(int loss, int depth, int n, bool add);
	void list_job_script(const std::wstring &label, std::function<void(GameList &)> work);

	// ---- app_play.cpp: games against the computer, the compare mode
	void set_play_mode(PlayMode m, int games = 0);
	bool com_turn() const;
	void com_think();
	void com_timer();
	void com_stop();
	void com_played(int sq);
	void play_line(const std::string &line);
	bool start_play_engine();
	void clock_switch();
	void clock_tick();
	void game_end_check();
	void play_command(int id);
	bool take_back();
	EngineKind other_kind() const { return kind_ == EngineKind::Newedax ? EngineKind::Nikque18 : EngineKind::Newedax; }
	void set_compare(bool on);
	void compare_request();
	void compare_line(const std::string &line);

	// ---- app_script.cpp: the command scripts
	void run_script_file(const std::wstring &file, int depth);
	void pump_script();
	void schedule_script();  // pump_script later, from the message loop (never inside the reading of an engine's answers)
	bool script_command(const std::string &line, int depth, std::wstring *error);
	bool script_waiting() const;
};

// drawing (draw_board.cpp, draw_panel.cpp)
void draw_board(App &app, const D2D1_RECT_F *only_cell = nullptr);
void draw_cell(App &app, int sq, double t);
void draw_bar(App &app);
void draw_panel(App &app);
void draw_log(App &app);
// app_play.cpp
bool play_timer(App &app, UINT_PTR id);
std::wstring play_bar_text(const App &app);

}  // namespace ng
