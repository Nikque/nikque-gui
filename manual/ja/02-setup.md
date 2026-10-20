# 2. 置き方と起動

[目次](README.md) ・ 前：[1. はじめに](01-introduction.md) ・ 次：[3. 画面](03-screen.md)

## 置き方

1. 配布の ZIP（`nikque-gui-0.1.0.zip`）を好きな場所に展開します。インストールは要りません。
2. `nikque-gui.exe` を起動します。同じフォルダに `config.ini`・`lang` フォルダ・`readme.txt` があります。

フォルダの中身：

| ファイル | 中身 |
|---|---|
| `nikque-gui.exe` | GUI |
| `config.ini` | 設定（[11. 設定](11-settings.md)） |
| `lang\*.txt` | 画面の言葉（日本語・English・简体中文・繁體中文） |
| `engines\nikque18` | C 版の Edax v4.5.5-nikque.19 |
| `engines\newedax` | newedax.4（C++ 版） |
| `readme.txt` | 短い説明 |

フォルダの名前 `nikque18` は `config.ini` の系の名前 `[nikque18]` に合わせたもので、中身は nikque.19 です。どちらのエンジンのフォルダにも `wEdax-x86-64-v4.exe`（AVX-512）・`wEdax-x86-64-v3.exe`（AVX2）・`wEdax-x86-64.exe`（ほかの x64）と `data\eval.dat` があります。

## エンジンの系を選ぶ

「エンジン」→「nikque 系」・「newedax 系」で切り替えます。どちらも同じ book の解析の命令を持ち、同じ答えを返します。newedax は eval2 の系列の評価関数（別の `eval.dat`）を使います。

## book を置く

book は付いていません。

- お持ちの `book.dat` をエンジンのフォルダの `data\book.dat` に置くか、
- 「book」→「別の book を開く…」で選びます（ファイルを窓に落としても開けます）。`config.ini` の `[nikque18]`・`[newedax]` の `book=` に書いても同じです。

book が無いと、エンジンは `data\book.dat` に初めの局面だけの小さな book を作ります。後でお持ちの book に置き換えてかまいません。

- book のファイルはエンジンが読み書きします。学習や book を変える命令の後は、エンジンを閉じるときに book のファイルが保存されることがあります。大事な book は先に写しを取ってください。
- nikque 系は newedax の book の形 2（`NEDXBOOK`）を読めません。その book を開くと知らせます。

## すでにあるエンジンを使う

`config.ini` の `[nikque18]`・`[newedax]` の `dir=` にエンジンのフォルダを書きます。`exe=auto` なら CPU に合わせて exe を選び、`exe=` にファイルの名前を書けばそれを使います。

GUI は book の解析の命令（`book moveinfo` など）を使うので、v4.5.5-nikque.19・v4.5.5-newedax.4 以降のエンジンが要ります。古いエンジンでは上の帯に「book の解析なし（古いエンジン）」と出て、book の値・木・book のタブは出ません（探索の評価値は出ます）。

## 言葉

「設定」→「言葉」で選びます。中国語の画面の言葉は機械的に訳したものです。

## 名前と文字

- エンジンは、ファイルの名前をシステムの文字コード（日本語の Windows では Shift_JIS）で読みます。GUI は、渡せない名前（その文字コードに無い文字、長すぎる名前）のときは、フォルダを短い名前（8.3 形式）にして渡すか、知らせて渡しません。
- book のファイルの名前そのものは短い名前にしません（エンジンは book を保存するとき、渡された名前で書くため）。book の名前に、システムの文字コードに無い文字（日本語の Windows ではハングルなど）があると開けません。名前を変えてください。
