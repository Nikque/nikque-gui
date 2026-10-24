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

| Item | Meaning |
|---|---|
| positions expanded at once, games stored at once | a number or auto (empty: the engine's setting) |
| save every n minutes, save every n rounds | files saved on the way (0 minutes: not by time; 0 rounds: only at the end) |
| random seed, the same book every time | to make the same book again |
| largest number of rounds of leaf recalculation | it ends at a round that changes nothing |
| save to .mrg after merge, to .store after store | on or off |

## Notes

- After a command that changes the book, the book file may be saved when the engine is closed. Back up an important book before learning.
- The speed and memory use of learning depend on the engine and the size of the book. Learning a large book takes a long time.
