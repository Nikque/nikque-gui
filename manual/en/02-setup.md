# 2. Setting up

[Contents](README.md) · Previous: [1. Introduction](01-introduction.md) · Next: [3. The screen](03-screen.md)

## Installing

1. Unpack the ZIP (`nikque-gui-0.2.3.zip`) anywhere. No installation is needed.
2. Start `nikque-gui.exe`.

Contents of the folder:

| File | What it is |
|---|---|
| `nikque-gui.exe` | the GUI |
| `config.ini` | settings ([11. Settings](11-settings.md)) |
| `lang\*.txt` | the words of the screen (Japanese, English, Simplified and Traditional Chinese) |
| `engines\nikque` | Edax v4.5.5-nikque.25, C version |
| `engines\newedax` | newedax.10, C++ version |
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
- newedax uses an evaluation function from the eval2 line (a different `eval.dat`).

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
