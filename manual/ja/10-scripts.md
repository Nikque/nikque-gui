# 10. 命令の列

[目次](README.md) ・ 前：[9. 道具と棋譜のファイル](09-tools.md) ・ 次：[11. 設定](11-settings.md)

文字のファイル（UTF-8）に 1 行 1 つの命令を書き、「入出力」→「命令の列を行う…」で順に行います。「命令の列を止める」で止めます。起動のときに行う列は `config.ini` の `[script] startup=` に書きます。

- `#` で始まる行は注です。
- 仕事を始める命令（`list load`・`search`・`learn start`・`edax`・`tool`・`estimate`・`wait` など）は、終わるまで次の行を待ちます。`wait`・`waitgame` が時間で終わったときは、ログにそう出ます。
- 相対の名前は、命令の列のファイルのフォルダからです。
- 誤りのある行があると、ログに出して列を止めます。
- 別の列を `run` で呼べます（4 段まで）。

## 命令

| 命令 | 中身 |
|---|---|
| `moves <棋譜>` | 棋譜を読む（座標の列・盤の文字列・ビットボード） |
| `board <盤の文字列>` | その局面から始める |
| `play <手>` | 今の局面から打つ |
| `new` / `goto <n\|end>` / `back [n]` / `forward [n]` | 初めから・n 手目へ・戻る・進む |
| `book on\|off` / `eval on\|off` / `numbers on\|off` | book を使う・評価値・手順の番号 |
| `level <n>` / `umi best\|loss\|off` | 評価の Lv・うみがめ数 |
| `book reload` | book を読み直す（主のエンジンを起動し直す） |
| `engine nikque18\|newedax` | エンジンの系 |
| `design standard\|paw` / `effects on\|subdued\|off` | デザイン・効果 |
| `tab moves\|book\|record\|list\|kept\|tree` | タブ |
| `rotate <0-7>` / `cellinfo none\|positions\|level\|stage\|umi` | 盤の向き・マスに出す数 |
| `lang ja\|en\|zh-Hans\|zh-Hant` | 言葉 |
| `wait [秒]` | 今の局面の解析が終わるまで待つ（既定 30 秒まで） |
| `estimate` | 盤から手順を推定する |
| `keep` | 今の手順を控える |
| `screenshot [ファイル]` / `savelog <ファイル>` | 画面を PNG に・ログを書き出す |
| `list load\|add\|save <ファイル>` | 棋譜リストを読む・足す・保存する |
| `list sort\|unique\|normalize\|delete\|next\|prev\|addgame\|clear` | 棋譜リストの操作 |
| `list truncate <n>` / `list shuffle [種]` / `list scatter black\|white\|both [種]` | 切る・混ぜる・向きを散らす |
| `list start <n>` / `list select <n>` | 開く手数・n 番目の棋譜を開く |
| `list book [石損] [深さ] [本数] [add]` | book の進行をリストへ |
| `search [exact]` | 棋譜検索（`exact` は向きの違うものを含めない） |
| `learn <book の命令>` / `learn start` | 学習の列に入れる（今の局面から）・列を始める |
| `edax <命令>` | 主のエンジンに送り、終わるまで待つ |
| `tool <命令>` | 道具のエンジンで行う（solve・bench・count・perft・estimate・base） |
| `run <ファイル>` | 別の命令の列 |
| `game free\|black\|white\|engines [数]` | 対局を始める（`black` は人が黒） |
| `comlevel <n>` / `clock <分>` / `ponder on\|off` | コンピューターの Lv・持ち時間・相手の番にも考える |
| `opening search\|best\|wide [幅] [手数]` | 序盤の打ち方 |
| `go` / `takeback` | コンピューターに 1 手打たせる・待った |
| `waitgame [秒]` | 対局が終わるまで待つ（コンピューターどうしは全部の対局） |
| `compare on\|off` | 比べるモード |
| `tree open\|close\|go <手順>` / `tree root` / `tree toggle <手順>` | 木の段を開く・閉じる・その局面へ（手順は根から）、今の局面を根に、行の ▸ / ▾ と同じ |
| `log <文字>` / `quit` | ログに書く・GUI を終える |

## ファイルの名前と文字

- `edax` と `tool` の命令の中で `"…"` で囲んだファイルの名前は、エンジンが開ける形（短い名前、または一時フォルダを通す形）にして渡します。そこに無いファイルは書かれるファイルとして扱います。
- 命令は、エンジンの読める文字（システムの文字コード）にして送ります。日本語のフォルダの名前も、囲んでも囲まなくても渡せます（空白があるときは囲みます）。囲まない名前はそのままエンジンに渡すので、相対の名前はエンジンのフォルダからになります。
- `learn` の命令は列の仕事として後で行うので、`"…"` の名前は変えずにそのまま送ります。空白の無い名前を書いてください。
- エンジンの読めない文字のある命令は、ログに出して列を止めます。

## 例

```text
# 今の book で、f5d6 の後の足りない進行を学習する
engine nikque18
moves f5d6
wait 60
learn book store
learn book deviate 2 4
learn start
edax book save "D:\books\学習後.dat"
screenshot after.png
quit
```
