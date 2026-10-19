// The main window: menu, the information bar, the board, the right panel (tabs, the range row, the table of
// moves, the graph) and the log. Split over app.cpp (window, layout, drawing), app_input.cpp (keys, mouse,
// menu commands) and app_engine.cpp (the engines and their answers).
#pragma once

#include <windows.h>

#include <chrono>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "analysis.h"
#include "engine.h"
#include "game.h"
#include "gfx.h"
#include "ini.h"
#include "keys.h"
#include "openings.h"
#include "theme.h"

namespace ng {

inline constexpr UINT WM_ENGINE = WM_APP + 1;

// menu commands
namespace cmd {
enum : int {
	NewGame = 1000,
	Undo,
	Redo,
	ToStart,
	ToEnd,
	ShowEval = 1100,
	Recompute,
	Backward,
	UmiBest,
	UmiLoss,
	UmiOff,
	CountOn,
	Level0 = 1150,  // + index into kLevels
	UseBook = 1200,
	ReloadBook,
	BaseDeviate,
	BaseDeviate2,
	BaseDeviate3,
	BookInfo,
	OpenBook,
	ExportMissing,
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
	LearnStop,
	LearnContinue,
	LearnSettings,
	EngineNikque = 1400,
	EngineNewedax,
	RestartEngines,
	EngineVersion,
	CopyMoves = 1500,
	PasteMoves,
	FixRecord,
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
	OpenConfig = 1700,
	RangeSettings,
	LangJa,
	HelpKeys = 1800,
	HelpAbout,
};
}
inline constexpr int kLevels[] = {1, 5, 10, 15, 18, 21, 24, 27, 30, 35, 40, 60};

enum class Effects : int { On = 0, Subdued = 1, Off = 2 };
// how much of its effects the top stage (7) shows
enum class TopEffect : int { Strong = 0, Normal = 1, Weak = 2 };
enum class Tab : int { Moves = 0, Book = 1, Record = 2, Count };
enum class EngineKind : int { Nikque18 = 0, Newedax = 1 };
enum class UmiMode : int { Best = 0, Loss = 1, Off = 2, Range = 3 };
enum class Column : int { Move, Value, Stage, Missing, Positions, Level, Umi, Count };

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
};

struct EngineSlot {
	EngineProcess proc;
	bool alive = false;
	bool loaded = false;               // the first answer came (the book is read)
	std::optional<PosKey> board;       // the position the engine has (setboard)
	unsigned generation = 0;           // counts the starts: messages of a process closed before are ignored
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
	enum Kind { Book, Count, Levels, Marker, Graph, Missing } kind;
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

	// engines: [0] main (reads the book), [1] search only (no book)
	EngineSlot eng_[2];
	int next_id_ = 1;
	std::unordered_map<int, Request> requests_;
	int book_id_ = 0, count_id_ = 0, levels_id_ = 0;   // running on the main engine
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
	// search engine: the hint in progress
	int marker_id_ = 0;
	bool hint_busy_ = false, hint_accept_ = false;
	PosKey hint_pos_{};
	int hint_level_ = 0;
	bool hint_background_ = false;  // the hint in progress is for another position of the game
	int hint_end_id_ = 0;            // the marker sent after the hint
	std::vector<LineEval> line_;    // computed with each full drawing
	int engine_level_[2] = {0, 0};

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
	void invalidate() { InvalidateRect(hwnd_, nullptr, FALSE); }
	void update_animation_timer();
	void prepare_glows();
	double now_s() const;
	CellView cell_view(int sq) const;
	std::vector<int> animated_cells() const;
	void log(const std::wstring &line);
	void set_status(const std::wstring &s);
	void set_window_icon();

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
	// app_settings.cpp
	void open_range_settings();
	void open_learn_settings();
	std::string learn_options_commands() const;
	// app_learn.cpp
	std::wstring book_file() const;
	void learn_command(int id);
	void start_learning(const std::string &command);
	void stop_learning();
	void continue_learning();
	void learning_line(const std::string &line);
	void learning_ended();
	void send_hint(const Position &p, bool background);
	void compute_line();
	std::string book_params() const;
	std::wstring engine_dir() const;
	std::wstring engine_name() const;
	void send(int role, const std::string &text);
};

// drawing (draw_board.cpp, draw_panel.cpp)
void draw_board(App &app, const D2D1_RECT_F *only_cell = nullptr);
void draw_cell(App &app, int sq, double t);
void draw_bar(App &app);
void draw_panel(App &app);
void draw_log(App &app);

}  // namespace ng
