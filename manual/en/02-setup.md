# 2. Setting up

[Contents](README.md) · Previous: [1. Introduction](01-introduction.md) · Next: [3. The screen](03-screen.md)

## Installing

1. Unpack the ZIP (`nikque-gui-0.4.11.zip`) anywhere. No installation is needed.
2. Start `nikque-gui.exe`.

Contents of the folder:

| File | What it is |
|---|---|
| `nikque-gui.exe` | the GUI |
| `config.ini` | settings ([11. Settings](11-settings.md)) |
| `lang\*.txt` | the words of the screen (Japanese, English, Simplified and Traditional Chinese) |
| `engines\nikque` | Edax v4.5.5-nikque.34, C version |
| `engines\newedax` | Heath (formerly newedax): heath.21, C++ version (the 64-slot build) |
| `readme.txt` | short notes |

- Each engine folder contains:
  - `wEdax-x86-64-v4.exe` (AVX-512)
  - `wEdax-x86-64-v3.exe` (AVX2)
  - `wEdax-x86-64.exe` (other x64 CPUs)
  - `data\eval.dat`
- A `config.ini` from 0.2.0 or earlier (with the section `[nikque18]`) is read as is. It is then saved with `[nikque]`.

## Mac (trial version)

From 0.4.3 there is a Mac version (`nikque-gui-0.4.11-mac.zip`).

- It runs on macOS 13 or later, on both Apple silicon and Intel Macs.
- It is still a trial version. Please report anything that does not work well.

1. Unpack the ZIP anywhere.
2. Once, before the first start, run this command in Terminal. The app is not signed, so macOS blocks it otherwise.

   ```
   xattr -dr com.apple.quarantine ~/Downloads/nikque-gui-0.4.11-mac
   ```

   Replace `~/Downloads/nikque-gui-0.4.11-mac` with the folder you unpacked.

   After this, the app and all the engines in the folder can be opened.
3. Open `nikque GUI.app`.

The folder has the same contents as the Windows version: `config.ini`, `lang` and `engines` are next to `nikque GUI.app`.

- Each engine folder contains the following files. With `exe=auto`, the GUI chooses the one for your Mac.
  - `mEdax-arm64` (Apple silicon)
  - `mEdax-x64-modern` (Intel)
  - `data/eval.dat`
- The nikque engine is the one from the release ZIP of v4.5.5-nikque.34.
- The Heath engine is built for this distribution from the source of the heath.21 tag (64 slots; see `engines/newedax/SOURCE.txt`). This is because the heath.21 release has no engine for macOS.
- Paths use `/` as the separator. Paths in `config.ini` (`dir=` and others) are also written in the Mac form (for example `/Users/name/books/book.dat`).
- Keys: use Command (⌘) where Windows uses Ctrl. For example, Command+C copies the game.
- To right-click, click with two fingers on a trackpad, or click while holding control.
- A file whose name contains a space cannot be passed to the engine directly. The GUI then passes it through a temporary folder, and the log says so.
- If you move the app into Applications, keep `config.ini`, `lang` and `engines` in the same folder as the app.

## Linux (trial version)

From 0.4.4 there is a Linux version (`nikque-gui-0.4.11-linux.tar.gz`).

- It runs on x86-64 Ubuntu 22.04 or later. It should also run on other distributions of the same time or later.
- It uses GTK 3 for the screen.
- It is still a trial version. Please report anything that does not work well.

1. Unpack it anywhere:

   ```
   tar -xzf nikque-gui-0.4.11-linux.tar.gz
   ```

2. Start `nikque-gui` in the folder. Open it from the file manager, or run `./nikque-gui` in a terminal.

The folder has the same contents as the Windows version: `config.ini`, `lang` and `engines` are next to `nikque-gui`.

- GTK 3 is installed by default on a standard desktop Ubuntu. If it is missing, install it with `sudo apt install libgtk-3-0`.
- Japanese and Chinese text is drawn with the system fonts (Noto Sans CJK and others). If characters appear as boxes, install the fonts with `sudo apt install fonts-noto-cjk`.
- Each engine folder contains the following files. With `exe=auto`, the GUI chooses the one for your CPU.
  - `lEdax-x86-64-v4` (AVX-512)
  - `lEdax-x86-64-v3` (AVX2)
  - `lEdax-x86-64` (other x64 CPUs)
  - `data/eval.dat`
- The engines are built for this distribution from the sources of their tags (see `engines/*/SOURCE.txt`). The tags are v4.5.5-nikque.34, and heath.21 with 64 slots.
- The Linux programs in the release ZIP of nikque.34 are for Ubuntu 24.04 or later. The heath.21 release has no engine for Linux.
- Paths use `/` as the separator. Paths in `config.ini` (`dir=` and others) are also written in the Linux form (for example `/home/name/books/book.dat`).
- A file whose name contains a space cannot be passed to the engine directly. The GUI then passes it through a temporary folder, and the log says so.
- If you move the folder, move `nikque-gui`, `config.ini`, `lang` and `engines` together.

## Android (trial version)

From 0.4.4 there is an Android version (`nikque-gui-0.4.11-android.apk`).

- It is for arm64 devices with Android 10 or later. Most phones and tablets of today are arm64 devices.
- It is still a trial version. It has been checked on an emulator only, not yet on a real device.
- Please report anything that does not work well.

1. Download the APK on the device, open it and install it. The app is not from a store, so the system first asks you to allow installing apps from this source.
2. Open "nikque GUI". The first start, and the first start after an update, take a moment to prepare the files.

The screen and touch controls:

- A row of buttons is at the top:
  - "Menu": the menus of the Windows version, shown as lists.
  - To the start, one move back, one move forward, to the end.
- Held upright, the board is at the top and the panel is below it. The panel has the tabs Moves, Book, Record, List and Tree, and the graph.
- Held sideways, the layout is the same as in the Windows version.
- A tap is a click. Tap a square to play there.
- A long press is a right click. On the board, it goes back one move. On the picture of a bookmark, it opens its menu.
- Move two fingers up or down to scroll.
- The Back button of the device asks whether to quit. Quitting saves the settings and closes the engines.

Not in this version:

- learning and maintaining the book
- the game lists
- the scripts
- everything that asks for a file (opening a book, exports, saving pictures)
- typing in the box of the moves (its Copy and Paste buttons work)
- a way to use your own book (there is none yet; the engine makes a small book of the initial position)

The first four are not in the menus either.

Other notes:

- Both engines are in the app: `aEdax-arm64-v8a` from the release ZIPs of v4.5.5-nikque.34 and heath.21.
- The engines run as separate programs.
- Heath uses about 160 to 275 MB of memory for each engine. On a device with little memory, switch to Engine → nikque type to use less memory.
- The engines may keep thinking while another app is in front. When you are done, quit with Back.
- The APK is signed with a trial key. If a later version cannot be installed over this one (the system says the keys differ), uninstall this one first, then install the new one.
- Uninstalling resets the settings.

## Choosing the engine

Switch with Engine → nikque type / Heath type (formerly newedax).

- The first start uses the Heath type. Up to 0.4.0, it was the nikque type.
- The type you choose is saved in `config.ini` and used at the next start.
- Both engines have the same book analysis commands and give the same results.
- From newedax.13 on, Heath uses a newly trained evaluation function (64 slots). Its `eval.dat` is about 108 MB. It is a different file from the `eval.dat` of nikque.
- Use the `eval.dat` from the release of the same version as the Heath engine.
  - The 64-slot heath.17, heath.18 and heath.19 builds use the same file. With an `eval.dat` from eval2 or nikque, the engine runs, but its evaluation is wrong.
  - The GUI then shows a message ([12. Troubleshooting](12-troubleshooting.md)).
- The 48-slot build is not included. It is `edax-4.5.5-heath.21-48.zip` in the Heath release. To use it:
  1. Unpack that ZIP into another folder.
  2. In `config.ini`, write its `bin` folder in `dir=` of `[newedax]`. `[newedax]` holds the Heath settings.

  Use the 48-slot `eval.dat` of the same ZIP with the 48-slot exe files. If they are mixed up, the evaluation is wrong, and no message is shown.
- Memory:
  - One heath.20 engine uses about 160 MB (`wEdax-x86-64.exe`) to about 275 MB (`wEdax-x86-64-v4.exe`, AVX-512).
  - The GUI starts up to five engines: main, search, tool, game and compare. With Heath for all of them, this is about 0.8 to 1.4 GB.
  - A larger hash table or more threads use more memory. The book adds its own size on top of this.
- The book format has not changed. Books you used with earlier versions can be read as they are.
- However, if a book was learned with an earlier version, its values come from the earlier evaluation function.

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
