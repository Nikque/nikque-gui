# 12. Troubleshooting

[Contents](README.md) · Previous: [11. Settings](11-settings.md)

| What you see | Why, and what to do |
|---|---|
| "No engine" in the top bar | the engine executable or `eval.dat` is not found; the log shows where it was looked for. Check `dir=` and `exe=` of `[nikque]` and `[newedax]` in `config.ini` |
| "No book analyses (old engine)" | the engine is older than v4.5.5-nikque.19 or newedax.4. Use the included engines or later versions |
| "This engine has no book reduce" | making the book lighter needs v4.5.5-nikque.20 or newedax.5. Use the included engines |
| The book accuracy is heavy or slow | turn off Analysis → Compute the book accuracy: only the book values are shown. Moving the position stops the work on the position before and starts the new one; Input/Output → Stop the background work stops the work now |
| Discs and numbers appear on other squares when the mouse is over a move | that is the expected line; View → Show on the board the expected line of the move under the mouse turns it off (off at first) |
| "Book path not usable" | the book name is too long or has letters not in the code page of the system, and no short form can be given. Copy the book to a place with a short name, or rename it |
| "Reading the book" for long | a large book takes time to read (the engine reads it) |
| "Counting…" stays in the Book tab | near the start of a large book, counting the positions takes time. Turning off Analysis → Also count the book positions makes it faster |
| "This file's path cannot be given to the engine" | it has spaces or letters the engine cannot read, and neither a short name nor the temporary folder could be used. Choose another place |
| "The command has letters the engine cannot read" | a command of the command box or of a script has letters not in the code page of the system. It was not sent |
| Learning does not end | learning a large book takes long. Learning → Stop learning… stops it; Continue from the file saved on the way… goes on later |
| The book file changed | after learning or a command that changes the book, the book is saved when the engine is closed. Copy it first |
| No values on the board | check H and Analysis → Show the values. A finished game shows none |
| Wrong language | Settings → Language |
| Other keys | `[keys]` of `config.ini` (edit it with the GUI closed) |

- The log (L) shows the engine responses and the GUI messages.
- View → Save the log… saves it.
