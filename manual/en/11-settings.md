# 11. Settings

[Contents](README.md) · Previous: [10. Command scripts](10-scripts.md) · Next: [12. Troubleshooting](12-troubleshooting.md)

## View

The View menu:

- Design: Standard, Paw.
- Effects: On, Subdued (no motion), Off. Effects of the top stage (stage 7): Strong, Normal, Weak.
- Board orientation: 8 orientations (R in turn). The game itself is turned.
- Move numbers on the discs, Number on the squares ([4. Analysis](04-analysis.md)).
- Tabs, Kept lines, Hide the right panel (P), Open the log (L), Save the log.

## config.ini

Settings → Open config.ini opens it in Notepad. The GUI also writes it when it closes, so close the GUI before editing it by hand.

| Section | Main keys |
|---|---|
| `[engine]` | `kind=nikque18\|newedax`, `search=1` (use the search engine) |
| `[nikque18]`, `[newedax]` | `dir` (engine folder), `exe` (`auto` or the file name), `eval`, `book` |
| `[analysis]` | `level`, `threads` (0: all), `hash` (bits of the hash table; 0: the engine's), `umi`, `x`, `y`, `base`, `stages`, `compare` |
| `[play]` | `level`, `opening` (`search\|best\|wide`), `width`, `until`, `clock`, `ponder`, `end_keep`, `end_list` |
| `[view]` | `design`, `effects`, `language`, `numbers`, `cell_info`, `fps`, `font`, `opening_file`, `screenshot_dir` |
| `[window]` | size and place, `tab`, `panel_w` |
| `[list]` | `start_ply` (the move to open games of the list at; -1: the last) |
| `[learn]` | the last learning command and its position (for Continue from the file saved on the way) |
| `[learn_options]` | learning settings sent to the engine ([6. Learning](06-learning.md)) |
| `[tools]` | the values of the tool windows, the last command of the command box |
| `[script]` | `startup` (the script run at the start) |
| `[keys]` | the keys |

## Keys

The default keys (all can be changed in `[keys]`, for example `back=Left, Backspace`; Help → Keys lists the keys of now):

| Key | Action |
|---|---|
| Left / Right | one move back / forward (a right click also goes back) |
| Ctrl+Left / Right | 10 moves |
| N, Home / End | first / last position |
| Up, Down / Enter, Space | choose a move / play it |
| B / U / H | use the book, umigame numbers, hide the values |
| R | board orientation |
| M | keep this line |
| E | start or finish setting up a position |
| Ctrl+C / Ctrl+V | copy / paste a game (press twice) |
| Ctrl+P | screenshot |
| Ctrl+Shift+Left / Right | previous / next game of the list |
| Ctrl+Shift+Up / Down | remove / read again the game of the list |
| P / L | right panel, log |

## Files

Made next to the exe: `kept.txt` (kept lines), `screenshots\`. Read: `opening.ini` (opening names, not included).
