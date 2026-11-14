# 12. Troubleshooting

[Contents](README.md) · Previous: [11. Settings](11-settings.md)

| What you see | Why, and what to do |
|---|---|
| "No engine" in the top bar | the engine executable or `eval.dat` is not found; the log shows where it was looked for. Check `dir=` and `exe=` of `[nikque]` and `[newedax]` in `config.ini` |
| "the eval file (eval.dat) it reads is not made for this version of the engine" | newedax.13 reads the `eval.dat` of an earlier newedax, of eval2 or of nikque. The engine runs, but its evaluation is wrong; the log also shows the `WARNING` line of the engine. Use the included `engines\newedax\data\eval.dat` (from the release of the same version as the engine), and check `eval=` and `dir=` of `[newedax]` in `config.ini` (the message is shown once while the GUI runs; the log and the bottom line show it each time) |
| "No book analyses (old engine)" | the engine is older than v4.5.5-nikque.19 or newedax.4. Use the included engines or later versions |
| "This engine has no book reduce" | making the book lighter needs v4.5.5-nikque.20 or newedax.5. Use the included engines |
| "This main engine cannot write the lines deviate walks" | writing the lines deviate walks needs v4.5.5-nikque.26, eval2.14 or newedax.11. An earlier engine would not read the word export and would expand the book, so the GUI does not send it. Use the included engines |
| "The main engine is still reading its book, so it is not known yet whether it can write the lines deviate walks" | the engine says whether it has the export when it has read its book. Try again when the log shows where the book is and its size (a script too) |
| The book accuracy is heavy or slow | turn off Analysis → Compute the book accuracy: only the book values are shown. Moving the position stops the work on the position before and starts the new one; Input/Output → Stop the background work stops the work now |
| The accuracy or the umigame numbers take time to appear | the values of the book come first. The accuracy, the umigame numbers and the position counts are computed when the values of the positions of the game are there |
| No accuracy and the like while learning, finding the moves of a position, reading book lines into the list, writing the best lines, writing the lines deviate walks or counting before making the book lighter | the engine cannot answer book analyses during these. The values are read directly from the book file, and the rest is computed when the work ends. While learning, the values are those of the book as last saved. While the book is being saved (Book → Save, the command `book save` and the like), the book file is not read either, so as not to keep the engine from saving it: the values not shown yet come when the save has ended |
| Discs and numbers appear on other squares when the mouse is over a move | that is the expected line; View → Show on the board the expected line of the move under the mouse turns it off (off at first) |
| "Book path not usable" | the book name is too long or has letters not in the code page of the system, and no short form can be given. Copy the book to a place with a short name, or rename it |
| "Reading the book" for long | a large book takes time to read (the engine reads it). The values of the book are shown meanwhile; the accuracy, the umigame numbers and the other analyses come after it. When it is read, the log says where the book is and its size |
| "Counting…" stays in the Book tab | near the start of a large book, counting the positions takes time. Turning off Analysis → Also count the book positions makes it faster |
| "This file's path cannot be given to the engine" | it has spaces or letters the engine cannot read, and neither a short name nor the temporary folder could be used. Choose another place |
| "The command has letters the engine cannot read" | a command of the command box or of a script has letters not in the code page of the system. It was not sent |
| Learning does not end | learning a large book takes long. Learning → Stop learning… stops it; Continue from the file saved on the way… goes on later |
| The book file changed | after learning or a command that changes the book, the book is saved when the engine is closed. Copy it first |
| "Waiting for the main engine to save its book and end" when closing and the like, and the GUI does not close for long | after a command that changes the book, the engine saves the book as it ends. A large book takes minutes, so the GUI waits without a time limit (the window can be used meanwhile; the row below shows the time so far). When the save has ended, what was asked is done (closing, opening a book, ...). Closing again asks whether to cut the save; cut, the book file does not get what was learned ([6. Learning](06-learning.md), Notes) |
| No values on the board | check H and Analysis → Show the values. A finished game shows none |
| Wrong language | Settings → Language |
| Other keys | `[keys]` of `config.ini` (edit it with the GUI closed) |

- The log (L) shows the engine responses and the GUI messages.
- View → Save the log… saves it.
