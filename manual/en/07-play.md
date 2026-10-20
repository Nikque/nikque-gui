# 7. Games

[Contents](README.md) · Previous: [6. Learning](06-learning.md) · Next: [8. Games, lists and kept lines](08-games.md)

The computer thinks on a "play engine" without the book, so the analysis of the board (values, accuracy of the book) goes on during a game. Press H to hide the values.

## Kind of game

Game → Kind of game:

- People only (free play): the usual way; a person plays both sides.
- Person black, computer white / Person white, computer black: from the start position.
- Computer against computer…: the number of games in a row (a new game after each).
- Let the computer play a move, Take back (to the person's turn), Stop the game (free play).

The top bar shows the kind of game and, with a time, the time left.

## Level and time

- Level of the computer…: the search depth.
- Time of a game… (minutes, each side): with a time, the time of a move comes from the time left; the side whose time runs out loses. 0 means no time (the computer plays by its Lv). It is used from the next new game.
- Think on the person's turn too.
- At the end of a game…: keep the game, add it to the game list.

## Opening moves

Game → Opening moves:

- No book (search only)
- The best book move
- A book move drawn within a width: a book move (leaves included; as Edax, among the links and the leaf of the position) within the given loss from the best, drawn at random.

Width and moves… sets the width (loss) and up to which move the book is used.
