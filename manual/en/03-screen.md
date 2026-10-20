# 3. The screen

[Contents](README.md) · Previous: [2. Setting up](02-setup.md) · Next: [4. Analysis](04-analysis.md)

![The Book tab](../images/en-02-book.png)

## Top bar

From the left: the move number and the side to move, the discs, the engine (kind, version, build date) and the search level; during a game, the kind of game and the time left. At the right, the colors of the accuracy of the book (weak to strong); hover over a dot to see the meaning of its stage.

The bar also says when no engine runs, while the book is read, when the book is off, with an old engine, and when the book path cannot be given.

## Board

- The values (disc differences from the side to move) of all the legal moves. The small number at the lower right is the search depth (Lv). A value searched to the end is a purple number with "E".
- The colors and effects of the numbers show the accuracy stage of the book ([4. Analysis](04-analysis.md)). The best move has a thick frame, moves near the best a thin one, and the move chosen in the Moves tab a white frame.
- Left click plays a move; a right click or Left goes one move back.
- View → Number on the squares shows book positions, Lv, the accuracy stage or umigame numbers at the lower left of each square.

## Right panel

At the top, the range (loss per move X and total Y), used by the accuracy stages, the missing lines, the umigame numbers and the learning commands. The tabs below:

| Tab | What it shows |
|---|---|
| Moves | a table of all the moves: Move, Value, Accuracy, Umigame, Missing (leaves missing for the next stage), Positions, Lv. Click a heading to sort. Up/Down chooses a move, Enter plays it |
| Book | whether the position is in the book, the positions by level, the best lines (their end positions, the ends not solved), the missing lines after the chosen move |
| Record | the moves of the game (number and move); click to go to that position |
| List | the game list and the search results ([8. Games, lists and kept lines](08-games.md)) |
| Kept | the kept lines (same chapter) |
| Tree | the book lines from the position shown ([5. Book](05-book.md)) |

P hides the panel. Drag the edge with the board to change the width of the panel, and the edge between the table and the graph to change the height of the graph.

## Graph

At the lower right, the values of the positions of the game. The vertical line is the position shown. Click the graph to go to the nearest position of the game.

## Log

The bottom row is the last line of the log. Click it or press L to open it: the answers of the engines, the messages and the output of the tools. View → Save the log… writes it to a file.

![The Record tab](../images/en-04-record.png)
