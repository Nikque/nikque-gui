# 6. Learning

[Contents](README.md) · Previous: [5. Book](05-book.md) · Next: [7. Games](07-play.md)

Book learning is done by the main engine with the book commands. The GUI has the engine play the game up to the position shown, sends the command and waits for its end. Meanwhile the search engine keeps giving values (the accuracy stages are those of before the learning, drawn faint).

## Learning commands

The Learning menu. X and Y are the range values of the right panel.

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

A confirmation window shows the command and the book file first. While learning, the learning commands cannot be chosen.

## Stop and continue

- Learning → Stop learning…: a book command cannot be stopped in the middle, so the main engine is closed and started again. The window shows the newest file saved on the way (`.dev`, `.dev2`, `.store`… next to the book) and its time.
- Learning → Continue from the file saved on the way…: if a file saved on the way is newer than the book, it is read and the last learning command goes on from the same position. The last command is kept in `[learn] last_command` of `config.ini`.

## Learning queue

- Learning → Put all the missing lines into the learning queue: each missing line of the Book tab becomes a job: play to its position, `book store`, then the command of the accuracy base with X Y.
- Start the learning queue… runs them in turn. When stopped, the jobs left stay in the queue. Clear the learning queue empties it.
- `learn <command>` of a command script also adds a job ([10. Command scripts](10-scripts.md)).

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

- After a command that changes the book, the book file may be saved when the engine is closed. Copy an important book before learning.
- The speed and memory of learning depend on the engine and the size of the book; learning a large book takes long.
