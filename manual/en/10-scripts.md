# 10. Command scripts

[Contents](README.md) · Previous: [9. Tools and files of games](09-tools.md) · Next: [11. Settings](11-settings.md)

A command script is a UTF-8 text file with one command per line.

- Input/Output → Run a command script… runs it from top to bottom.
- Stop the command script stops it.
- A script to run at startup is set in `[script] startup=` of `config.ini`.

Rules:

- Lines starting with `#` are comments.
- A command that starts work (`list load`, `search`, `learn start`, `edax`, `tool`, `estimate`, `wait`, `book reduce` and so on) waits until the work finishes before the next line runs.
- When `wait` or `waitgame` ends because its time ran out, the log says so.
- Relative paths are relative to the folder of the script.
- If a line has an error, it is written to the log and the script stops.
- `run` runs another script (up to 4 levels deep).

## Commands

| Command | What it does |
|---|---|
| `moves <game>` | reads a game (coordinates, board text, bitboards) |
| `board <board text>` | starts from that position |
| `play <move>` | plays from the position shown |
| `new` / `goto <n\|end>` / `back [n]` / `forward [n]` | from the start, to move n, back, forward |
| `book on\|off` / `eval on\|off` / `numbers on\|off` | use the book, values, move numbers |
| `level <0-60>` / `umi best\|loss\|off` | search level, umigame numbers |
| `precision on\|off` | compute the book accuracy or not |
| `analysis backward\|forward\|off` | the order of the search of the game in the background (backward analysis, forward analysis, none) |
| `stop` | stops the background work |
| `pv on\|off` | the expected line of the move under the mouse on the board, or not |
| `openingmarks on\|off` | the marks on the lines of the openings, or not |
| `book reload` | reads the book again (restarts the main engine) |
| `book reduce <X> <Y> [here]` | makes the book lighter (from the initial position; `here`: from the position shown); counted, then done without asking |
| `engine nikque\|newedax` | kind of engine |
| `design standard\|paw\|toggle` / `effects on\|subdued\|off` | design (`toggle` switches it), effects |
| `tab moves\|book\|record\|list\|tree` | tab |
| `rotate <0-7>` / `cellinfo none\|positions\|level\|stage\|umi` | board orientation, number on the squares |
| `lang ja\|en\|zh-Hans\|zh-Hant` | language |
| `wait [s]` | waits for the analysis of the position shown (30 s at most by default) |
| `estimate` | finds the moves of the position on the board |
| `keep` / `bookmark <n>` / `bookmarks on\|off` | bookmark the position now, go to bookmark n, show the row of bookmarks or not |
| `screenshot [file]` / `savelog <file>` | the screen as a PNG, the log to a file |
| `list load\|add\|save <file>` | open, add, save the game list |
| `list sort\|unique\|normalize\|delete\|next\|prev\|addgame\|clear` | game list operations |
| `list truncate <n>` / `list shuffle [seed]` / `list scatter black\|white\|both [seed]` | cut, shuffle, random orientations |
| `list start <n>` / `list select <n>` | the move to open at, open game n |
| `list book [loss] [depth] [count] [add]` | book lines into the list |
| `search [exact]` | search the list (`exact`: not other orientations) |
| `learn <book command>` / `learn start` | add a job to the learning queue (from this position), start the queue |
| `edax <command>` | sends a command to the main engine and waits for its end |
| `tool <command>` | runs on the tool engine (solve, bench, count, perft, estimate, base) |
| `run <file>` | another script |
| `game free\|black\|white\|engines [n]` | starts a game (`black`: you as black) |
| `comlevel <n>` / `clock <min>` / `ponder on\|off` | computer level, time, thinking on your turn |
| `opening search\|best\|wide [width] [moves]` | opening moves |
| `go` / `takeback` | let the computer play a move, take back |
| `waitgame [s]` | waits for the end of the game (all the games, computer against computer) |
| `compare on\|off` | compare mode |
| `tree open\|close\|go <moves>` / `tree root` / `tree toggle <moves>` | opens, closes or goes to a row (moves from the root), makes the position shown the root, as ▸ / ▾ of a row |
| `log <text>` / `quit` | writes to the log, closes the GUI |

## File names and characters

- In `edax` and `tool` commands, a file name enclosed in `"…"` is converted to a form the engine can open: its short name, or a copy in the temporary folder.
- If a file in `"…"` does not exist, it is treated as a file to be written.
- Commands are converted to the characters the engine can read (the system code page) before they are sent.
- A folder name with Japanese characters can be passed with or without `"…"`. Use `"…"` when the name contains spaces.
- A name without `"…"` is passed to the engine as is, so a relative path is relative to the engine folder.
- `learn` commands run later as jobs in the queue, so their `"…"` names are sent unchanged. Use names without spaces.
- If a command contains characters the engine cannot read, it is written to the log and the script stops.

## Examples

```text
# learn the missing lines after f5d6 with the book shown
engine nikque
moves f5d6
wait 60
learn book store
learn book deviate 2 4
learn start
edax book save "D:\books\after learning.dat"
screenshot after.png
quit
```

```text
# make the book smaller, then save it under another name (the original book is kept)
engine nikque
book reduce 2 4
edax book save "D:\books\smaller.dat"
book reload
quit
```
