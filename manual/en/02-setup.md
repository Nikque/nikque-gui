# 2. Setting up

[Contents](README.md) · Previous: [1. Introduction](01-introduction.md) · Next: [3. The screen](03-screen.md)

## Installing

1. Unpack the ZIP (`nikque-gui-0.4.0.zip`) anywhere. No installation is needed.
2. Start `nikque-gui.exe`.

Contents of the folder:

| File | What it is |
|---|---|
| `nikque-gui.exe` | the GUI |
| `config.ini` | settings ([11. Settings](11-settings.md)) |
| `lang\*.txt` | the words of the screen (Japanese, English, Simplified and Traditional Chinese) |
| `engines\nikque` | Edax v4.5.5-nikque.27, C version |
| `engines\newedax` | newedax.13, C++ version (the 64-slot build) |
| `readme.txt` | short notes |

- Each engine folder contains:
  - `wEdax-x86-64-v4.exe` (AVX-512)
  - `wEdax-x86-64-v3.exe` (AVX2)
  - `wEdax-x86-64.exe` (other x64 CPUs)
  - `data\eval.dat`
- A `config.ini` from 0.2.0 or earlier (with the section `[nikque18]`) is read as is. It is then saved with `[nikque]`.

## Choosing the engine

Switch with Engine → nikque type / newedax type.

- Both engines have the same book analysis commands and give the same results.
- newedax.13 uses a newly trained evaluation function (64 slots). Its `eval.dat` is about 108 MB and is not the `eval.dat` of nikque.
- Use the `eval.dat` from the release of the same version as the newedax engine. With the `eval.dat` of an earlier newedax, of eval2 or of nikque, the engine runs but its evaluation is wrong. The GUI then tells you so in a message ([12. Troubleshooting](12-troubleshooting.md)).
- The 48-slot build (`edax-4.5.5-newedax.13-48.zip` in the newedax release) is not included. To use it, unpack that ZIP into another folder and write its `bin` folder in `dir=` of `[newedax]` in `config.ini`. Use the 48-slot `eval.dat` of the same ZIP with the 48-slot exe files (if they are mixed up, the evaluation is wrong without any message).
- Memory: one newedax.13 engine uses about 160 MB (`wEdax-x86-64.exe`) to about 275 MB (`wEdax-x86-64-v4.exe`, AVX-512). The GUI starts up to five engines (main, search, tool, game and compare), so with newedax for all of them it is about 0.8 to 1.4 GB. A larger hash table or more threads use more, and the book adds its own size.
- The book format has not changed, so the books you used with earlier versions are read as they are. Their values, however, were made with the earlier evaluation function if they were learned with an earlier version.

## The book

No book is included. Use your own book in one of these ways:

- Put your `book.dat` in `data\book.dat` of the engine folder.
- Choose it with Book → Open another book…. You can also drag and drop the file onto the window. Setting `book=` in `[nikque]` and `[newedax]` of `config.ini` does the same.

Without a book, the engine creates a small book (the start position only) in `data\book.dat`. You can replace it with your book later.

- The engine reads and writes the book file. After learning or a command that changes the book, the book file may be saved when the engine is closed. Back up an important book first.
- The nikque type cannot read newedax's book format 2 (`NEDXBOOK`). The GUI tells you when you open such a book.

## Using engines you already have

Set the engine folder in `dir=` of `[nikque]` and `[newedax]` in `config.ini`.

- With `exe=auto`, the executable is chosen to match the CPU.
- A file name in `exe=` is used as given.

The GUI uses the book analysis commands (`book moveinfo` and others), so it requires v4.5.5-nikque.19 or v4.5.5-newedax.4 or later.

- With an older engine, the top bar shows "No book analyses (old engine)". The book values, the tree and the Book tab stay empty. Search values are still shown.
- Making the book lighter (`book reduce`) requires v4.5.5-nikque.20 or v4.5.5-newedax.5. An older engine reports this and does nothing.

## Language

Choose it in Settings → Language. The Chinese words on the screen are machine translations.

## File names and characters

- The engines read file names in the system code page (Shift_JIS on Japanese Windows).
- If a name cannot be passed to the engine, the GUI does one of the following. Such names include names with characters outside that code page, and names that are too long.
  - It passes the name with its folder in the short (8.3) form.
  - It reports that the name cannot be passed.
- The file name of a book itself is never converted to the short form, because the engine saves the book under the name it was given.
- A book whose name contains characters outside the system code page (for example, Hangul on Japanese Windows) cannot be opened. Rename it.
