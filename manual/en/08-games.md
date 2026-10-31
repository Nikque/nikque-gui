# 8. Games, lists and bookmarks

[Contents](README.md) · Previous: [7. Games](07-play.md) · Next: [9. Tools and files of games](09-tools.md)

## Copy and paste a game

- Input/Output → Copy the game (up to this position), Paste a game.
- The keys are Ctrl+C and Ctrl+V. Press Ctrl+V twice.
- Formats that can be pasted:
  - coordinates (`f5d6c3…`)
  - board text (64 squares and the side to move)
  - bitboards
  - the PARAM format used in blogs
- Fix the game: fixes the current game. Games played afterward are compared with it: bad moves get a red frame, and the points where the games differ are marked.

## Position text and bitboards

Input/Output → Position text and bitboards copies one of these:

- the board text
- a problem line (obf)
- the bitboards (decimal, hexadecimal, Ntest form)

Read a position… reads one in.

- Decimal and hexadecimal are "black white side" (a1 is bit 0, h8 is bit 63; the side is X or O).
- The Ntest form is "empty-squares discs-of-the-side-to-move side".

## Setting up a position

Press E to start. Press E again or Enter to finish. While setting up:

| Input | Action |
|---|---|
| Left click | black → white → empty |
| Right click | empty |
| T | side to move |
| Delete | removes all discs |
| I | the start position |
| Esc | cancels |

## Finding the moves of a position

Input/Output → Find the moves of this position (up to move 20) builds a line of moves that leads to the position on the board.

1. It first looks for a line through book positions only (`book findline`).
2. If there is none, it searches without the book.

If no line is found, the log says that the search could not finish, or that no line leads to the position.

## Game list

A text file with one game per line, in one of these forms:

- `f5d6c3…`
- `<64 squares> <X|O>;moves` (from another start position)

The list is shown in the List tab. Click a line to open that game.

The Input/Output → Game list menu:

- Open, Add, Save
- Add this game to the list
- Book lines into the list
- Sort (shorter first, then by the moves)
- Join the same games
- Cut (every game to n moves)
- Shuffle (seed)
- One orientation (first move f5, also at later symmetric positions)
- Random orientations (at symmetric positions)
- Previous / Next game (Ctrl+Shift+Left/Right)
- Read this game again / Remove this game from the list (Ctrl+Shift+Down/Up)
- Empty the list
- Open games at move

Notes:

- Lists of a million games can be handled.
- Heavy work runs in the background. Input/Output → Stop the background work stops it.
- If you stop One orientation or Random orientations partway, the games turned so far stay turned (the same games in another orientation).

## Search

Input/Output → Search the list (games with this position) finds the games in the list where the current position appears.

- Other orientations of the position are included.
- The results are shown in the List tab. Click a line to open that position.
- Clear the search result clears the results.

## Bookmarks

![The row of bookmarks](../images/en-08-bookmarks.png)

A bookmark stores a game and a position in it. Pictures of the bookmarked positions are shown just above the graph, oldest on the left.

### Adding a bookmark

- M bookmarks the current position. In the menus, this is Game → Bookmark the position now; the same item is also in View → Bookmarks.
- A different position of the same game becomes a separate bookmark.

### Automatic bookmarks

- In these cases, the previous game is bookmarked automatically:
  - a new game
  - pasting a game
  - setting up a position
  - finding the moves of a position
- There is no limit on the number of bookmarks. They are stored in `kept.txt` next to the exe.
- When the same game comes again, its automatic bookmark moves to the last position and remains a single bookmark.
- A bookmark made with M does not move. A different position of the same game becomes a new bookmark (since 0.2.1).
- Bookmarks in a `kept.txt` from 0.2.0 or earlier are read as automatic bookmarks.

### Using bookmarks

- Click a picture to go to that position of its game.
- Right-click a picture to open a menu:
  - Go to the chosen bookmark
  - Compare with the game of the chosen bookmark: compares the games in the same way as a fixed game (red frame on bad moves, marks where they differ).
  - Delete the chosen bookmark
- Hovering over a picture shows the bookmark number and its moves in the bottom row.
- View → Bookmarks → Delete all the bookmarks… deletes every bookmark.
- View → Bookmarks → Show the row of bookmarks shows or hides the row.

## Images

- Input/Output → Screenshot (Ctrl+P): saves the screen as a PNG in the `screenshots` folder next to the exe. You can change the folder with `[view] screenshot_dir` in `config.ini`.
- Save the board as an image (PNG)…: saves the board only.

## Opening names

The opening name is shown under the board ([The screen](03-screen.md)), read from `opening.ini` next to the exe (included).

- It is a text file with one `name=moves` entry per line.
- Moves are written from the start, with f5 as the first move.
- Leading spaces only indicate the hierarchy.
- Lines starting with `//` are comments.
- You can edit it with Notepad.
- To use another file, set `[view] opening_file=` in `config.ini`.
