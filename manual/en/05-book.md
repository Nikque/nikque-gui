# 5. Book

[Contents](README.md) · Previous: [4. Analysis](04-analysis.md) · Next: [6. Learning](06-learning.md)

## Book tab

![The Book tab](../images/en-02-book.png)

- Whether this position is in the book.
- The positions after this position, by level (and in all).
- Best lines: the number of lines where both sides play best moves only (their end positions), and the ends not solved.
- Missing lines: after the move chosen in the Moves tab or with Up/Down, the lines to the leaves missing for the next stage. Click a line to play up to the position where its leaf is to be added (not during a game against the computer).

With a large book this takes time near the start ("Counting…").

## Tree of the book

![The Tree tab](../images/en-03-tree.png)

Book → Tree of the book, or the Tree tab: the book moves (links) from the position shown, in a tree that opens and closes.

- Each row: the disc of the side that plays, the move, the value (the book value from that side), the accuracy stage, umigame numbers, Lv.
- A leaf (the best move of a book position that is not in the book yet) is a faint row that does not open.
- Click a row to go to its position and open it; ▸ / ▾ opens or closes that row only.
- While you go on from the root the tree stays and the row of the position shown is colored; going back before the root or to another game makes a new root.
- When there are too many rows the tree stops and says so: close some rows.

## Output

- Book → Write the missing lines…: the missing lines of the Book tab, one a line (the moves from the start and the leaf move), to a text file. They can also go to the learning queue ([6. Learning](06-learning.md)).
- Book → Write the best lines…: from this position, the lines where both sides play best moves (or moves within a loss) to their ends, one a line (the loss, the depth, whether to stop at solved positions, all ends or the ends not solved only).
- Input/Output → Game list → Book lines into the list…: the same lines into the game list.

## Book contents to the log

Book → Book contents to the log: whole book (`book info`), counts (`book stats`), this position (`book show`).

## Another book, reading again

- Book → Open another book… (or drop a file on the window), Recent books. The main engine is started again with it.
- Book → Read again: after the file was changed outside.
- Book → Use the book (B): off, no book value is shown, only search values.

## Maintenance

Book → Book maintenance and output:

| Item | Engine command |
|---|---|
| Save, Save as | `book save` |
| New empty book | `book new` |
| Change the book depth | `book depth` |
| Show as problems | `book problem` |
| Merge another book | `book merge` |
| Keep only this position and after | `book subtree` |
| Keep only lines where one side plays best | `book prune` |
| Write to / read back from a text file | `book export` / `book import` |
| Write the best lines as games | `book extract` |

After a command that changes the book (subtree, prune, import, new…), the book file may be overwritten when the engine is closed; the confirmation window says so. Copy the book first.
