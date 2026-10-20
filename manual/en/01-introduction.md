# 1. Introduction

[Contents](README.md) · Next: [2. Setting up](02-setup.md)

## What it does

- **Values**: the values of all the legal moves on the board. A move in the book shows its book value; the others show a search value.
- **Accuracy of the book**: for every move, how complete the book is after it (stages 0 to 7), with colors and effects.
- **Book analyses**: the lines to the leaves missing for the next stage, umigame numbers (the number of line ends to learn to know every line after the move), the positions by level, the number of best lines and their output, a tree of the book lines.
- **Book learning**: the learning commands (deviate, enhance, fill and others) from the position shown. A learning run can be stopped and continued from the file saved on the way.
- **Games**: a person against the computer, or the computer on both sides; time of a game, opening moves drawn from the book.
- **Games**: game lists (a million games can be handled), search of the games with a position, kept lines, setting up a position, finding the moves that lead to a position.
- **Tools**: the engine tools (solve, bench, perft and others), the files of games (base).
- **Command scripts**: operations written in a text file and run in turn.

## How it works

The GUI starts the engines as child processes and talks to them with text commands. The engine reads the book file; the GUI never reads it. The engines are started for their roles:

| Engine | Role |
|---|---|
| Main engine | reads the book; book analyses (in a background thread) and learning |
| Search engine | the values of all the moves, searched without the book (`[engine] search=1`) |
| Tool engine | solve, bench, base and the others, started for each job |
| Play engine | the computer's moves in a game (without the book) |
| Compare engine | in compare mode, the engine of the other kind searches |

So the values keep coming while the book is learning, and the analysis of the board goes on during a game.

## License

- nikque GUI: MIT License (`LICENSE.txt`).
- The included engines (Edax nikque, newedax): GNU GPL v3, separate programs. The GPL text and the place of the source are in `LICENSE.txt` and `SOURCE.txt` of `engines\nikque18` and `engines\newedax`.

## System

Windows, 64-bit (x64); checked on Windows 11. The engine file is chosen for the CPU among the AVX-512, AVX2 and other x64 builds (`exe=auto`).
