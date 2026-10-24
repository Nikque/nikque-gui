# 7. Games

[Contents](README.md) · Previous: [6. Learning](06-learning.md) · Next: [8. Games, lists and bookmarks](08-games.md)

The computer thinks on a "play engine" that does not read the book. As a result, the board analysis (values, accuracy of the book) continues during a game. Press H to hide the values.

## Kind of game

Game → Kind of game:

- People only (free play): the normal mode. A person plays both sides.
- Person black, computer white / Person white, computer black: starts from the start position.
- Computer against computer…: set the number of games to play in a row. A new game starts after each one.

The Game menu also has these items:

- Let the computer play a move
- Take back (to the person's turn)
- Stop the game (free play)

The top bar shows the kind of game and, with a time control, the time left.

## Level and time

- Level of the computer…: the search depth.
- Time of a game… (minutes, per side):
  - With a time control, the time for each move is based on the time left.
  - The side whose time runs out loses.
  - 0 means no time control (the computer plays by its Lv).
  - The setting takes effect from the next new game.
- Think on the person's turn too: the computer also thinks during the person's turn.
- At the end of a game…: keep the game (bookmark it), add it to the game list.

## Opening moves

Game → Opening moves:

- No book (search only)
- The best book move
- A book move drawn within a width: a book move within the given loss from the best, chosen at random.
  - Leaves are included.
  - As in Edax, the moves are chosen from the links and the leaf of the position.

Width and moves… sets the width (loss) and the move up to which the book is used.
