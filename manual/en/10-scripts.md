# 10. Command scripts

[Contents](README.md) · Previous: [9. Tools and files of games](09-tools.md) · Next: [11. Settings](11-settings.md)

A UTF-8 text file with one command a line, run with Input/Output → Run a command script…; Stop the command script stops it. The script run at the start is set in `[script] startup=` of `config.ini`.

- Lines starting with `#` are notes.
- A command that starts work (`list load`, `search`, `learn start`, `edax`, `tool`, `estimate`, `wait`…) waits for its end before the next line. A `wait` or `waitgame` that ends by its time says so in the log.
- Relative names are from the folder of the script.
- A wrong line is written to the log and the script stops.
- `run` runs another script (4 levels at most).

## Commands

| Command | What it does |
|---|---|
| `moves <game>` | reads a game (coordinates, board text, bitboards) |
| `board <board text>` | starts from that position |
| `play <move>` | plays from the position shown |
| `new` / `goto <n\|end>` / `back [n]` / `forward [n]` | from the start, to move n, back, forward |
| `book on\|off` / `eval on\|off` / `numbers on\|off` | use the book, values, move numbers |
| `level <n>` / `umi best\|loss\|off` | search level, umigame numbers |
| `book reload` | reads the book again (restarts the main engine) |
| `engine nikque18\|newedax` | kind of engine |
| `design standard\|paw` / `effects on\|subdued\|off` | design, effects |
| `tab moves\|book\|record\|list\|kept\|tree` | tab |
| `rotate <0-7>` / `cellinfo none\|positions\|level\|stage\|umi` | board orientation, number on the squares |
| `lang ja\|en\|zh-Hans\|zh-Hant` | language |
| `wait [s]` | waits for the analysis of the position shown (30 s at most by default) |
| `estimate` | finds the moves of the position on the board |
| `keep` | keeps this line |
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

## File names and letters

- In `edax` and `tool` commands, a file name in `"…"` is given as the engine can open it (its short form, or through the temporary folder). A file not there is taken as a file to be written.
- Commands are sent in the letters the engine reads (the code page of the system). A folder name with Japanese letters can be given with or without `"…"` (use `"…"` when it has spaces). A name without `"…"` goes to the engine as it is, so a relative name is from the engine folder.
- The commands of `learn` run later as jobs of the queue, so their `"…"` names are sent as they are: write names without spaces.
- A command with letters the engine cannot read is written to the log and the script stops.

## Example

```text
# learn the missing lines after f5d6 with the book shown
engine nikque18
moves f5d6
wait 60
learn book store
learn book deviate 2 4
learn start
edax book save "D:\books\after learning.dat"
screenshot after.png
quit
```
