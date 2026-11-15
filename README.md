# nikque GUI

[日本語](#日本語) · [English](#english)

![nikque GUI](manual/images/ja-01-moves.png)

## 日本語

オセロのエンジン [Edax 4.5.5 nikque](https://github.com/Nikque/edax-reversi-AVX)（C 版）と [newedax](https://github.com/Nikque/newedax)（C++ 版）を画面から使うための、Windows 用のソフトです。試しの Mac 版（0.4.3 から）と、Linux 版・Android 版（0.4.4 から）もあります。

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

Mac 版（`nikque-gui-<版>-mac.zip`、macOS 13 以上、Apple シリコンと Intel）は署名していないので、展開した後に一度だけ「ターミナル」で `xattr -dr com.apple.quarantine <展開したフォルダ>` を打ってから `nikque GUI.app` を開きます（[2 章](manual/ja/02-setup.md#mac試しの版)）。

Linux 版（`nikque-gui-<版>-linux.tar.gz`、x86-64 の Ubuntu 22.04 以上、GTK 3）は、展開して `nikque-gui` を起動します（[2 章](manual/ja/02-setup.md#linux試しの版)）。

Android 版（`nikque-gui-<版>-android.apk`、Android 10 以上の arm64 の端末）は、APK を入れて開きます。対局・解析・棋譜までの版です（[2 章](manual/ja/02-setup.md#android試しの版)）。

エンジン 2 つ（Edax nikque と newedax）が同梱されています。**book は付いていません**。お持ちの `book.dat` を使う方法は、マニュアルの [2 章「置き方と起動」](manual/ja/02-setup.md#book-を置く) を見てください。

**マニュアル**：[manual/ja](manual/ja/README.md)

### ビルド

Visual Studio 2022（C++、x64）と CMake を使います。

- `build.cmd`：`build\Release` にビルドし、単体テスト 6 本を実行します。
- `build.cmd measure`：計測用のビルド（`build-measure`）。
- `tools\make_dist.ps1`：配布用の zip を作ります。
- Mac 版：CMake と Xcode のコマンドラインツールで `cmake -S . -B build -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`、`cmake --build build`。配布用の zip は `tools/make_dist_mac.sh`（GitHub Actions の `.github/workflows/mac.yml`）。
- Linux 版：GTK 3 の開発用のパッケージ（Ubuntu では `libgtk-3-dev`）と CMake、g++ 12 以上で `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`、`cmake --build build`。配布用の tar.gz は `tools/make_dist_linux.sh`（`.github/workflows/linux.yml`）。
- Android 版：`android/` が Gradle のプロジェクトです（Android SDK・NDK、JDK 17）。APK は `tools/make_dist_android.sh`（`.github/workflows/android.yml`）。

### ライセンス

nikque GUI は MIT License（[LICENSE.txt](LICENSE.txt)）です。同梱のエンジンは GNU GPL v3 の別のプログラムで、GUI はそれを別のプロセスとして起動し、テキストのコマンドでやりとりします。エンジンのソースは上のリンクと、配布物の `engines\*\SOURCE.txt` にあります。

## English

A GUI for Windows (and trial versions for Mac from 0.4.3, and for Linux and Android from 0.4.4) for the Othello engines [Edax 4.5.5 nikque](https://github.com/Nikque/edax-reversi-AVX) (C) and [newedax](https://github.com/Nikque/newedax) (C++).

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

The Mac version (`nikque-gui-<version>-mac.zip`, macOS 13 or later, Apple silicon and Intel) is not signed: after unpacking it, type `xattr -dr com.apple.quarantine <the folder>` once in Terminal, then open `nikque GUI.app` ([chapter 2](manual/en/02-setup.md#mac-trial-version)).

The Linux version (`nikque-gui-<version>-linux.tar.gz`, x86-64 Ubuntu 22.04 or later, GTK 3): unpack it and start `nikque-gui` ([chapter 2](manual/en/02-setup.md#linux-trial-version)).

The Android version (`nikque-gui-<version>-android.apk`, arm64 devices with Android 10 or later): install the APK and open it. It plays, analyses and keeps the moves ([chapter 2](manual/en/02-setup.md#android-trial-version)).

Two engines (Edax nikque and newedax) are included. **No book is included**; to use your own `book.dat`, see [chapter 2 of the manual](manual/en/02-setup.md).

**Manual**: [manual/en](manual/en/README.md)

### Building

Visual Studio 2022 (C++, x64) and CMake.

- `build.cmd`: builds into `build\Release` and runs the 6 unit tests.
- `build.cmd measure`: a build for measurements (`build-measure`).
- `tools\make_dist.ps1`: makes the distribution zip.
- Mac: CMake and the Xcode command line tools: `cmake -S . -B build -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, `cmake --build build`. The distribution zip: `tools/make_dist_mac.sh` (GitHub Actions, `.github/workflows/mac.yml`).
- Linux: the development package of GTK 3 (`libgtk-3-dev` on Ubuntu), CMake and g++ 12 or later: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`, `cmake --build build`. The distribution tar.gz: `tools/make_dist_linux.sh` (`.github/workflows/linux.yml`).
- Android: `android/` is a Gradle project (the Android SDK and NDK, JDK 17). The APKs: `tools/make_dist_android.sh` (`.github/workflows/android.yml`).

### License

nikque GUI is under the MIT License ([LICENSE.txt](LICENSE.txt)). The included engines are separate programs under the GNU GPL v3; the GUI starts them as separate processes and talks to them with text commands. Their source is at the links above and in `engines\*\SOURCE.txt` of the distribution.
