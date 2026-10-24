nikque GUI
==========

[日本語] [English] [简体中文] [繁體中文] の順に書いています。
中国語（簡体字・繁体字）の文と画面の言葉は、機械的に訳したものです（確かめていません）。
The Chinese texts here and on the screen are machine translations (not checked).
本说明和界面中的中文为机器翻译（未经确认）。
本說明和介面中的中文為機器翻譯（未經確認）。


========================================================================
[日本語]
========================================================================

1. nikque GUI とは
------------------
Edax v4.5.5-nikque と newedax のための GUI です。盤の上に全部の合法手の評価値と
book の精度（段階 0〜7）を出し、book の解析（足りない leaf、うみがめ数、level ごとの数、
最善の進行）と book の学習・整理を画面から行えます。
GUI は MIT ライセンスです。エンジン（Edax nikque・newedax）は GPL で、別のプログラムとして
動きます（GUI はエンジンを子のプロセスとして起動し、文字のコマンドでやりとりします）。

2. 置き方
---------
- 配布の ZIP を好きな場所に展開し、nikque-gui.exe を起動します（インストールは要りません）。
  nikque-gui.exe と同じフォルダに config.ini・lang フォルダ・readme.txt があります。
- エンジンが付いています：engines\nikque に C 版の Edax v4.5.5-nikque.22、engines\newedax に
  newedax.7（どちらも wEdax-x86-64-v4.exe・-v3.exe・wEdax-x86-64.exe と data\eval.dat）。
  0.2.0 までの config.ini（系の名前 [nikque18]）も、そのまま読めます。
- すでにあるエンジンのフォルダを使うときは、config.ini の [nikque]・[newedax] の dir= で指します。
  GUI には book の解析のコマンド（book moveinfo など）が要るので、v4.5.5-nikque.19・newedax.4 以降を
  使います。古いバージョンでは上の帯に「book の解析なし（古いエンジン）」と出て、book の値・木・book タブは
  出ません。book を軽くする機能（book reduce）は v4.5.5-nikque.20・newedax.5 からです。
  exe= を auto にすると、CPU に合わせて wEdax-x86-64-v4.exe・-v3.exe・wEdax-x86-64.exe から選びます。
- book は付いていません。お持ちの book.dat をエンジンの data\book.dat に置くか、book= でファイルを
  指します。book はエンジンが読みます（GUI は book のファイルを読みません）。book が無いときは、
  エンジンが data\book.dat に初期局面だけの小さな book を作ります（後でお持ちの book で置き換えて
  かまいません）。
- 画面の言葉は「設定」→「言語」で選びます（日本語・English・简体中文・繁體中文）。
- 詳しい説明は manual\ja\README.md（ユーザーマニュアル）にあります。

3. 画面
-------
- 盤：全部の合法手に評価値。色と効果は book の精度の段階（上の帯の玉に乗せると意味）。
  手にマウスを乗せたときの読み筋は、初めはいちばん下の行に文字で出ます（「表示」→「マウスを乗せた手の
  読み筋を盤に表示する」を入れると、盤に半透明の石と番号でも出ます）。
- 右のパネルのタブ：候補手（並べ替えのできる表）・book（level ごとの数、最善の進行、
  足りない進行）・棋譜・リスト（棋譜リストと検索の結果）・木（book の進行）。
- その下に、しおりの列（しおりを挟んだ局面の盤の絵。押すとその局面へ、右クリックでメニュー）と、
  評価値のグラフがいつも出ます。
- ログ：下の 1 行。押すと広がります。「表示」→「ログを保存」でファイルにできます。

4. キー（既定。config.ini の [keys] で全部変えられます）
---------------------------------------------------------
  ← →            1 手戻る・進む（右クリックでも 1 手戻る）
  Ctrl+← →       10 手
  N・Home / End  最初・最後の局面
  ↑ ↓ / Enter・Space  候補手を選ぶ / 打つ
  B / U / H      book を使う・うみがめ数・評価値を隠す
  R              盤の向き（8 通りを順に。「表示」→「盤の向き」で選べる）
  D              デザインを切り替える（標準 ⇄ 肉球）
  M              今の局面にしおりを挟む
  E              盤面配置を始める・決める（配置中：左クリックで黒→白→空き、右クリックで空き、
                 T で手番、Delete で全部消す、I で初期配置、Enter で決める、Esc でやめる）
  Ctrl+C / Ctrl+V  棋譜をコピー / 貼り付け（2 回押す。座標の列、盤面の文字列、ビットボード、
                 ブログの PARAM の形を読む）
  Ctrl+P         スクリーンショット（exe の隣の screenshots フォルダ）
  Ctrl+Shift+← → 棋譜リストの前・次の棋譜
  Ctrl+Shift+↑ ↓ 棋譜リストの今の棋譜を消す・読み込み直す
  P / L          右のパネル・ログ欄

5. 主な機能（メニュー）
-----------------------
- 解析：評価値、評価の Lv（0〜60 をスライダーで選ぶ）、棋譜の局面を裏で読む順（後ろから解析・前から解析・
  読まない）、book の精度を計算する（切ると book の評価値だけ。大きな book で重いとき）、うみがめ数、
  book の局面数、比較モード、調べる範囲。
- book：使う・読み込み直す・別の book、精度の基準、book の中身をログに表示（info・stats・show）、
  足りない進行・最善の進行の書き出し、book の整理（保存・別名で保存・新しい空の book・深さ・
  merge・subtree・prune・reduce・export・import・extract・problem）。
  book reduce（book を軽くする）は、指定の局面（ふつうは初期局面）から book deviate3 X Y でたどれない
  局面を消します。先に残る局面と消える局面の数を数えて見せてから行います。
- 学習：deviate・deviate2・deviate3・enhance・leaf expand・fill・fill2・play・store・add・learn・
  check・negamax・fix・correct・leaf recalculate、中止、途中保存から再開、学習キュー、学習の設定。
  book を変えるコマンド（subtree・prune・reduce・import・new など）の後は、エンジンを閉じるときに
  book のファイルが上書きされることがあります。先にコピーを取ってください。
- エンジン：nikque 系（C 版）・newedax 系（C++ 版）の切り替え、solve・bench・count・perft・estimate
  （book を読まない「ツール用エンジン」を 1 回ごとに起動）、コマンドの入力（メインエンジンに送る）。
- 入出力：棋譜のコピー・貼り付け・固定、局面の文字列とビットボード（10 進・16 進・Ntest の形式・obf）
  のコピーと取り込み、盤面配置、盤から手順の推定（20 手目まで。book にある手順を先に探し、
  無ければ book を使わずに探す）、スクリーンショット、盤の図（PNG）、棋譜リスト、棋譜検索、
  裏の処理を止める（棋譜リストの処理・精度の計算・棋譜の裏読み。局面を動かすとまた始まる）、
  棋譜のファイル（base の convert・unique・compare・check・correct・complete・problem・tofen）、スクリプト。
- 表示：デザインを切り替える（D）、デザイン（標準・肉球）、効果（入・控えめ・切、いちばん上の段（段階 7）の
  強さ）、盤の向き、手の番号、読み筋を盤に表示する、マスの左下に表示する数（book の局面数・Lv・
  精度の段階・うみがめ数）、タブ、しおり、パネル、ログ。
- しおり：M で今の局面にしおりを挟みます。新しい対局・棋譜の貼り付け・盤面配置・手順の推定のときも、
  それまでの棋譜に自動でしおりが挟まれます（数の制限なし、exe の隣の kept.txt）。グラフの上の
  しおりの列で絵を押すと、その局面へ行きます。右クリックで「選んだしおりの棋譜と比べる」「消す」など。
- 対局：人どうし（自由打ち）・人が黒・人が白・コンピューターどうし（続けて打つ対局の数）。コンピューターは
  book を読まない「対局用エンジン」で考えるので、その間も盤の解析は続きます。待ったは人の番まで戻ります。
  コンピューターの Lv、持ち時間（分。時間があるときは残りの時間で 1 手の時間を決め、切れた側の負け）、
  相手の番にも考える、終局したときの動作（しおりを挟む・棋譜リストに追加）。上の帯に対局の種類と残りの時間。
  評価値は H で隠せます。
- 序盤の打ち方：book を使わない・book の最善の手・book から幅をもって選ぶ（最善からの石損の幅と、
  book を使う手数）。book の手は Edax と同じく、局面のリンクと leaf から選びます。
- 木の表示（「book」→「木を表示する」、または木タブ）：今の局面から先の book の手（リンク）を、開け閉めできる木で
  並べます（leaf は開けない薄い行）。各行に打つ側の石・評価値（打つ側から見た book の値）・精度の段階・うみがめ数・Lv。
  行を押すとその局面へ進んでその段を開き、▸ / ▾ で開く・閉じる。盤で根より先へ進む間は木がそのままで、
  今の局面の行に色が付きます。根より前に戻るか別の棋譜にすると、そこが新しい根になります。
- 比較モード（「解析」→「比較モード」）：もう一方の系のエンジンを book なしの探索だけで
  起動し、候補手の表にその評価値の列を足します。

ビットボードの形式：10 進・16 進は「黒 白 手番」（a1 が 0 ビット目、h8 が 63 ビット目。手番は X か O）。
Ntest の形式は「空き 手番の側の石 手番」。

6. 棋譜リスト
-------------
1 行に 1 局のテキストファイル（"f5d6c3..."、または "<64 マス> <X|O>;手順" で最初の局面から）。
読み込む・追加する・保存・並べ替え（短い順、同じ長さは手順の順）・同じ棋譜をまとめる・短くする・並びを混ぜる（乱数の種）・
向きをそろえる（初手を f5 に、対称な局面でも決まった向きに）・向きをばらばらにする（対称な局面で乱数）・
消す・何手目から表示するか・book の進行をリストへ。重い処理は裏で行い、「裏の処理を止める」で止められます。
向きをそろえる・ばらばらにするを途中で止めると、それまでに向きを変えた棋譜は変えたまま残ります（同じ棋譜の別の向き）。
棋譜検索は、今の局面（向きの違うものも含む）が現れる棋譜を探し、行を押すとその局面を開きます。

7. スクリプト
-------------
テキストファイル（UTF-8）に 1 行 1 つのコマンドを書き、「入出力」→「スクリプトを実行」で順に実行します。
起動のときに実行するスクリプトは config.ini の [script] startup= に書きます。# で始まる行は注です。
処理を始めるコマンド（list load、search、learn start、edax、tool、estimate、wait、book reduce など）は、
終わるまで次の行を待ちます。相対の名前はスクリプトのファイルのフォルダから。

  moves <棋譜>             棋譜を読み込む（座標の列・盤面の文字列・ビットボード）
  board <盤面の文字列>     その局面から始める
  play <手>                今の局面から打つ
  new / goto <n|end> / back [n] / forward [n]
  book on|off / eval on|off / numbers on|off / level <0-60> / umi best|loss|off
  precision on|off         book の精度を計算する・しない
  analysis backward|forward|off  棋譜の局面を裏で読む順（後ろから解析・前から解析・読まない）
  stop                     裏の処理を止める
  pv on|off                マウスを乗せた手の読み筋を盤に表示する・しない
  book reload              book を読み込み直す（メインエンジンを再起動）
  book reduce <X> <Y> [here]  book を軽くする（初期局面から。here は今の局面から）。数えてから、確かめずに行う
  engine nikque|newedax / design standard|paw|toggle / effects on|subdued|off
  tab moves|book|record|list|tree / rotate <0-7> / cellinfo none|positions|level|stage|umi
  lang ja|en|zh-Hans|zh-Hant
  wait [秒]                今の局面の解析が終わるまで待つ（既定 30 秒まで。時間で終わったときはログに出る）
  estimate                 盤から手順を推定する
  keep                     今の局面にしおりを挟む
  bookmark <n>             n 番のしおりの局面へ / bookmarks on|off  しおりの列を表示する・しない
  screenshot [ファイル]    画面を PNG に
  savelog <ファイル>       ログを保存する
  list load|add|save <ファイル>
  list sort|unique|normalize|delete|next|prev|addgame|clear
  list truncate <n> / list shuffle [種] / list scatter black|white|both [種]
  list start <n> / list select <n> / list book [石損] [深さ] [本数] [add]
  search [exact]           棋譜検索（exact は向きの違うものを含めない）
  learn <book のコマンド>  学習キューに入れる（今の局面から） / learn start で実行する
  edax <コマンド>          メインエンジンに送り、終わるまで待つ
  tool <コマンド>          ツール用エンジンで実行する（solve・bench・count・perft・estimate・base）
  run <ファイル>           別のスクリプト（4 段まで）
  game free|black|white|engines [数]  対局を始める（black は人が黒）
  comlevel <n> / clock <分> / ponder on|off / opening search|best|wide [幅] [手数]
  go                       コンピューターに 1 手打たせる / takeback  待った
  waitgame [秒]            対局が終わるまで待つ（コンピューターどうしは全部の対局。時間で終わったときはログに出る）
  compare on|off           比較モード
  tree open|close|go <手順>  木の段を開く・閉じる・その局面へ（手順は根から） / tree root  今の局面を根に
  tree toggle <手順>       行の ▸ ／ ▾ と同じ（その段だけを開く・閉じる）
  log <文字> / quit

8. config.ini の主な項目
------------------------
[engine] kind=nikque|newedax、search=1（探索用エンジンを使う）
[nikque]・[newedax] dir・exe・eval・book
[analysis] level（0〜60）・threads（0 は全部）・hash（ハッシュ表のビット数、0 はエンジンのまま）・umi・x・y・base・stages・
           precision（1 で精度を計算する）・backward・forward（棋譜の局面を裏で読む順）・compare
[play] level・opening（search|best|wide）・width・until・clock・ponder・end_keep・end_list
[view] design・effects・language・numbers・pv（1 で読み筋を盤に表示）・cell_info・fps・font・opening_file・screenshot_dir
[window] 大きさと場所、tab、panel_w、bookmarks（1 でしおりの列を表示）
[list] start_ply（棋譜リストの棋譜を開く手数、-1 は最後）
[learn_options] エンジンに送る学習の設定
[script] startup（起動のときのスクリプト）
[keys] キーの割り当て（例：back=Left, Backspace）

9. ファイル
-----------
kept.txt（しおり）、screenshots\（スクリーンショット）は exe の隣に作ります。
定石名は exe の隣の opening.ini を読みます（付いています）。テキストファイルで、1 行に「名前=手順」
（手順は初手を f5 として書く。行の頭の空白は段を見せるだけ、// で始まる行は注釈）。メモ帳などで直せます。

10. ライセンス
--------------
nikque GUI：MIT License（LICENSE.txt）。エンジン（Edax nikque・newedax）：GNU GPL v3（別のプログラム）。
付いているエンジンの GPL の文とソースの場所は、engines\nikque・engines\newedax の LICENSE.txt と
SOURCE.txt にあります。


========================================================================
[English]
========================================================================

1. What it is
-------------
A GUI for Edax v4.5.5-nikque and newedax. The values of all the legal moves and the accuracy stage of the
book (0..7) are shown on the board; the book analyses (missing leaves, umigame numbers, positions by level,
best lines), book learning and book maintenance are run from the menus.
The GUI is under the MIT License. The engines (Edax nikque, newedax) are GPL and run as separate programs
(the GUI starts an engine as a child process and talks to it with text commands).

2. Setting up
-------------
- Unpack the ZIP anywhere and start nikque-gui.exe (no installation). config.ini, the lang folder and
  readme.txt are next to nikque-gui.exe.
- The engines are included: engines\nikque holds Edax v4.5.5-nikque.22 (C version), engines\newedax holds
  newedax.7 (each with wEdax-x86-64-v4.exe, -v3.exe, wEdax-x86-64.exe and data\eval.dat). A config.ini of 0.2.0 or
  before (section [nikque18]) is read as it is.
- To use engine folders you already have, point to them with dir= in [nikque] and [newedax] of config.ini.
  The GUI needs the book analyses (book moveinfo and the others) of v4.5.5-nikque.19 or newedax.4 or later:
  with an older engine the top bar says "No book analyses (old engine)" and the book values, the tree and
  the Book tab stay empty. Making the book lighter (book reduce) needs v4.5.5-nikque.20 or newedax.5.
  With exe=auto the engine file is chosen for the CPU (wEdax-x86-64-v4.exe, -v3.exe, wEdax-x86-64.exe).
- No book is included. Put your book.dat in data\book.dat of the engine, or point to it with book=.
  The engine reads the book (the GUI never reads the file). Without a book the engine makes a small new book
  (the initial position only) in data\book.dat; replace it with your book later.
- The language is chosen in Settings → Language.
- The user manual is manual\en\README.md.

3. The screen
-------------
- Board: the values of all the legal moves; colors and effects show the accuracy stage of the book
  (the dots in the top bar explain them). The expected line of the move under the mouse is written in the
  bottom row (with View → Show on the board the expected line of the move under the mouse, on the board too).
- Tabs of the right panel: Moves (a sortable table), Book (positions by level, best lines, missing lines),
  Record, List (the game list and search results), Tree (book lines).
- Below them, always: the row of bookmarks (pictures of the bookmarked positions; click one to go there,
  right click for its menu) and the graph of values.
- Log: the bottom row; click to open it. View → Save the log writes it to a file.

4. Keys (default; all can be changed in [keys] of config.ini)
-------------------------------------------------------------
  Left / Right       one move back / forward (a right click also goes back)
  Ctrl+Left/Right    10 moves
  N, Home / End      first / last position
  Up, Down / Enter, Space   choose a move / play it
  B / U / H          book on/off, umigame numbers, hide the values
  R                  board orientation (8 in turn; View → Board orientation to choose)
  D                  switch the design (standard / paw)
  M                  bookmark the position now
  E                  start or finish setting up a position (left click black→white→empty, right click empty,
                     T side to move, Delete clear, I start position, Enter done, Esc cancel)
  Ctrl+C / Ctrl+V    copy / paste a game (press twice; coordinates, board text, bitboards, blog PARAM)
  Ctrl+P             screenshot (screenshots folder next to the exe)
  Ctrl+Shift+Left/Right  previous / next game of the list
  Ctrl+Shift+Up/Down     remove / read again the game of the list
  P / L              right panel / log

5. Main features (menus)
------------------------
- Analysis: values, search level (0 to 60 on a slider), the order of the search of the game in the background
  (backward analysis, forward analysis, none), compute the book accuracy (off: the book values only, for a heavy
  book), umigame numbers, book positions, compare mode, the range.
- Book: use, read again, other books, accuracy base, book contents to the log (info, stats, show), write
  missing or best lines, maintenance (save, save as, new empty book, depth, merge, subtree, prune, reduce,
  export, import, extract, problem). book reduce (making the book lighter) removes the positions that
  book deviate3 X Y cannot reach from a position (usually the initial one); the numbers of positions kept and
  removed are counted and shown first.
- Learning: deviate, deviate2, deviate3, enhance, leaf expand, fill, fill2, play, store, add, learn, check,
  negamax, fix, correct, leaf recalculate; stop; continue from the file saved on the way; the learning queue;
  settings. After a command that changes the book (subtree, prune, reduce, import, new...), the book file may
  be overwritten when the engine is closed. Copy it first.
- Engine: nikque (C version) or newedax (C++ version); solve, bench, count, perft, estimate (on a "tool engine"
  started for each job, without the book); command box (to the main engine).
- Input/Output: copy, paste and fix games; position text and bitboards (decimal, hexadecimal, Ntest form, obf);
  setting up a position; finding the moves of a position (up to move 20: the book first, else without it);
  screenshot; board image (PNG); game list; search; stop the background work (the game list, the book accuracy,
  the search of the game; it starts again when the position changes); files of games (base convert, unique,
  compare, check, correct, complete, problem, tofen); command scripts.
- View: switch the design (D), design (standard, paw), effects (on, subdued, off; the strength of the top stage),
  orientation, move numbers, the expected line on the board, number on the squares (book positions, Lv,
  accuracy stage or umigame numbers at the lower left), tabs, bookmarks, panel, log.
- Bookmarks: M bookmarks the position now. A new game, a pasted game, a position set up and a line found also
  bookmark the game before (no limit; kept.txt next to the exe). Click a picture in the row of bookmarks above
  the graph to go to its position; a right click opens a menu (compare with its game, delete...).
- Games: two people (free), you as black, you as white, the computer on both sides (a number of games). The
  computer thinks on a "play engine" without the book, so the analysis of the board goes on. Take back goes to
  your turn. The computer's Lv, the time (minutes; the time of a move comes from the time left, and the side
  whose time runs out loses), thinking on your turn, what to do at the end (bookmark the game, add it to the list).
  The top bar shows the kind of game and the time left. H hides the values.
- Opening moves: no book, the best book move, a book move drawn within a width (the loss from the best, and
  up to which move). As Edax, the book moves are the links and the leaf of the position.
- Tree of the book (Book → Tree of the book, or the Tree tab): the book moves (links) from the position shown, in a
  tree that opens and closes (the leaf as a faint row that does not open). Each row: the disc of the side that plays,
  the book value from that side, the accuracy stage, umigame numbers, Lv. Click a row to go to its position and open
  it; ▸ / ▾ opens or closes. While you go on from the root the tree stays and the row of the position is colored;
  going back before the root or to another game makes a new root.
- Compare mode (Analysis → Compare mode): the engine of the other kind runs a search without
  the book, and the Moves table gets a column of its values.

Bitboards: decimal and hexadecimal are "black white side" (a1 is bit 0, h8 bit 63; side X or O).
The Ntest form is "empty-squares discs-of-the-side-to-move side".

6. Game list
------------
A text file with one game a line ("f5d6c3...", or "<64 squares> <X|O>;moves" from another start).
Open, add, save, sort (shorter first, then by the moves), join the same games, cut, shuffle (seed), one
orientation (first move f5, also at later symmetric positions), random orientations, remove, the move to
open at, book lines into the list. Heavy work runs in the background; "Stop the background work" stops it.
Stopping one orientation or random orientations half way leaves the games turned so far as they are (the same
games turned another way).
The search finds the games where this position appears (other orientations too); click a line to open it.

7. Command scripts
------------------
A UTF-8 text file with one command a line, run with Input/Output → Run a command script. The script run at
the start is set in [script] startup= of config.ini. Lines starting with # are notes. A command that starts
work waits for its end. Relative names are from the script's folder. The commands are listed in the
Japanese part above (moves, board, play, new, goto, back, forward, book, book reload, book reduce, eval,
numbers, level, umi, precision, analysis, stop, pv, engine, design, effects, tab, rotate, cellinfo, lang, wait,
estimate, keep, bookmark, bookmarks, screenshot, savelog, list ..., search, learn, edax, tool, run, game,
comlevel, clock, ponder, opening, go, takeback, waitgame, compare, tree, log, quit).
A wait or waitgame that ends by its time says so in the log.

8. config.ini
-------------
See the Japanese part (section 8) for the keys; their names are the same.

9. Files
--------
kept.txt (bookmarks) and screenshots\ are made next to the exe. Opening names are read from opening.ini
next to the exe (included): a text file of lines name=moves (moves from the start, f5 first;
leading spaces only show the levels, lines starting with // are notes). It can be edited with Notepad.

10. License
-----------
nikque GUI: MIT License (LICENSE.txt). Engines (Edax nikque, newedax): GNU GPL v3 (separate programs).
The GPL text and the place of the source of the included engines are in LICENSE.txt and SOURCE.txt of
engines\nikque and engines\newedax.


========================================================================
[简体中文]（机器翻译）
========================================================================

1. 简介
-------
用于 Edax v4.5.5-nikque 与 newedax 的 GUI。在棋盘上显示所有合法着法的评价值与 book 的精度（阶段 0〜7），
并可从菜单进行 book 分析（不足的 leaf、海龟数、各 level 的局面数、最佳进行）与 book 的学习和整理。
GUI 为 MIT 许可。引擎（Edax nikque、newedax）为 GPL，作为独立程序运行。

2. 放置
-------
- 将 ZIP 解压到任意位置，启动 nikque-gui.exe（无需安装）。config.ini、lang 文件夹、readme.txt 在 nikque-gui.exe 旁。
- 附带引擎：engines\nikque 为 C 版 Edax v4.5.5-nikque.22，engines\newedax 为 newedax.7
  （各含 wEdax-x86-64-v4.exe、-v3.exe、wEdax-x86-64.exe 与 data\eval.dat）。0.2.0 以前的 config.ini（[nikque18]）也可直接读取。
- 使用已有的引擎文件夹时，在 config.ini 的 [nikque]、[newedax] 的 dir= 指定。GUI 需要 v4.5.5-nikque.19、newedax.4 以后的
  book 分析命令（book moveinfo 等）；旧版本时上方栏显示「无 book 分析（旧引擎）」，不显示 book 值、树与 book 标签页。
  减小 book 的功能（book reduce）需要 v4.5.5-nikque.20、newedax.5 以后。
- 不附带 book。请把您的 book.dat 放在引擎的 data\book.dat，或用 book= 指定。book 由引擎读取（GUI 不读取 book 文件）。
  没有 book 时，引擎会在 data\book.dat 建立只含初始局面的小 book（之后可用您的 book 替换）。
- 语言在「设置」→「语言」中选择。

3. 按键
-------
← →：后退/前进一步（右键也后退）；Ctrl+← →：10 手；R：棋盘方向；D：切换设计；M：为当前局面添加书签；E：摆放局面；
Ctrl+C / Ctrl+V：复制/粘贴棋谱；Ctrl+P：截图；Ctrl+Shift+← →：棋谱列表的上/下一个棋谱。
所有按键可在 config.ini 的 [keys] 中更改。

4. 功能
-------
book 的整理与导出（包括 book reduce：删除从指定局面按 book deviate3 X Y 无法到达的局面以减小 book，执行前先统计并显示保留与删除的局面数）、
学习的命令、工具（solve、bench、count、perft、estimate、base）、棋谱列表与检索、
书签（图表上方的书签栏显示局面缩略图，点击即可转到该局面）、摆放局面、从棋盘推断手顺（到第 20 手）、棋盘方向、手顺编号、
格子上的数字、位棋盘、截图、脚本。
分析：评估的 Lv（0〜60，用滑块选择）、在后台分析棋谱局面的顺序（逆向、正向、不分析）、可关闭 book 精度的计算、停止后台处理。
对局（人对电脑、电脑对电脑、悔棋、电脑的 Lv、持有时间、序盘从 book 选择）与比较模式（另一引擎的评价值列）。
树状显示（「book」→「树状显示」）：从当前局面起的 book 进行，可展开・收起。
脚本的命令请参见日文部分第 7 节（命令名相同）。
改变 book 的命令（subtree、prune、reduce、import、new 等）之后，关闭引擎时 book 文件可能被覆盖。请先备份。

5. 许可
-------
nikque GUI：MIT License（LICENSE.txt）。引擎（Edax nikque、newedax）：GNU GPL v3（独立程序）。
附带引擎的 GPL 文本与源代码位置见 engines\nikque、engines\newedax 的 LICENSE.txt 与 SOURCE.txt。


========================================================================
[繁體中文]（機器翻譯）
========================================================================

1. 簡介
-------
用於 Edax v4.5.5-nikque 與 newedax 的 GUI。在棋盤上顯示所有合法著法的評價值與 book 的精度（階段 0〜7），
並可從選單進行 book 分析（不足的 leaf、海龜數、各 level 的局面數、最佳進行）與 book 的學習和整理。
GUI 為 MIT 授權。引擎（Edax nikque、newedax）為 GPL，作為獨立程式執行。

2. 放置
-------
- 將 ZIP 解壓到任意位置，啟動 nikque-gui.exe（無需安裝）。config.ini、lang 資料夾、readme.txt 在 nikque-gui.exe 旁。
- 附帶引擎：engines\nikque 為 C 版 Edax v4.5.5-nikque.22，engines\newedax 為 newedax.7
  （各含 wEdax-x86-64-v4.exe、-v3.exe、wEdax-x86-64.exe 與 data\eval.dat）。0.2.0 以前的 config.ini（[nikque18]）也可直接讀取。
- 使用已有的引擎資料夾時，在 config.ini 的 [nikque]、[newedax] 的 dir= 指定。GUI 需要 v4.5.5-nikque.19、newedax.4 以後的
  book 分析命令（book moveinfo 等）；舊版本時上方列顯示「無 book 分析（舊引擎）」，不顯示 book 值、樹與 book 分頁。
  減小 book 的功能（book reduce）需要 v4.5.5-nikque.20、newedax.5 以後。
- 不附帶 book。請把您的 book.dat 放在引擎的 data\book.dat，或用 book= 指定。book 由引擎讀取（GUI 不讀取 book 檔案）。
  沒有 book 時，引擎會在 data\book.dat 建立只含初始局面的小 book（之後可用您的 book 替換）。
- 語言在「設定」→「語言」中選擇。

3. 按鍵
-------
← →：後退/前進一步（右鍵也後退）；Ctrl+← →：10 手；R：棋盤方向；D：切換設計；M：為目前局面加入書籤；E：擺放局面；
Ctrl+C / Ctrl+V：複製/貼上棋譜；Ctrl+P：螢幕截圖；Ctrl+Shift+← →：棋譜清單的上/下一個棋譜。
所有按鍵可在 config.ini 的 [keys] 中變更。

4. 功能
-------
book 的整理與匯出（包括 book reduce：刪除從指定局面按 book deviate3 X Y 無法到達的局面以減小 book，執行前先統計並顯示保留與刪除的局面數）、
學習的命令、工具（solve、bench、count、perft、estimate、base）、棋譜清單與檢索、
書籤（圖表上方的書籤列顯示局面縮圖，點擊即可轉到該局面）、擺放局面、從棋盤推斷手順（到第 20 手）、棋盤方向、手順編號、
格子上的數字、位元棋盤、螢幕截圖、腳本。
分析：評估的 Lv（0〜60，用滑桿選擇）、在背景分析棋譜局面的順序（逆向、正向、不分析）、可關閉 book 精度的計算、停止背景處理。
對局（人對電腦、電腦對電腦、悔棋、電腦的 Lv、持有時間、序盤從 book 選擇）與比較模式（另一引擎的評價值欄）。
樹狀顯示（「book」→「樹狀顯示」）：從目前局面起的 book 進行，可展開・收合。
腳本的命令請參見日文部分第 7 節（命令名相同）。
改變 book 的命令（subtree、prune、reduce、import、new 等）之後，關閉引擎時 book 檔案可能被覆寫。請先備份。

5. 授權
-------
nikque GUI：MIT License（LICENSE.txt）。引擎（Edax nikque、newedax）：GNU GPL v3（獨立程式）。
附帶引擎的 GPL 文字與原始碼位置見 engines\nikque、engines\newedax 的 LICENSE.txt 與 SOURCE.txt。
