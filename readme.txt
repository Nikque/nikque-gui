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
最善の進行）と book の学習を画面から行えます。
GUI は MIT ライセンスです。エンジン（Edax nikque・newedax）は GPL で、別のプログラムとして
動きます（GUI はエンジンを子のプロセスとして起動し、文字の命令でやりとりします）。

2. 置き方
---------
- nikque-gui.exe と同じフォルダに config.ini・lang フォルダ・readme.txt を置きます。
- エンジンは exe の隣の engines\nikque18、engines\newedax に置くか、config.ini の
  [nikque18]・[newedax] の dir= でフォルダを指します。exe= を auto にすると、CPU に合わせて
  wEdax-x86-64-v4.exe・-v3.exe・wEdax-x86-64.exe から選びます。
- book はエンジンの data\book.dat（または book= のファイル）をエンジンが読みます。
  GUI は book のファイルを読みません。
- 画面の言葉は「設定」→「言葉」で選びます（日本語・English・简体中文・繁體中文）。

3. 画面
-------
- 盤：全部の合法手に評価値。色と効果は book の精度の段階（上の帯の玉に乗せると意味）。
- 右のパネルのタブ：候補手（並べ替えのできる表）・book（level ごとの数、最善の進行、
  足りない進行）・棋譜・リスト（棋譜リストと検索の結果）・控え（控えた手順）。
  下には評価値のグラフがいつも出ます。
- ログ：下の 1 行。押すと広がります。「表示」→「ログを書き出す」でファイルにできます。

4. キー（既定。config.ini の [keys] で全部変えられます）
---------------------------------------------------------
  ← →            1 手戻る・進む（右クリックでも 1 手戻る）
  Ctrl+← →       10 手
  N・Home / End  初め・最後の局面
  ↑ ↓ / Enter    候補手を選ぶ / 打つ
  B / U / H      book を使う・うみがめ数・評価値を隠す
  R              盤の向き（8 通りを順に。「表示」→「盤の向き」で選べる）
  M              今の手順を控える
  E              盤面配置を始める・決める（配置中：左クリックで黒→白→空き、右クリックで空き、
                 T で手番、Delete で全部消す、I で初めの配置、Enter で決める、Esc でやめる）
  Ctrl+C / Ctrl+V  棋譜を写す / 貼る（二度押す。座標の列、盤の文字列、ビットボード、
                 ブログの PARAM の形を読む）
  Ctrl+P         スクリーンショット（exe の隣の screenshots フォルダ）
  Ctrl+Shift+← → 棋譜リストの前・次の棋譜
  Ctrl+Shift+↑ ↓ 棋譜リストの今の棋譜を消す・読み直す
  P / L          右のパネル・ログ

5. 主な機能（メニュー）
-----------------------
- 解析：評価値、評価の Lv、うみがめ数、後ろから解析、調べる範囲。
- book：使う・読み直す・別の book、精度の基準、book の中身をログに（info・stats・show）、
  足りない進行・最善の進行の書き出し、book の整理（保存・別名で保存・新しい空の book・深さ・
  merge・subtree・prune・export・import・extract・problem）。
- 学習：deviate・deviate2・deviate3・enhance・leaf expand・fill・fill2・play・store・add・learn・
  check・negamax・fix・correct・leaf recalculate、止める、途中保存から続ける、学習の列、学習の設定。
  book を変える命令（subtree・prune・import・new など）の後は、エンジンを閉じるときに
  book のファイルが上書きされることがあります。先に写しを取ってください。
- エンジン：nikque.18 系・newedax 系の切り替え、solve・bench・count・perft・estimate
  （book を読まない「道具のエンジン」を 1 回ごとに起動）、命令の欄（主のエンジンに送る）。
- 入出力：棋譜の写し・貼り付け・固定、局面の文字列とビットボード（10 進・16 進・Ntest の形・obf）
  の写しと取り込み、盤面配置、盤から手順の推定（20 手目まで。book にある手順を先に探し、
  無ければ book を使わずに探す）、スクリーンショット、盤の図（PNG）、棋譜リスト、棋譜検索、
  棋譜のファイル（base の convert・unique・compare・check・correct・complete・problem・tofen）、
  命令の列。
- 表示：デザイン（標準・肉球）、効果、盤の向き、手順の番号、マスに出す数（左下に book の局面数・
  Lv・精度の段階・うみがめ数）、タブ、控え、パネル、ログ。
- 控え：M で今の手順を控えます。新しい対局・棋譜の貼り付け・盤面配置・手順の推定のときも、それまでの
  手順を自動で控えます（数の制限なし、exe の隣の kept.txt）。控えのタブで押すと選び、二度押すと見る。
  「選んだ控えと比べる」は、固定の棋譜と同じ印（悪手に赤枠、違う手の所に印）で比べます。

ビットボードの形：10 進・16 進は「黒 白 手番」（a1 が 0 ビット目、h8 が 63 ビット目。手番は X か O）。
Ntest の形は「空き 手番の側の石 手番」。

6. 棋譜リスト
-------------
1 行に 1 局の文字のファイル（"f5d6c3..."、または "<64 マス> <X|O>;手順" で初めの局面から）。
読む・足す・保存・並べ替え（短い順、同じ長さは手順の順）・同じ棋譜をまとめる・切る・混ぜる（種）・
向きをそろえる（初手を f5 に、対称な局面でも決まった向きに）・向きを散らす（対称な局面で乱数）・
消す・何手目から始めるか・book の進行をリストへ。重い処理は裏で行い、「裏の仕事を止める」で止められます。
棋譜検索は、今の局面（向きの違うものも含む）が出る棋譜を探し、行を押すとその局面を開きます。

7. 命令の列
-----------
文字のファイル（UTF-8）に 1 行 1 つの命令を書き、「入出力」→「命令の列を行う」で順に行います。
起動のときに行う列は config.ini の [script] startup= に書きます。# で始まる行は注です。
仕事を始める命令（list load、search、learn start、edax、tool、estimate、wait など）は、
終わるまで次の行を待ちます。相対の名前は命令の列のファイルのフォルダから。

  moves <棋譜>             棋譜を読む（座標の列・盤の文字列・ビットボード）
  board <盤の文字列>       その局面から始める
  play <手>                今の局面から打つ
  new / goto <n|end> / back [n] / forward [n]
  book on|off / eval on|off / numbers on|off / level <n> / umi best|loss|off
  engine nikque18|newedax / design standard|paw / effects on|subdued|off
  tab moves|book|record|list|kept / rotate <0-7> / cellinfo none|positions|level|stage|umi
  lang ja|en|zh-Hans|zh-Hant
  wait [秒]                今の局面の解析が終わるまで待つ（既定 30 秒まで）
  estimate                 盤から手順を推定する
  keep                     今の手順を控える
  screenshot [ファイル]    画面を PNG に
  savelog <ファイル>       ログを書き出す
  list load|add|save <ファイル>
  list sort|unique|normalize|delete|next|prev|addgame|clear
  list truncate <n> / list shuffle [種] / list scatter black|white|both [種]
  list start <n> / list select <n> / list book [石損] [深さ] [本数] [add]
  search [exact]           棋譜検索（exact は向きの違うものを含めない）
  learn <book の命令>      学習の列に入れる（今の局面から） / learn start で始める
  edax <命令>              主のエンジンに送り、終わるまで待つ
  tool <命令>              道具のエンジンで行う（solve・bench・count・perft・estimate・base）
  run <ファイル>           別の命令の列（4 段まで）
  log <文字> / quit

8. config.ini の主な項目
------------------------
[engine] kind=nikque18|newedax、search=1（探索だけのエンジンを使う）
[nikque18]・[newedax] dir・exe・eval・book
[analysis] level・threads（0 は全部）・hash（ハッシュ表のビット数、0 はエンジンのまま）・umi・x・y・base・stages
[view] design・effects・language・numbers・cell_info・fps・font・opening_file・screenshot_dir
[window] 大きさと場所、tab、panel_w
[list] start_ply（棋譜リストの棋譜を開く手数、-1 は最後）
[learn_options] エンジンに送る学習の設定
[script] startup（起動のときの命令の列）
[keys] キーの割り当て（例：back=Left, Backspace）

9. ファイル
-----------
kept.txt（控え）、screenshots\（スクリーンショット）は exe の隣に作ります。
定石名は exe の隣の opening.ini（名前=手順）を読みます（ファイルは付いていません）。

10. ライセンス
--------------
nikque GUI：MIT License。エンジン（Edax nikque・newedax）：GNU GPL（別のプログラム）。


========================================================================
[English]
========================================================================

1. What it is
-------------
A GUI for Edax v4.5.5-nikque and newedax. The values of all the legal moves and the accuracy stage of the
book (0..7) are shown on the board; the book analyses (missing leaves, umigame numbers, positions by level,
best lines) and book learning are run from the menus.
The GUI is under the MIT License. The engines (Edax nikque, newedax) are GPL and run as separate programs
(the GUI starts an engine as a child process and talks to it with text commands).

2. Setting up
-------------
- Put config.ini, the lang folder and readme.txt next to nikque-gui.exe.
- Put the engines in engines\nikque18 and engines\newedax next to the exe, or point to their folders with
  dir= in [nikque18] and [newedax] of config.ini. With exe=auto the engine file is chosen for the CPU
  (wEdax-x86-64-v4.exe, -v3.exe, wEdax-x86-64.exe).
- The engine reads the book (data\book.dat of the engine, or the file in book=). The GUI never reads it.
- The language is chosen in Settings → Language.

3. The screen
-------------
- Board: the values of all the legal moves; colors and effects show the accuracy stage of the book
  (the dots in the top bar explain them).
- Tabs of the right panel: Moves (a sortable table), Book (positions by level, best lines, missing lines),
  Record, List (the game list and search results), Kept (kept lines). The graph of values is always below.
- Log: the bottom row; click to open it. View → Save the log writes it to a file.

4. Keys (default; all can be changed in [keys] of config.ini)
-------------------------------------------------------------
  Left / Right       one move back / forward (a right click also goes back)
  Ctrl+Left/Right    10 moves
  N, Home / End      first / last position
  Up, Down / Enter   choose a move / play it
  B / U / H          book on/off, umigame numbers, hide the values
  R                  board orientation (8 in turn; View → Board orientation to choose)
  M                  keep this line
  E                  start or finish setting up a position (left click black→white→empty, right click empty,
                     T side to move, Delete clear, I start position, Enter done, Esc cancel)
  Ctrl+C / Ctrl+V    copy / paste a game (press twice; coordinates, board text, bitboards, blog PARAM)
  Ctrl+P             screenshot (screenshots folder next to the exe)
  Ctrl+Shift+Left/Right  previous / next game of the list
  Ctrl+Shift+Up/Down     remove / read again the game of the list
  P / L              right panel / log

5. Main features (menus)
------------------------
- Analysis: values, search level, umigame numbers, backward analysis, the range.
- Book: use, read again, other books, accuracy base, book contents to the log (info, stats, show), write
  missing or best lines, maintenance (save, save as, new empty book, depth, merge, subtree, prune, export,
  import, extract, problem).
- Learning: deviate, deviate2, deviate3, enhance, leaf expand, fill, fill2, play, store, add, learn, check,
  negamax, fix, correct, leaf recalculate; stop; continue from the file saved on the way; the learning queue;
  settings. After a command that changes the book (subtree, prune, import, new...), the book file may be
  overwritten when the engine is closed. Copy it first.
- Engine: nikque.18 or newedax; solve, bench, count, perft, estimate (on a "tool engine" started for each
  job, without the book); command box (to the main engine).
- Input/Output: copy, paste and fix games; position text and bitboards (decimal, hexadecimal, Ntest form, obf);
  setting up a position; finding the moves of a position (up to move 20: the book first, else without it);
  screenshot; board image (PNG); game list; search; files of games (base convert, unique, compare, check,
  correct, complete, problem, tofen); command scripts.
- View: design (standard, paw), effects, orientation, move numbers, number on the squares (book positions,
  Lv, accuracy stage or umigame numbers at the lower left), tabs, kept lines, panel, log.
- Kept lines: M keeps this line. A new game, a pasted game, a position set up and a line found also keep the
  line before (no limit; kept.txt next to the exe). In the Kept tab, click to choose, twice to show.
  "Compare with the chosen line" marks the game like a fixed game (red frame on bad moves, marks where it differs).

Bitboards: decimal and hexadecimal are "black white side" (a1 is bit 0, h8 bit 63; side X or O).
The Ntest form is "empty-squares discs-of-the-side-to-move side".

6. Game list
------------
A text file with one game a line ("f5d6c3...", or "<64 squares> <X|O>;moves" from another start).
Open, add, save, sort (shorter first, then by the moves), join the same games, cut, shuffle (seed), one
orientation (first move f5, also at later symmetric positions), random orientations, remove, the move to
open at, book lines into the list. Heavy work runs in the background; "Stop the background work" stops it.
The search finds the games where this position appears (other orientations too); click a line to open it.

7. Command scripts
------------------
A UTF-8 text file with one command a line, run with Input/Output → Run a command script. The script run at
the start is set in [script] startup= of config.ini. Lines starting with # are notes. A command that starts
work waits for its end. Relative names are from the script's folder. The commands are listed in the
Japanese part above (moves, board, play, new, goto, back, forward, book, eval, numbers, level, umi, engine,
design, effects, tab, rotate, cellinfo, lang, wait, estimate, keep, screenshot, savelog, list ..., search,
learn, edax, tool, run, log, quit).

8. config.ini
-------------
See the Japanese part (section 8) for the keys; their names are the same.

9. Files
--------
kept.txt (kept lines) and screenshots\ are made next to the exe. Opening names are read from opening.ini
next to the exe (name=moves; not included).

10. License
-----------
nikque GUI: MIT License. Engines (Edax nikque, newedax): GNU GPL (separate programs).


========================================================================
[简体中文]（机器翻译）
========================================================================

1. 简介
-------
用于 Edax v4.5.5-nikque 与 newedax 的 GUI。在棋盘上显示所有合法着法的评价值与 book 的精度（段阶 0〜7），
并可从菜单进行 book 分析（不足的 leaf、海龟数、各 level 的局面数、最佳进行）与 book 学习。
GUI 为 MIT 许可。引擎（Edax nikque、newedax）为 GPL，作为独立程序运行。

2. 放置
-------
- 在 nikque-gui.exe 旁放置 config.ini、lang 文件夹、readme.txt。
- 把引擎放在 exe 旁的 engines\nikque18、engines\newedax，或在 config.ini 的 [nikque18]、[newedax] 的 dir= 指定文件夹。
- book 由引擎读取（引擎的 data\book.dat 或 book= 的文件）。GUI 不读取 book 文件。
- 语言在「设置」→「语言」中选择。

3. 按键
-------
← →：后退/前进一步（右键也后退）；Ctrl+← →：10 手；R：棋盘方向；M：备份手顺；E：摆放局面；
Ctrl+C / Ctrl+V：复制/粘贴棋谱；Ctrl+P：截图；Ctrl+Shift+← →：棋谱列表的上/下一个棋谱。
所有按键可在 config.ini 的 [keys] 中更改。

4. 功能
-------
book 的整理与导出、学习的命令、工具（solve、bench、count、perft、estimate、base）、棋谱列表与检索、
备份、摆放局面、从棋盘推断手顺（到第 20 手）、棋盘方向、手顺编号、格子上的数字、位棋盘、截图、命令序列。
命令序列的命令请参见日文部分第 7 节（命令名相同）。
改变 book 的命令（subtree、prune、import、new 等）之后，关闭引擎时 book 文件可能被覆盖。请先备份。

5. 许可
-------
nikque GUI：MIT License。引擎（Edax nikque、newedax）：GNU GPL（独立程序）。


========================================================================
[繁體中文]（機器翻譯）
========================================================================

1. 簡介
-------
用於 Edax v4.5.5-nikque 與 newedax 的 GUI。在棋盤上顯示所有合法著法的評價值與 book 的精度（段階 0〜7），
並可從選單進行 book 分析（不足的 leaf、海龜數、各 level 的局面數、最佳進行）與 book 學習。
GUI 為 MIT 授權。引擎（Edax nikque、newedax）為 GPL，作為獨立程式執行。

2. 放置
-------
- 在 nikque-gui.exe 旁放置 config.ini、lang 資料夾、readme.txt。
- 把引擎放在 exe 旁的 engines\nikque18、engines\newedax，或在 config.ini 的 [nikque18]、[newedax] 的 dir= 指定資料夾。
- book 由引擎讀取（引擎的 data\book.dat 或 book= 的檔案）。GUI 不讀取 book 檔案。
- 語言在「設定」→「語言」中選擇。

3. 按鍵
-------
← →：後退/前進一步（右鍵也後退）；Ctrl+← →：10 手；R：棋盤方向；M：備份手順；E：擺放局面；
Ctrl+C / Ctrl+V：複製/貼上棋譜；Ctrl+P：螢幕截圖；Ctrl+Shift+← →：棋譜清單的上/下一個棋譜。
所有按鍵可在 config.ini 的 [keys] 中變更。

4. 功能
-------
book 的整理與匯出、學習的命令、工具（solve、bench、count、perft、estimate、base）、棋譜清單與檢索、
備份、擺放局面、從棋盤推斷手順（到第 20 手）、棋盤方向、手順編號、格子上的數字、位元棋盤、螢幕截圖、命令序列。
命令序列的命令請參見日文部分第 7 節（命令名相同）。
改變 book 的命令（subtree、prune、import、new 等）之後，關閉引擎時 book 檔案可能被覆寫。請先備份。

5. 授權
-------
nikque GUI：MIT License。引擎（Edax nikque、newedax）：GNU GPL（獨立程式）。
