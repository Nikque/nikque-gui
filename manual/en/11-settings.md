# 11. Settings

[Contents](README.md) · Previous: [10. Command scripts](10-scripts.md) · Next: [12. Troubleshooting](12-troubleshooting.md)

## View

The View menu:

- Switch the design (D): toggles between standard and paw. You can also choose one in Design.
- Effects: On, Subdued (no motion), Off. Lower in the same menu, choose the effects of the top stage (stage 7): Strong, Normal, Weak.
- Board orientation: 8 orientations (R cycles through them). The game itself is transformed.
- Move numbers on the discs.
- Show on the board the expected line of the move under the mouse (off by default).
- Mark the moves on the lines of the openings (on by default).
- Number on the squares ([4. Analysis](04-analysis.md)).
- Tabs.
- Bookmarks (Show the row of bookmarks and others).
- Hide the right panel (P), Open the log (L), Save the log….

## config.ini

Settings → Open config.ini opens it in Notepad.

The GUI also writes the file when it closes. Close the GUI before editing the file by hand.

| Section | Main keys |
|---|---|
| `[engine]` | `kind=nikque\|newedax`, `search=1` (use the search engine) |
| `[nikque]`, `[newedax]` | `dir` (engine folder), `exe` (`auto` or the file name), `eval`, `book` |
| `[analysis]` | `level`, `threads` (0: all), `hash` (bits of the hash table; 0: the engine's), `umi`, `x`, `y`, `base`, `stages`, `precision` (1: compute the book accuracy), `backward`, `forward` (the order of the search of the game), `compare` |
| `[play]` | `level`, `opening` (`search\|best\|wide`), `width`, `until`, `clock`, `ponder`, `end_keep`, `end_list` |
| `[view]` | `design`, `effects`, `language`, `numbers`, `opening_marks` (0: no marks on the lines of the openings), `pv` (1: the expected line on the board), `cell_info`, `fps`, `font`, `opening_file`, `screenshot_dir` |
| `[window]` | size and place, `tab`, `panel_w`, `bookmarks` (1: the row of bookmarks) |
| `[list]` | `start_ply` (the move to open games of the list at; -1: the last) |
| `[learn]` | the last learning command and its position (for Continue from the file saved on the way) |
| `[learn_options]` | learning settings sent to the engine ([6. Learning](06-learning.md)) |
| `[tools]` | the values of the tool windows, the last command of the command box |
| `[script]` | `startup` (the script run at the start) |
| `[keys]` | the keys |

## Keys

The default key assignments:

- All of them can be changed in `[keys]` (for example, `back=Left, Backspace`).
- Help → Keys lists the current assignments.

| Key | Action |
|---|---|
| Left / Right | one move back / forward (a right click also goes back) |
| Ctrl+Left / Right | 10 moves |
| N, Home / End | first / last position |
| Up, Down / Enter, Space | choose a move / play it |
| B / U / H | use the book, umigame numbers, hide the values |
| R | board orientation |
| D | switch the design |
| M | bookmark the position now |
| E | start or finish setting up a position |
| Ctrl+C / Ctrl+V | copy / paste a game (press twice) |
| Ctrl+P | screenshot |
| Ctrl+Shift+Left / Right | previous / next game of the list |
| Ctrl+Shift+Up / Down | remove / read again the game of the list |
| P / L | right panel, log |

## Files

- Created next to the exe: `kept.txt` (bookmarks), `screenshots\`.
- Read: `opening.ini` (opening names, included).
