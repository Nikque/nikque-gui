# 11. 設定

[目次](README.md) ・ 前：[10. 命令の列](10-scripts.md) ・ 次：[12. 困ったときは](12-troubleshooting.md)

## 表示

![肉球のデザイン](../images/ja-07-paw.png)

「表示」のメニュー：

- デザイン：標準・肉球。
- 効果：入・控えめ（動かさない）・切。いちばん上の段（段階 7）の効果：強い・ふつう・弱い。
- 盤の向き：8 通り（R で順に）。棋譜そのものを対称に動かします。
- 手順の番号を石に出す、マスに出す数（[4. 解析](04-analysis.md)）。
- タブ、控え、右のパネルを隠す（P）、ログを広げる（L）、ログを書き出す。

## config.ini

「設定」→「config.ini を開く」でメモ帳に開きます。GUI を閉じるときにも書き直されるので、手で直すときは GUI を閉じてからにしてください。

| 節 | 主な項目 |
|---|---|
| `[engine]` | `kind=nikque18\|newedax`、`search=1`（探索だけのエンジンを使う） |
| `[nikque18]`・`[newedax]` | `dir`（エンジンのフォルダ）・`exe`（`auto` か exe の名前）・`eval`・`book` |
| `[analysis]` | `level`・`threads`（0 は全部）・`hash`（ハッシュ表のビット数、0 はエンジンのまま）・`umi`・`x`・`y`・`base`・`stages`・`compare` |
| `[play]` | `level`・`opening`（`search\|best\|wide`）・`width`・`until`・`clock`・`ponder`・`end_keep`・`end_list` |
| `[view]` | `design`・`effects`・`language`・`numbers`・`cell_info`・`fps`・`font`・`opening_file`・`screenshot_dir` |
| `[window]` | 大きさと場所、`tab`、`panel_w` |
| `[list]` | `start_ply`（棋譜リストの棋譜を開く手数、-1 は最後） |
| `[learn]` | 最後の学習の命令と局面（「途中保存から続ける」が使う） |
| `[learn_options]` | エンジンに送る学習の設定（[6. 学習](06-learning.md)） |
| `[tools]` | 道具の窓の値、命令の欄の最後の命令 |
| `[script]` | `startup`（起動のときの命令の列） |
| `[keys]` | キーの割り当て |

## キー

既定の割り当て（`[keys]` で全部変えられます。例：`back=Left, Backspace`。「ヘルプ」→「キーの一覧」で今の割り当てが出ます）：

| キー | 動き |
|---|---|
| ← → | 1 手戻る・進む（右クリックでも 1 手戻る） |
| Ctrl+← → | 10 手 |
| N・Home / End | 初め・最後の局面 |
| ↑ ↓ / Enter・Space | 候補手を選ぶ / 打つ |
| B / U / H | book を使う・うみがめ数・評価値を隠す |
| R | 盤の向き |
| M | 今の手順を控える |
| E | 盤面配置を始める・決める |
| Ctrl+C / Ctrl+V | 棋譜を写す / 貼る（二度押す） |
| Ctrl+P | スクリーンショット |
| Ctrl+Shift+← → | 棋譜リストの前・次の棋譜 |
| Ctrl+Shift+↑ ↓ | 棋譜リストの今の棋譜を消す・読み直す |
| P / L | 右のパネル・ログ |

## ファイル

exe の隣に作るもの：`kept.txt`（控え）、`screenshots\`（スクリーンショット）。読むもの：`opening.ini`（定石名、付いていません）。
