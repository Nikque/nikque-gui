# 6. Learning

[Contents](README.md) · Previous: [5. Book](05-book.md) · Next: [7. Games](07-play.md)

Book learning is done by the main engine running book commands.

- The GUI has the engine play the game up to the current position, sends the command, and waits for it to finish.
- Meanwhile, the search engine keeps providing values.
- The accuracy stages shown are those from before the learning, drawn faint.

## Learning commands

Choose them from the Learning menu. X and Y are the range values in the right panel.

| Item | Engine command |
|---|---|
| book deviate X Y from this position (also deviate2, deviate3) | `book deviate X Y` and so on |
| book enhance X Y from this position | `book enhance X Y` |
| book leaf expand X Y from this position | `book leaf expand X Y` |
| book fill 2 X Y from this position (also fill2) | `book fill 2 X Y`, `book fill2 2 X Y` |
| book play | every line extended with best moves to the depth |
| Put this game into the book | `book store` |
| Put a file of games into the book; learn a list of lines; compare a file with the book | `book add`, `book learn`, `book check` |
| Recompute the values; relink the book | `book negamax`, `book fix` |
| Check the solved positions again | `book correct` |
| Recompute the leaves (4 kinds) | `book leaf recalculate` to `recalculate4` |

- Before it starts, a confirmation window shows the command and the book file.
- While learning is running, the learning commands cannot be chosen.

## Stop and continue

- Learning → Stop learning…:
  - A book command cannot be interrupted, so the main engine is closed and restarted.
  - The window shows the newest intermediate save file and its time. Intermediate save files are `.dev`, `.dev2`, `.store` and so on, next to the book.
- Learning → Continue from the file saved on the way…:
  - If an intermediate save file is newer than the book, it is loaded.
  - The last learning command resumes from the same position.
  - The last command is stored in `[learn] last_command` of `config.ini`.

Whether the work already done is searched again when a command continues depends on the command. This is how the engines included with GUI 0.4.6 and later work:

- `deviate` and the like, `enhance`, `leaf expand`, `fill`, `play`, `add`: the work already done is not searched again. The expanded moves and the added positions are already in the book.
- `correct`, `leaf recalculate`:
  - Each position searched again is marked in the book. When the command continues, only the positions without a mark are searched.
  - The marks are removed when the command ends.
  - Their intermediate save files are `.err` for `correct`, and `.leaf` and so on for `leaf recalculate`.
- `learn`:
  - The lines learned are recorded in `<book>.learn` next to the book.
  - When the command continues, the lines of the game file that were already learned are skipped.
  - This file is removed when the whole game file has been learned.

With the engines included with GUI 0.4.5 and before:

- `correct`, `leaf recalculate` and `learn` started over from the beginning when they continued.
- An intermediate save file with marks cannot be read. These engines see it as a damaged book.

## Learning queue

- Learning → Put all the missing lines into the learning queue: each missing line in the Book tab becomes one job in the queue. Each job does the following:
  1. play to its position
  2. `book store`
  3. the command of the accuracy base, with X Y
- Start the learning queue… runs the jobs in order. If you stop it, the remaining jobs stay in the queue.
- Clear the learning queue empties the queue.
- `learn <command>` in a command script also adds a job ([10. Command scripts](10-scripts.md)).

## Learning settings

Learning → Learning settings…: settings sent to the main engine (`[learn_options]` of `config.ini`).

- Settings changed during learning are sent when that learning has ended. The learning in progress keeps the settings it started with.
- When the main engine is not running, they are sent when it starts.
- An empty box means the engine's own setting, but there is no way to send that to the engine. So a running main engine keeps the value sent before, and the log says so.
- The engine's own setting comes back the next time the main engine starts. The main engine starts again with:
  - Engine → Restart
  - opening or reading the book again
  - starting the GUI

| Item | Meaning |
|---|---|
| positions expanded at once, games stored at once | a number or auto (empty: the engine's setting) |
| save every n minutes, save every n rounds | files saved on the way (0 minutes: not by time; 0 rounds: only at the end) |
| random seed, the same book every time | to make the same book again |
| largest number of rounds of leaf recalculation | it ends at a round that changes nothing |
| save to .mrg after merge, to .store after store | on or off |

## Notes

- After a command that changes the book, the book file may be saved when the engine is closed. Back up an important book before learning.
- For this reason, after a command that changes the book, closing the main engine waits for the engine to save the book and end. There is no time limit. The main engine is closed when you:
  - close the GUI
  - open or read the book again
  - switch or restart the engines
- The engine writes the book to another file beside it (`<book name>.tmp.<number>`), then replaces the book with that file. So the book file stays unchanged while it is being saved.
- You can use the window while the GUI waits.
  - The log says "Waiting for the main engine to save its book and end".
  - The row below shows "Saving the book" and the time so far.
  - The log also says when the save has ended.
  - If the engine could not save the book, an ERROR line of the main engine appears before that. The book file stays unchanged.
  - The next step is done after the save has ended. For example: closing the GUI, or starting the main engine with the new book.
  - While the GUI waits, nothing that uses the main engine can be done: learning, writing from the book, opening or reading the book again, switching or restarting the engines, and so on. A message asks you to wait until the save has ended.
  - A script goes on to its next line after the save has ended.
- If you close again while the GUI waits, it asks whether to cut the save.
  - Yes: the engine is ended at once and the GUI closes.
    - The book file stays as it was last saved. What was learned after that is lost.
    - The intermediate save files remain.
    - If a half-written `.tmp.<number>` file is left beside the book, you can delete it.
  - No: the GUI closes after the save has ended.
  - When `quit` in a script closes the GUI, it does not ask. The GUI closes after the save has ended.
- If you close while learning is running, the engine is ended without waiting. What was learned after the last intermediate save is lost.
- The speed and memory use of learning depend on the engine and the size of the book. Learning a large book takes a long time.
