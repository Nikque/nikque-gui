# 4. Analysis

[Contents](README.md) · Previous: [3. The screen](03-screen.md) · Next: [5. Book](05-book.md)

![Accuracy stages on the squares](../images/en-05-cellinfo.png)

## Values

- When the position changes, the main engine starts the book analyses and the search engine starts searching all the moves.
- Values are disc differences from the side to move.
- A move in the book shows its book value (with the book Lv). Other moves show the search value (searched to the search level).
- When you move to another position, the computation for the previous position stops and the new position is computed immediately.
- Computations of other positions for the graph wait until the current position is done.
- Analysis → Show the values toggles the values (H hides them).
- Analysis → Compute again discards the stored values and computes them again.

## Search level

Analysis → Search level… sets the Edax level (0 to 60) with a slider or a number.

- The menu shows the current level.
- Higher levels search deeper and take longer (21 is a good starting point).
- You can also set it in the Settings → Range… window.

## Searching the positions of the game in the background

When the search engine is idle, it also searches the other positions of the game in the background and fills in the graph. Choose the order in the Analysis menu:

| Item | What it does |
|---|---|
| Backward analysis | from the last position back; a value is corrected by the value of the next move, and a corrected value has a ★ on its Lv (the default) |
| Forward analysis | from the first position on; no value is corrected |
| Do not search the positions of the game in the background | only the position shown is searched |

## Stopping the background work

Input/Output → Stop the background work stops:

- the work on the game list
- the book analyses (the accuracy, the points of the graph, the counts)
- the count before book reduce (the book is not made lighter)
- the background search of the positions of the game

The work starts again when the position changes. To keep it stopped, do one of the following:

- Turn off Compute the book accuracy (see below).
- Choose Do not search the positions of the game in the background.

## Accuracy stages of the book (0 to 7)

For each move, the stage shows how complete the book is after that move. Hover over the dots in the top bar to see the colors and their meanings.

| Stage | Meaning |
|---|---|
| 0 | a move not in the book (search value) |
| 1 | in the book, but the best line does not reach a position solved to the end |
| 2 | the best line reaches the end, but leaves near the best remain |
| 3 to 7 | `book deviate x y` from the position after the move would add nothing (by default 3 is 1/1, 4 is 2/2 … 7 is 5/5) |

- Book → Accuracy based on selects the base command: deviate, deviate2 or deviate3.
- The ranges (x/y) of stages 3 to 7 are set in Settings → Range….
- Missing in the Moves table is the number of leaves missing for the next stage. The Book tab shows the lines to those leaves (the missing lines).
- From stage 3 on, the numbers have effects (light, gloss, stars and so on).
  - View → Effects: On, Subdued (no motion) or Off.
  - Lower in the same menu, choose the strength of the effects of the top stage (stage 7): Strong, Normal or Weak.

### Not computing the book accuracy

If computing the accuracy stages is too slow for a large book, turn off Analysis → Compute the book accuracy. The Range… window has the same checkbox.

When it is off:

- A book move shows only its value, in white (without a Lv).
- Accuracy in the Moves table shows "—".
- Umigame numbers are still computed according to their setting.

When you turn it back on, the current position is computed again.

## Umigame numbers

The smallest number of line ends you must memorize to know every line after the move (by default, the best moves of both sides).

- The numbers for black and white are shown as "black:white".
- A position reached by different move orders is counted once.

Analysis → Umigame numbers selects how they are counted:

- Best lines only
- Up to the loss X, Y (the range values)
- Do not show

Settings → Range… also lets you count within a range of values and set the depth to follow.

- With a wide range, a number that may not be the smallest gets "≦".
- A number too large to compute is shown as "too large".

## Range

Loss/move X and Total Y at the top of the right panel are shared by:

- the accuracy stages
- the missing lines
- the umigame numbers (when counted with losses)
- the learning commands

The Range… window also sets:

- the accuracy base
- whether the book accuracy is computed
- the range of each stage
- the umigame condition
- the search level
- the number of threads
- the hash table size

Changing the threads or the hash table size restarts the engines.

## Book positions

With Analysis → Also count the book positions turned on, the number of book positions after each move is counted.

- With a large book, this takes time, especially near the start.
- The levels and the best lines in the Book tab appear only after the count finishes.

## Number on the squares

View → Number on the squares shows one of the following at the lower left of each square:

- book positions
- Lv (L)
- accuracy stage (S)
- umigame numbers (black:white)

## Compare mode

Analysis → Compare mode starts the engine of the other kind for a search without the book. The Moves table gets a Compare column.

This lets you compare the evaluation functions of the nikque and newedax types side by side.
