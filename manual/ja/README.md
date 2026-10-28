# nikque GUI ユーザーマニュアル

[English](../en/README.md)

nikque GUI は、オセロのエンジン Edax v4.5.5-nikque（C 版）と newedax（C++ 版）のための、Windows 用の GUI です。

- 盤の上に、すべての合法手の評価値と book の精度を表示します。
- book の解析・学習・整理、対局、棋譜の整理を画面から行えます。

![候補手タブ](../images/ja-01-moves.png)

## 目次

1. [はじめに](01-introduction.md)：何ができるか、ライセンス
2. [置き方と起動](02-setup.md)：ZIP、エンジン、book、言語
3. [画面](03-screen.md)：上の帯、盤、デザイン、右のパネル、しおりの列、グラフ、ログ
4. [解析](04-analysis.md)：評価値、Lv、棋譜の局面をバックグラウンドで読む、バックグラウンドの処理を止める、book の精度の段階、うみがめ数、調べる範囲、比較モード
5. [book](05-book.md)：book タブ、木の表示、書き出し、book の整理、book を軽くする
6. [学習](06-learning.md)：学習のコマンド、中止・再開、学習キュー、学習の設定
7. [対局](07-play.md)：対局の種類、コンピューターの Lv、持ち時間、序盤の打ち方
8. [棋譜・リスト・しおり](08-games.md)：コピー・貼り付け、局面の文字列、盤面配置、手順の推定、棋譜リスト、検索、しおり
9. [ツールと棋譜のファイル](09-tools.md)：solve・bench・count・perft・estimate、base、コマンドの入力
10. [スクリプト](10-scripts.md)：画面を使わずに操作を並べて実行する
11. [設定](11-settings.md)：config.ini、キー、表示
12. [困ったときは](12-troubleshooting.md)

対象のバージョン：

- nikque GUI 0.2.3
- エンジン v4.5.5-nikque.25・v4.5.5-newedax.10
- book を軽くする機能は v4.5.5-nikque.20・newedax.5 から使えます。ほかの機能は v4.5.5-nikque.19・newedax.4 でも使えます。
