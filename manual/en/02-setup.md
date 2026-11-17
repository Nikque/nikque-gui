# 2. Setting up

[Contents](README.md) · Previous: [1. Introduction](01-introduction.md) · Next: [3. The screen](03-screen.md)

## Installing

1. Unpack the ZIP (`nikque-gui-0.4.6.zip`) anywhere. No installation is needed.
2. Start `nikque-gui.exe`.

Contents of the folder:

| File | What it is |
|---|---|
| `nikque-gui.exe` | the GUI |
| `config.ini` | settings ([11. Settings](11-settings.md)) |
| `lang\*.txt` | the words of the screen (Japanese, English, Simplified and Traditional Chinese) |
| `engines\nikque` | Edax v4.5.5-nikque.29, C version |
| `engines\newedax` | Heath (formerly newedax): heath.16, C++ version (the 64-slot build) |
| `readme.txt` | short notes |

- Each engine folder contains:
  - `wEdax-x86-64-v4.exe` (AVX-512)
  - `wEdax-x86-64-v3.exe` (AVX2)
  - `wEdax-x86-64.exe` (other x64 CPUs)
  - `data\eval.dat`
- A `config.ini` from 0.2.0 or earlier (with the section `[nikque18]`) is read as is. It is then saved with `[nikque]`.

## Mac (trial version)

From 0.4.3 there is a Mac version (`nikque-gui-0.4.6-mac.zip`). It runs on macOS 13 or later, on Apple silicon and Intel Macs. It is still a trial version: please report anything that does not work well.

1. Unpack the ZIP anywhere.
2. The app is not signed, so macOS stops it as it is. Once, at first, type this in Terminal (replace `~/Downloads/nikque-gui-0.4.6-mac` with the folder you unpacked):

   ```
   xattr -dr com.apple.quarantine ~/Downloads/nikque-gui-0.4.6-mac
   ```

   The app and the engines in the folder can then all be opened.
3. Open `nikque GUI.app`.

The folder holds the same as the Windows version: `config.ini`, `lang` and `engines` are next to `nikque GUI.app`.

- Each engine folder holds `mEdax-arm64` (Apple silicon), `mEdax-x64-modern` (Intel) and `data/eval.dat`. With `exe=auto`, the one for your Mac is chosen.
- The nikque engine is the one of the release ZIP of v4.5.5-nikque.29. The release of heath.16 has no engine for macOS, so the Heath engine is built for this distribution from the source of its tag (64 slots; see `engines/newedax/SOURCE.txt`).
- Names are separated with `/`. Places written in `config.ini` (`dir=` and others) are written the Mac way (for example `/Users/name/books/book.dat`).
- Keys: use Command (⌘) where Windows uses Ctrl (Command+C copies the moves, for example). A right click is a two-finger click on a trackpad, or a click with control held.
- A file whose name contains a space cannot be given to the engine directly; the GUI gives it through a temporary folder (the log says so).
- If you move the app into Applications, keep `config.ini`, `lang` and `engines` in the same folder as the app.

## Linux (trial version)

From 0.4.4 there is a Linux version (`nikque-gui-0.4.6-linux.tar.gz`). It runs on x86-64 Ubuntu 22.04 or later (it should run on other distributions of that time or later), with GTK 3. It is still a trial version: please report anything that does not work well.

1. Unpack it anywhere:

   ```
   tar -xzf nikque-gui-0.4.6-linux.tar.gz
   ```

2. Start `nikque-gui` in it (open it from the file manager, or type `./nikque-gui` in a terminal).

The folder holds the same as the Windows version: `config.ini`, `lang` and `engines` are next to `nikque-gui`.

- GTK 3 is there on a usual desktop Ubuntu. If it is not, install it with `sudo apt install libgtk-3-0`.
- Japanese and Chinese are drawn with the fonts of the system (Noto Sans CJK and others). If the letters are boxes, install them with `sudo apt install fonts-noto-cjk`.
- Each engine folder holds `lEdax-x86-64-v4` (AVX-512), `lEdax-x86-64-v3` (AVX2), `lEdax-x86-64` (other x64 CPUs) and `data/eval.dat`. With `exe=auto`, the one for your CPU is chosen.
- The engines are built for this distribution from the sources of their tags (v4.5.5-nikque.29, and heath.16 with 64 slots; see `engines/*/SOURCE.txt`). The Linux programs of the release ZIP of nikque.29 are for Ubuntu 24.04 or later, and the release of heath.16 has no engine for Linux.
- Names are separated with `/`. Places written in `config.ini` (`dir=` and others) are written the Linux way (for example `/home/name/books/book.dat`).
- A file whose name contains a space cannot be given to the engine directly; the GUI gives it through a temporary folder (the log says so).
- If you move the folder, move `nikque-gui`, `config.ini`, `lang` and `engines` together.

## Android (trial version)

From 0.4.4 there is an Android version (`nikque-gui-0.4.6-android.apk`), for arm64 devices (most phones and tablets of today) with Android 10 or later. It is still a trial version, checked on an emulator only (not yet on a real device): please report anything that does not work well.

1. Download the APK on the device, open it and install it. It is not from a store, so the system first asks you to allow installing apps from this source.
2. Open "nikque GUI". The first start, and the first start after an update, take a moment to prepare the files.

The screen and the touches:

- A row of buttons at the top: "Menu" (the menus of the Windows version, as lists), to the start, one move back, one move forward, to the end.
- Held upright, the board is at the top and the panel (the tabs Moves, Book, Record, List and Tree, and the graph) under it. Held sideways, the layout is that of the Windows version.
- A tap is a click (a tap on a square plays there), a long press is a right click (on the board: one move back; on the picture of a bookmark: its menu), two fingers moved up or down scroll.
- The Back button of the device asks whether to quit. Quitting saves the settings and closes the engines.

Not in this version:

- Learning and tidying the book, the game lists, the scripts, and everything that asks for a file (opening a book, exports, saving pictures). They are not in the menus either.
- Typing in the row of the moves (its Copy and Paste buttons work).
- There is no way yet to use a book of your own (the engine makes a small book of the initial position).

Other notes:

- Both engines are in the app (`aEdax-arm64-v8a` of the release ZIPs of v4.5.5-nikque.29 and heath.16). They run as separate programs.
- Heath uses about 160 to 275 MB of memory for each engine. On a device with little memory, Engine → Edax nikque is lighter.
- The engines may go on thinking while another app is in front. Quit with Back when you are done.
- The APK is signed with a trial key. If a later version cannot be installed over this one (the system says the keys differ), uninstall this one first (the settings start again).

## Choosing the engine

Switch with Engine → nikque type / Heath type (formerly newedax). The first start uses the Heath type (up to 0.4.0 it was the nikque type). The type you choose is saved in `config.ini` and used at the next start.

- Both engines have the same book analysis commands and give the same results.
- From newedax.13 on, Heath uses a newly trained evaluation function (64 slots). Its `eval.dat` is about 108 MB and is not the `eval.dat` of nikque.
- Use the `eval.dat` from the release of the same version as the Heath engine. With the `eval.dat` of an earlier Heath, of eval2 or of nikque, the engine runs but its evaluation is wrong. The GUI then tells you so in a message ([12. Troubleshooting](12-troubleshooting.md)).
- The 48-slot build (`edax-4.5.5-heath.16-48.zip` in the Heath release) is not included. To use it, unpack that ZIP into another folder and write its `bin` folder in `dir=` of `[newedax]` (the Heath settings) in `config.ini`. Use the 48-slot `eval.dat` of the same ZIP with the 48-slot exe files (if they are mixed up, the evaluation is wrong without any message).
- Memory: one heath.16 engine uses about 160 MB (`wEdax-x86-64.exe`) to about 275 MB (`wEdax-x86-64-v4.exe`, AVX-512). The GUI starts up to five engines (main, search, tool, game and compare), so with Heath for all of them it is about 0.8 to 1.4 GB. A larger hash table or more threads use more, and the book adds its own size.
- The book format has not changed, so the books you used with earlier versions are read as they are. Their values, however, were made with the earlier evaluation function if they were learned with an earlier version.

## The book

No book is included. Use your own book in one of these ways:

- Put your `book.dat` in `data\book.dat` of the engine folder.
- Choose it with Book → Open another book…. You can also drag and drop the file onto the window. Setting `book=` in `[nikque]` and `[newedax]` of `config.ini` does the same.

Without a book, the engine creates a small book (the start position only) in `data\book.dat`. You can replace it with your book later.

- The engine reads and writes the book file. After learning or a command that changes the book, the book file may be saved when the engine is closed. Back up an important book first.
- The nikque type cannot read Heath's book format 2 (`NEDXBOOK`). The GUI tells you when you open such a book.

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
