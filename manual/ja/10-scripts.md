# 10. スクリプト

[目次](README.md) ・ 前：[9. ツールと棋譜のファイル](09-tools.md) ・ 次：[11. 設定](11-settings.md)

テキストファイル（UTF-8）に、1 行に 1 つのコマンドを書きます。

- 「入出力」→「スクリプトを実行…」で、上から順に実行します。
- 「スクリプトを止める」で止めます。
- 起動時に実行するスクリプトは、`config.ini` の `[script] startup=` に書きます。

書き方の決まり：

- `#` で始まる行はコメントです。
- 処理を始めるコマンド（`list load`・`search`・`learn start`・`edax`・`tool`・`estimate`・`wait`・`book reduce` など）は、終わるまで次の行に進みません。
- `wait`・`waitgame` が時間切れで終わったときは、ログにそう出ます。
- 相対パスは、スクリプトのファイルがあるフォルダを基準にします。
- 誤りのある行があると、ログに出してスクリプトを止めます。
- `run` で別のスクリプトを呼べます（4 段まで）。

## コマンド

| コマンド | 中身 |
|---|---|
| `moves <棋譜>` | 棋譜を読み込む（座標の列・盤面の文字列・ビットボード） |
| `board <盤面の文字列>` | その局面から始める |
| `play <手>` | 今の局面から打つ |
| `new` / `goto <n\|end>` / `back [n]` / `forward [n]` | 最初から・n 手目へ・戻る・進む |
| `book on\|off` / `eval on\|off` / `numbers on\|off` | book を使う・評価値・手の番号 |
| `level <0-60>` / `umi best\|loss\|off` | 評価の Lv・うみがめ数 |
| `precision on\|off` | book の精度を計算する・しない |
| `analysis backward\|forward\|off` | 棋譜の局面を裏で読む順（後ろから解析・前から解析・読まない） |
| `stop` | 裏の処理を止める |
| `pv on\|off` | マウスを乗せた手の読み筋を盤に表示する・しない |
| `openingmarks on\|off` | 定石の手順の手に印を付ける・付けない |
| `book reload` | book を読み込み直す（メインエンジンを再起動） |
| `book reduce <X> <Y> [here]` | book を軽くする（初期局面から。`here` は今の局面から）。数えてから、確かめずに行う |
| `engine nikque\|newedax` | エンジンの系列 |
| `design standard\|paw\|toggle` / `effects on\|subdued\|off` | デザイン（`toggle` で切り替え）・効果 |
| `tab moves\|book\|record\|list\|tree` | タブ |
| `rotate <0-7>` / `cellinfo none\|positions\|level\|stage\|umi` | 盤の向き・マスの左下に表示する数 |
| `lang ja\|en\|zh-Hans\|zh-Hant` | 言語 |
| `wait [秒]` | 今の局面の解析が終わるまで待つ（既定 30 秒まで） |
| `estimate` | 盤から手順を推定する |
| `keep` / `bookmark <n>` / `bookmarks on\|off` | 今の局面にしおりを挟む・n 番のしおりの局面へ・しおりの列を表示する・しない |
| `screenshot [ファイル]` / `savelog <ファイル>` | 画面を PNG に・ログを保存する |
| `list load\|add\|save <ファイル>` | 棋譜リストを読み込む・追加する・保存する |
| `list sort\|unique\|normalize\|delete\|next\|prev\|addgame\|clear` | 棋譜リストの操作 |
| `list truncate <n>` / `list shuffle [種]` / `list scatter black\|white\|both [種]` | 短くする・並びを混ぜる・向きをばらばらにする |
| `list start <n>` / `list select <n>` | 表示する手数・n 番目の棋譜を開く |
| `list book [石損] [深さ] [本数] [add]` | book の進行をリストへ |
| `search [exact]` | 棋譜検索（`exact` は向きの違うものを含めない） |
| `learn <book のコマンド>` / `learn start` | 学習キューに入れる（今の局面から）・キューを実行する |
| `edax <コマンド>` | メインエンジンに送り、終わるまで待つ |
| `tool <コマンド>` | ツール用エンジンで実行する（solve・bench・count・perft・estimate・base） |
| `run <ファイル>` | 別のスクリプト |
| `game free\|black\|white\|engines [数]` | 対局を始める（`black` は人が黒） |
| `comlevel <n>` / `clock <分>` / `ponder on\|off` | コンピューターの Lv・持ち時間・相手の番にも考える |
| `opening search\|best\|wide [幅] [手数]` | 序盤の打ち方 |
| `go` / `takeback` | コンピューターに 1 手打たせる・待った |
| `waitgame [秒]` | 対局が終わるまで待つ（コンピューターどうしは全部の対局） |
| `compare on\|off` | 比較モード |
| `tree open\|close\|go <手順>` / `tree root` / `tree toggle <手順>` | 木の段を開く・閉じる・その局面へ（手順は根から）、今の局面を根に、行の ▸ / ▾ と同じ |
| `log <文字>` / `quit` | ログに書く・GUI を終える |

## ファイル名と文字

- `edax` と `tool` のコマンドの中で `"…"` で囲んだファイル名は、エンジンが開ける形に変換して渡します。変換の方法は、短い名前にするか、一時フォルダを経由するかです。
- `"…"` で囲んだファイルが存在しないときは、これから書き出すファイルとして扱います。
- コマンドは、エンジンが読める文字（システムの文字コード）に変換して送ります。
- 日本語のフォルダ名も、`"…"` で囲んでも囲まなくても渡せます。空白があるときは囲みます。
- 囲まない名前はそのままエンジンに渡します。そのため、相対パスはエンジンのフォルダが基準になります。
- `learn` のコマンドはキューの 1 件として後で実行するので、`"…"` の名前を変換せずにそのまま送ります。空白の無い名前を書いてください。
- エンジンが読めない文字を含むコマンドがあると、ログに出してスクリプトを止めます。

## 例

```text
# 今の book で、f5d6 の後の足りない進行を学習する
engine nikque
moves f5d6
wait 60
learn book store
learn book deviate 2 4
learn start
edax book save "D:\books\学習後.dat"
screenshot after.png
quit
```

```text
# book を軽くしてから、別の名前で保存する（元の book は残す）
engine nikque
book reduce 2 4
edax book save "D:\books\軽い.dat"
book reload
quit
```
