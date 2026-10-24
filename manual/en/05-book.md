# 5. Book

[Contents](README.md) · Previous: [4. Analysis](04-analysis.md) · Next: [6. Learning](06-learning.md)

## Book tab

![The Book tab](../images/en-02-book.png)

The Book tab shows:

- Whether this position is in the book.
- The positions after this position, by level (and the total).
- Best lines:
  - the number of lines where both sides play only best moves (their end positions)
  - the number of those ends that are not solved
- Missing lines: the lines to the leaves missing for the next stage, after the move selected in the Moves tab or with Up/Down.
  - Click a line to play up to the position where its leaf is to be added.
  - This does not work during a game against the computer.

With a large book, counting takes time near the start ("Counting…" is shown).

## Tree of the book

![The Tree tab](../images/en-03-tree.png)

Open it with Book → Tree of the book, or with the Tree tab. It shows the book moves (links) from the current position as a tree that you can expand and collapse.

- Each row shows: the disc of the side to play, the move, the value (the book value from that side), the accuracy stage, umigame numbers and Lv.
- A leaf (the best move from a book position that is not yet in the book) is shown as a faint row. It cannot be expanded.
- Click a row to go to its position and expand it.
- Click ▸ / ▾ to expand or collapse that row only.
- While you move forward from the root on the board, the tree stays the same and the row of the current position is highlighted.
- If you go back before the root or switch to another game, that position becomes the new root.
- When there are too many rows, the tree is cut off and a message says so. Collapse some rows.

## Output

- Book → Write the missing lines…:
  - Writes the missing lines shown in the Book tab to a text file.
  - One line per entry (the moves from the start and the leaf move).
  - The lines can also be added to the learning queue ([6. Learning](06-learning.md)).
- Book → Write the best lines…:
  - From this position, writes the lines where both sides play best moves (or moves within a loss) up to their ends. One line per entry.
  - Options: the loss, the depth, whether to stop at solved positions, and which ends to write (all ends, or only the ends not solved).
- Input/Output → Game list → Book lines into the list…: puts the same lines into the game list instead of a file.

## Book contents to the log

Book → Book contents to the log shows one of the following in the log:

- Whole book (`book info`)
- Counts (`book stats`)
- This position (`book show`)

## Another book, reading again

- Book → Open another book…, Recent books: restarts the main engine with that book. You can also drag and drop a file onto the window.
- Book → Read again: use this after the book file was changed outside the GUI.
- Book → Use the book (B): when off, no book values are shown, only search values.

## Maintenance

The Book → Book maintenance and output menu:

| Item | Engine command |
|---|---|
| Save, Save as | `book save` |
| New empty book | `book new` |
| Change the book depth | `book depth` |
| Show as problems | `book problem` |
| Merge another book | `book merge` |
| Keep only this position and after | `book subtree` |
| Keep only lines where one side plays best | `book prune` |
| Remove what deviate3 X Y cannot reach, to make the book lighter | `book reduce` (below) |
| Write to / read back from a text file | `book export` / `book import` |
| Write the best lines as games | `book extract` |

After a command that changes the book (subtree, prune, reduce, import, new and so on), the book file may be overwritten when the engine is closed. The confirmation window also says so. Back up the book first.

## Making the book lighter (book reduce)

Book → Book maintenance and output → Remove what deviate3 X Y cannot reach, to make the book lighter (book reduce)….

This keeps only the positions that `book deviate3 X Y` reaches from a given position, removes the others, and makes the book smaller. It is an engine feature available since v4.5.5-nikque.20 and newedax.5.

### Steps

1. In the window, choose:
   - the largest loss of a move X (0 to 129)
   - the largest sum of losses Y (0 to 254)
   - where to start (the initial position, or the position shown)

   X and Y start at the range values. Usually choose the initial position. If you start from the position shown, the positions from the initial position up to it are removed too.
2. The GUI first has the engine count the positions (`book reducecount`, which does not change the book). The window shows the numbers of positions kept and removed.
3. Click OK to run `book reduce X Y` in the same way as a learning command. When it finishes, the log shows the numbers of positions kept and removed.

### What is kept

- The given position, and the positions reached from it by moves that each lose at most X discs, with at most Y discs lost in total.
- This is the same range that `book deviate3 X Y` follows when it looks for positions to expand.
- The values of the kept positions stay as they were after being recomputed first.

### When nothing is done

The book is not made lighter in these cases:

- Input/Output → Stop the background work is chosen while the positions are being counted, or after counting but before the window appears.
- The engine closes (including when you switch the book or the engine).
- The engine closes while the window is open. In this case OK does nothing either (since 0.2.1).

The GUI reports the problem and does nothing in these cases:

- The book after the position is damaged (engines since v4.5.5-nikque.21 and newedax.6). Damaged means a link to a square already taken, or a link back to the same position.
- The engine is too old to know the command (nikque.19, newedax.4 and earlier).

### Reusing the count

Engines since v4.5.5-nikque.22 and newedax.7 reuse the count if the book has not changed since it was counted (the engine setting `book-reduce-reuse`).

- After OK, the positions to keep are chosen from the count, without counting again.
- The larger the book, the more time this saves after OK.

### Keeping the original book

The reduced book is saved to its file when the engine is closed. To keep the original book, do one of the following:

- Copy the file first.
- Save with Save as…, and then restore the original with Read again.

### From a script

In a script, use `book reduce X Y` (from the position shown: `book reduce X Y here`). It runs without the confirmation window ([10. Command scripts](10-scripts.md)).
