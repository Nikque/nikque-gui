# 8. Games, lists and kept lines

[Contents](README.md) · Previous: [7. Games](07-play.md) · Next: [9. Tools and files of games](09-tools.md)

## Copy and paste a game

- Input/Output → Copy the game (up to this position), Paste a game. With the keys, press Ctrl+C or Ctrl+V twice.
- Forms read: coordinates (`f5d6c3…`), board text (64 squares and the side to move), bitboards, the PARAM form of blogs.
- Fix the game: the game is fixed, and the games played after it are compared with it: a red frame on bad moves, a mark where they differ.

## Position text and bitboards

Input/Output → Position text and bitboards: copy the board text, a problem line (obf), the bitboards (decimal, hexadecimal, Ntest form). Read a position… reads one.

- Decimal and hexadecimal are "black white side" (a1 is bit 0, h8 bit 63; side X or O).
- The Ntest form is "empty-squares discs-of-the-side-to-move side".

## Setting up a position

E starts; E again or Enter finishes. While setting up: left click black → white → empty, right click empty, T side to move, Delete removes all discs, I the start position, Esc cancels.

## Finding the moves of a position

Input/Output → Find the moves of this position (up to move 20): makes a line that leads to the position on the board. A line through book positions only is looked for first (`book findline`); if there is none, it is searched without the book. When none is found, the log says that the search gave up, or that no line leads to the position.

## Game list

A text file with one game a line (`f5d6c3…`, or `<64 squares> <X|O>;moves` from another start). It is shown in the List tab; click a line to open the game.

Input/Output → Game list: Open, Add, Save, Add this game to the list, Book lines into the list, Sort (shorter first, then by the moves), Join the same games, Cut (every game to n moves), Shuffle (seed), One orientation (first move f5, also at later symmetric positions), Random orientations (at symmetric positions), Previous / Next game (Ctrl+Shift+Left/Right), Read this game again / Remove it (Ctrl+Shift+Down/Up), Empty the list, Open games at move.

- A list of a million games can be handled. Heavy work runs in the background; Stop the background work stops it.
- Stopping One orientation or Random orientations half way leaves the games turned so far as they are (the same games turned another way).

## Search

Input/Output → Search the list (games with this position): the games of the list where this position appears (other orientations too), shown in the List tab; click a line to open that position. Clear the search result clears it.

## Kept lines

- M (Game → Keep this line) keeps this line. A new game, a pasted game, a position set up and a line found also keep the line before (no limit; `kept.txt` next to the exe).
- In the Kept tab, click to choose, twice to show.
- View → Kept lines → Compare with the chosen line: marks the game as a fixed game does (red frame on bad moves, marks where it differs).
- Remove the chosen line, Remove all kept lines….

## Images

- Input/Output → Screenshot (Ctrl+P): the screen as a PNG in the `screenshots` folder next to the exe (`[view] screenshot_dir` of `config.ini` changes the place).
- Save the board as an image (PNG)…: the board alone.

## Opening names

With `opening.ini` (lines `name=moves`) next to the exe, the top bar shows the opening name. The file is not included.
