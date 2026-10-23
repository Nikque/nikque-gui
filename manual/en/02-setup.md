# 2. Setting up

[Contents](README.md) · Previous: [1. Introduction](01-introduction.md) · Next: [3. The screen](03-screen.md)

## Installing

1. Unpack the ZIP (`nikque-gui-0.2.0.zip`) anywhere. There is nothing to install.
2. Start `nikque-gui.exe`. `config.ini`, the `lang` folder and `readme.txt` are in the same folder.

| File | What it is |
|---|---|
| `nikque-gui.exe` | the GUI |
| `config.ini` | settings ([11. Settings](11-settings.md)) |
| `lang\*.txt` | the words of the screen (Japanese, English, Simplified and Traditional Chinese) |
| `engines\nikque18` | Edax v4.5.5-nikque.20, C version |
| `engines\newedax` | newedax.5, C++ version |
| `readme.txt` | short notes |

The folder name `nikque18` follows the section name `[nikque18]` of `config.ini`; it holds nikque.20. Each engine folder has `wEdax-x86-64-v4.exe` (AVX-512), `wEdax-x86-64-v3.exe` (AVX2), `wEdax-x86-64.exe` (other x64 CPUs) and `data\eval.dat`.

## Choosing the engine

Engine → nikque type / newedax type. Both have the same book analysis commands with the same answers. newedax uses the evaluation of the eval2 line (another `eval.dat`).

## The book

No book is included.

- Put your `book.dat` in `data\book.dat` of the engine folder, or
- choose it with Book → Open another book… (or drop the file on the window). `book=` in `[nikque18]` and `[newedax]` of `config.ini` does the same.

Without a book, the engine makes a small book (the start position only) in `data\book.dat`. Replace it with your book later.

- The engine reads and writes the book file. After learning or a command that changes the book, the book file may be saved when the engine is closed. Copy an important book first.
- The nikque type cannot read the book form 2 of newedax (`NEDXBOOK`); the GUI says so when such a book is opened.

## Engines you already have

Write the engine folder in `dir=` of `[nikque18]` and `[newedax]` in `config.ini`. With `exe=auto` the executable is chosen for the CPU; a file name in `exe=` is used as it is.

The GUI uses the book analysis commands (`book moveinfo` and the others), so it needs v4.5.5-nikque.19 or v4.5.5-newedax.4 or later. With an older engine the top bar says "No book analyses (old engine)", and the book values, the tree and the Book tab stay empty (the search values are shown). Making the book lighter (`book reduce`) needs v4.5.5-nikque.20 or v4.5.5-newedax.5; an older engine says so and does nothing.

## Language

Settings → Language. The Chinese words on the screen are machine translations.

## Names and letters

- The engines read file names in the code page of the system (Shift_JIS on a Japanese Windows). A name they cannot take (letters not in that code page, too long) is given with its folder in the short (8.3) form, or the GUI says it cannot be given.
- The file name of a book itself is never given in the short form (the engine writes the book under the name it was given). A book whose name has letters not in the code page of the system (Hangul on a Japanese Windows, for example) cannot be opened: rename it.
