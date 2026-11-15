# 2. 置き方と起動

[目次](README.md) ・ 前：[1. はじめに](01-introduction.md) ・ 次：[3. 画面](03-screen.md)

## 置き方

1. 配布の ZIP（`nikque-gui-0.4.3.zip`）を好きな場所に展開します。インストールは要りません。
2. `nikque-gui.exe` を起動します。

フォルダの中身：

| ファイル | 中身 |
|---|---|
| `nikque-gui.exe` | GUI |
| `config.ini` | 設定（[11. 設定](11-settings.md)） |
| `lang\*.txt` | 画面の言葉（日本語・English・简体中文・繁體中文） |
| `engines\nikque` | C 版の Edax v4.5.5-nikque.27 |
| `engines\newedax` | newedax.14（C++ 版、64 枠の版） |
| `readme.txt` | 短い説明 |

- どちらのエンジンのフォルダにも、次のファイルがあります。
  - `wEdax-x86-64-v4.exe`（AVX-512）
  - `wEdax-x86-64-v3.exe`（AVX2）
  - `wEdax-x86-64.exe`（その他の x64）
  - `data\eval.dat`
- 0.2.0 までの `config.ini`（系列の名前が `[nikque18]`）も、そのまま読めます。読み込むと `[nikque]` に直して保存します。

## Mac（試しの版）

0.4.3 から、Mac 版（`nikque-gui-0.4.3-mac.zip`）があります。macOS 13 以上の Mac（Apple シリコン・Intel のどちらも）で動きます。まだ試しの版です。動きのおかしい所があれば知らせてください。

1. ZIP を好きな場所に展開します。
2. 署名していないアプリなので、そのままでは macOS に止められます。初めに一度だけ、「ターミナル」で次のように打ちます（`~/Downloads/nikque-gui-0.4.3-mac` は展開したフォルダの場所に替えてください）。

   ```
   xattr -dr com.apple.quarantine ~/Downloads/nikque-gui-0.4.3-mac
   ```

   フォルダの中のアプリとエンジンのすべてが開けるようになります。
3. `nikque GUI.app` を開きます。

フォルダの中身は Windows 版と同じです。`nikque GUI.app` の隣に `config.ini`・`lang`・`engines` があります。

- どちらのエンジンのフォルダにも、`mEdax-arm64`（Apple シリコン）・`mEdax-x64-modern`（Intel）・`data/eval.dat` があります。`exe=auto` なら Mac に合わせて選びます。
- nikque 系のエンジンは v4.5.5-nikque.27 の Release の ZIP のものです。newedax 系のエンジンは、newedax.14 の Release に Mac 用が無いので、そのタグのソース（64 枠）からこの配布物のために作ったものです（`engines/newedax/SOURCE.txt`）。
- `config.ini` などの名前は `/` で区切ります。`dir=` などに書く場所も Mac の書き方です（例：`/Users/名前/books/book.dat`）。
- キー：Windows の Ctrl の代わりに Command（⌘）を使います（例：Command+C で棋譜をコピー）。右クリックは、トラックパッドの 2 本指のクリックか、control を押しながらのクリックです。
- 空白を含む名前のファイルはエンジンに直接渡せないので、GUI が一時フォルダを通して渡します（ログにそう出ます）。
- アプリを「アプリケーション」フォルダに移すときは、`config.ini`・`lang`・`engines` もいっしょのフォルダに置いてください。

## エンジンの系列を選ぶ

「エンジン」→「nikque 系」・「newedax 系」で切り替えます。初めて起動したときは newedax 系です（0.4.0 までは nikque 系でした）。選んだ系列は `config.ini` に保存され、次の起動でも使われます。

- どちらも同じ book の解析のコマンドを持ち、同じ結果を返します。
- newedax.13 からは、新しく学習し直した評価関数（64 枠）を使います。`eval.dat` は約 108 MB で、nikque の `eval.dat` とは別のものです。
- newedax の `eval.dat` は、エンジンと同じ版の配布物のものを使ってください。前の版の newedax・eval2・nikque の `eval.dat` を読ませると、エンジンは動きますが評価が正しくありません。そのときは GUI がメッセージで知らせます（[12. 困ったときは](12-troubleshooting.md)）。
- 48 枠の版（newedax の Release の `edax-4.5.5-newedax.14-48.zip`）は付いていません。使うときは、その ZIP を別のフォルダに展開し、`config.ini` の `[newedax]` の `dir=` にその `bin` のフォルダを書きます。48 枠の exe には、同じ ZIP の 48 枠の `eval.dat` を使ってください（取り違えると、メッセージは出ずに評価が正しくなくなります）。
- メモリの目安：newedax.14 はエンジン 1 つで約 160 MB（`wEdax-x86-64.exe`）〜約 275 MB（AVX-512 の `wEdax-x86-64-v4.exe`）を使います。GUI はエンジンを最大 5 つ（メイン・探索用・ツール用・対局用・比較用）起動するので、すべて newedax だと約 0.8〜1.4 GB になります。置換表の大きさやスレッドの数で増え、book の大きさは別に足されます。
- book の形式は変わらないので、前の版で使っていた book はそのまま読めます。ただし、前の版で学習した book の値は前の評価関数でつけたものです。

## book を置く

book は付いていません。次のどちらかの方法で使います。

- お持ちの `book.dat` を、エンジンのフォルダの `data\book.dat` に置く。
- 「book」→「別の book を開く…」で選ぶ。ファイルをウィンドウにドラッグ＆ドロップしても開けます。`config.ini` の `[nikque]`・`[newedax]` の `book=` に書いても同じです。

book が無いと、エンジンは `data\book.dat` に初期局面だけの小さな book を作ります。後でお持ちの book に置き換えてかまいません。

- book のファイルはエンジンが読み書きします。学習や book を変えるコマンドの後は、エンジンを閉じるときに book のファイルが保存されることがあります。大事な book は先にコピーを取ってください。
- nikque 系列は、newedax の book の形式 2（`NEDXBOOK`）を読めません。その book を開くと、メッセージが出ます。

## すでにあるエンジンを使う

`config.ini` の `[nikque]`・`[newedax]` の `dir=` に、エンジンのフォルダを書きます。

- `exe=auto` なら、CPU に合わせて exe を選びます。
- `exe=` にファイル名を書けば、そのファイルを使います。

GUI は book の解析のコマンド（`book moveinfo` など）を使います。そのため、v4.5.5-nikque.19・v4.5.5-newedax.4 以降のエンジンが必要です。

- 古いエンジンでは、上の帯に「book の解析なし（古いエンジン）」と出ます。book の値・木・book タブは表示されません。探索の評価値は表示されます。
- book を軽くする機能（`book reduce`）は v4.5.5-nikque.20・v4.5.5-newedax.5 からです。それより前のエンジンでは、メッセージを出して何もしません。

## 言語

「設定」→「言語」で選びます。中国語の画面の言葉は機械翻訳です。

## ファイル名と文字

- エンジンは、ファイル名をシステムの文字コード（日本語の Windows では Shift_JIS）で読みます。
- エンジンに渡せない名前のときは、GUI は次のどちらかにします。渡せない名前とは、その文字コードに無い文字を含む名前や、長すぎる名前です。
  - フォルダを短い名前（8.3 形式）にして渡す。
  - メッセージを出して渡さない。
- book のファイル名そのものは、短い名前にしません。エンジンは book を保存するとき、渡された名前で書くためです。
- book のファイル名に、システムの文字コードに無い文字（日本語の Windows ではハングルなど）があると開けません。名前を変えてください。
