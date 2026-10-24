# 9. Tools and files of games

[Contents](README.md) · Previous: [8. Games, lists and bookmarks](08-games.md) · Next: [10. Command scripts](10-scripts.md)

## Tool engine

The tools in the Engine menu run on a "tool engine" that does not read the book.

- A tool engine is started for each job.
- It runs from the current position.
- Its output goes to the log.
- The main engine (book analyses) and the search engine keep running in the meantime.

| Item | Engine command |
|---|---|
| Solve problems… | `solve <obf file>` |
| Measure the speed… | `bench <n>` |
| Count lines or positions… | `count games\|positions\|shapes <depth>` |
| Count lines (perft)… | `perft <depth>` |
| Estimate the number of lines… | `estimate <n>` |
| Stop the tool | the tool engine is closed |

## Files of games (base)

Input/Output → Files of games (base) processes game files (txt, ggf, sgf, pgn, wtb and others) on the tool engine.

| Item | What it does |
|---|---|
| Change the format (convert) | writes another format |
| Join the same games (unique) | one game for each set of the same games |
| Count the common positions (compare) | compares two files |
| Find endgame mistakes (check) | from a number of empty squares |
| Correct endgame mistakes (correct) | rewrites the file |
| Play unfinished games to the end (complete) | rewrites the file |
| Write positions to a problem file (problem) | obf |
| Write positions as FEN (tofen) | FEN |

- Commands that rewrite a file ask for confirmation first.
- Choosing the same file for input and output is refused.

## Command box

Engine → Command box (to the main engine)… sends an Edax command directly to the main engine.

- Separate several commands with `|`.
- The command runs from the current position. The book analyses wait until it finishes.
- The last command is stored in `[tools] last_command` of `config.ini`.
- The command is converted to the characters the engine can read (the system code page). If it contains characters the engine cannot read, it is not sent, and the GUI tells you.
- A file name with spaces or characters the engine cannot read cannot be passed in the command box. Use one of these instead:
  - the menu items (which open a file dialog)
  - `"…"` in a command script ([10. Command scripts](10-scripts.md))

## How file names are passed

The engines read a file name up to the first space and open it in the system code page.

When a name chosen in a file dialog has spaces or other such characters, the GUI does the following:

- It passes the short (8.3) form of the name.
- On a drive without short names (often D:), it copies the file to the temporary folder and passes that copy. A file the engine writes there is moved to its proper place after the job (the log says so).
