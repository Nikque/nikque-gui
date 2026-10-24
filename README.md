# nikque GUI

[日本語](#日本語) · [English](#english)

![nikque GUI](manual/images/ja-01-moves.png)

## 日本語

オセロのエンジン [Edax 4.5.5 nikque](https://github.com/Nikque/edax-reversi-AVX)（C 版）と [newedax](https://github.com/Nikque/newedax)（C++ 版）を画面から使うための、Windows 用のソフトです。

### できること

- **盤の上に評価値を表示**：すべての合法手の評価値と、book の精度（段階 0〜7）を盤に重ねて表示します。
- **book の解析**：次の精度の段階に足りない局面、うみがめ数（暗記が必要な進行の数）、level ごとの局面数、最善の進行、進行の木。
- **book の学習と整理**：`deviate`・`enhance`・`fill` などの学習を画面から実行。途中で止めて再開でき、複数の学習を順番に並べられます。book を軽くする `book reduce` も使えます。
- **対局**：人対コンピューター、コンピューター同士、持ち時間あり。序盤を book から選べます。
- **棋譜**：100 万局まで扱える棋譜リスト、棋譜検索、盤面の配置、盤面から手順の推定。
- **そのほか**：2 つの棋譜の比較（悪手や違う手に印）、しおり、コマンドを並べたスクリプトの実行。
- **表示言語**：日本語・English・简体中文・繁體中文（中国語は機械翻訳）。

### 使い始め方

1. [最新の Release](https://github.com/Nikque/nikque-gui/releases/latest) の zip をダウンロードし、好きな場所に展開します。インストールは要りません。
2. `nikque-gui.exe` を起動します。

エンジン 2 つ（Edax nikque と newedax）が同梱されています。**book は付いていません**。お持ちの `book.dat` を使う方法は、マニュアルの [2 章「置き方と起動」](manual/ja/02-setup.md#book-を置く) を見てください。

**マニュアル**：[manual/ja](manual/ja/README.md)

### ビルド

Visual Studio 2022（C++、x64）と CMake を使います。

- `build.cmd`：`build\Release` にビルドし、単体テスト 6 本を実行します。
- `build.cmd measure`：計測用のビルド（`build-measure`）。
- `tools\make_dist.ps1`：配布用の zip を作ります。

### ライセンス

nikque GUI は MIT License（[LICENSE.txt](LICENSE.txt)）です。同梱のエンジンは GNU GPL v3 の別のプログラムで、GUI はそれを別のプロセスとして起動し、テキストのコマンドでやりとりします。エンジンのソースは上のリンクと、配布物の `engines\*\SOURCE.txt` にあります。

## English

A Windows GUI for the Othello engines [Edax 4.5.5 nikque](https://github.com/Nikque/edax-reversi-AVX) (C) and [newedax](https://github.com/Nikque/newedax) (C++).

### Features

- **Values on the board**: the score of every legal move and the accuracy of the book (stages 0 to 7).
- **Book analysis**: positions missing for the next accuracy stage, umigame numbers (how many lines must be memorized), positions per level, best lines, a tree of book lines.
- **Book learning and maintenance**: run `deviate`, `enhance`, `fill` and other learning commands from the GUI; stop and resume them, and queue several runs. `book reduce` makes a book smaller.
- **Games**: human vs. computer, computer vs. computer, time controls, openings chosen from the book.
- **Game records**: lists of up to a million games, search, position setup, finding the moves that lead to a position.
- **Also**: comparing two game records (bad moves and differences marked), bookmarks, scripts of commands.
- **Languages**: Japanese, English, Simplified and Traditional Chinese (Chinese is machine-translated).

### Getting started

1. Download the zip from the [latest release](https://github.com/Nikque/nikque-gui/releases/latest) and unpack it anywhere. No installation is needed.
2. Start `nikque-gui.exe`.

Two engines (Edax nikque and newedax) are included. **No book is included**; to use your own `book.dat`, see [chapter 2 of the manual](manual/en/02-setup.md).

**Manual**: [manual/en](manual/en/README.md)

### Building

Visual Studio 2022 (C++, x64) and CMake.

- `build.cmd`: builds into `build\Release` and runs the 6 unit tests.
- `build.cmd measure`: a build for measurements (`build-measure`).
- `tools\make_dist.ps1`: makes the distribution zip.

### License

nikque GUI is under the MIT License ([LICENSE.txt](LICENSE.txt)). The included engines are separate programs under the GNU GPL v3; the GUI starts them as separate processes and talks to them with text commands. Their source is at the links above and in `engines\*\SOURCE.txt` of the distribution.
