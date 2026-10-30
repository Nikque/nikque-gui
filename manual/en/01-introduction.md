# 1. Introduction

[Contents](README.md) · Next: [2. Setting up](02-setup.md)

## What it does

- **Values**: shows the value of every legal move on the board. A move in the book shows its book value; other moves show a search value.
- **Accuracy of the book**: for each move, shows how complete the book is after that move (stages 0 to 7), with colors and effects.
- **Book analyses**:
  - the lines to the leaves missing for the next stage
  - umigame numbers (how many line ends you must memorize to know every line after the move)
  - the positions by level
  - the number of best lines, and writing them to a file
  - a tree of the book lines
- **Book learning and maintenance**:
  - Runs the learning commands (deviate, enhance, fill and others) from the current position.
  - You can stop a learning run and continue it later from its intermediate save file.
  - You can also make the book smaller by removing the positions that `book deviate3 X Y` does not reach.
- **Games**: a person against the computer, or computer against computer. Time controls are available, and opening moves can be drawn from the book.
- **Game records**:
  - game lists (up to a million games)
  - searching the games for a position
  - bookmarks (chosen from pictures of their positions)
  - setting up a position
  - finding the moves that lead to a position
- **Tools**: the engine tools (solve, bench, perft and others) and conversion of game files (base).
- **Command scripts**: a text file of operations, run one after another.

## How it works

The GUI starts the engines as child processes and communicates with them through text commands. The engine reads the book file. The GUI reads only the positions it needs for the values while the engine is still reading the book.

Separate engines are started for separate roles:

| Engine | Role |
|---|---|
| Main engine | reads the book; book analyses (in a background thread) and learning |
| Search engine | the values of all the moves, searched without the book (`[engine] search=1`) |
| Tool engine | solve, bench, base and the others, started for each job |
| Play engine | the computer's moves in a game (without the book) |
| Compare engine | in compare mode, the engine of the other kind searches |

As a result, values keep appearing while the book is learning, and the board analysis continues during a game.

## License

- nikque GUI: MIT License (`LICENSE.txt`).
- The included engines (Edax nikque, newedax): GNU GPL v3. They run as separate programs.
- The GPL text and the location of the source code are in `LICENSE.txt` and `SOURCE.txt` in `engines\nikque` and `engines\newedax`.

## System requirements

- 64-bit (x64) Windows. Tested on Windows 11.
- The engine executable is chosen to match the CPU, from the AVX-512, AVX2 and generic x64 builds (`exe=auto`).
