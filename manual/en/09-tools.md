# 9. Tools and files of games

[Contents](README.md) · Previous: [8. Games, lists and bookmarks](08-games.md) · Next: [10. Command scripts](10-scripts.md)

## Tool engine

The tools of the Engine menu run on a "tool engine" without the book, started for each job, from the position shown. Their output goes to the log. The main engine (book analyses) and the search engine keep running.

| Item | Engine command |
|---|---|
| Solve problems… | `solve <obf file>` |
| Measure the speed… | `bench <n>` |
| Count lines or positions… | `count games\|positions\|shapes <depth>` |
| Count lines (perft)… | `perft <depth>` |
| Estimate the number of lines… | `estimate <n>` |
| Stop the tool | the tool engine is closed |

## Files of games (base)

Input/Output → Files of games (base): files of games (txt, ggf, sgf, pgn, wtb…) on the tool engine.

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

The commands that rewrite a file ask first. The same file as input and output is refused.

## Command box

Engine → Command box (to the main engine)…: an Edax command sent to the main engine as it is. `|` separates several commands. It runs from the position shown, and the book analyses wait for its end. The last command is kept in `[tools] last_command` of `config.ini`.

- The command is sent in the letters the engine reads (the code page of the system). A command with letters the engine cannot read is not sent, and the GUI says so.
- A file name with spaces or letters the engine cannot read cannot be given in the command box. Use the menu items (which open a file window) or `"…"` in a command script ([10. Command scripts](10-scripts.md)).

## How names are given

The engines read a file name up to a space and open it in the code page of the system. When a name chosen in a file window has spaces or other letters, the GUI gives its short (8.3) form. On a drive without short names (often D:), the file goes through the temporary folder, and a file written there is moved to its place after the job (the log says so).
