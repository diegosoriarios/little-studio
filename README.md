# Lite Studio

A lightweight, project-aware code viewer/editor for desktop. It is **not an IDE**:
no language servers, no build system, no Git integration, no background threads.
Built for browsing and lightly editing code on low-powered machines.

C++17 · Qt 6 Widgets · QScintilla · CMake · Linux first.

## Features

- Project file tree with **lazy loading** (directories are read only when expanded;
  no file watching, no indexing)
- `build`, `.gradle`, `.git`, `DerivedData`, `node_modules`, `.idea` hidden by
  default, toggle via **View → Show Ignored Folders** (expansion state preserved)
- Tabbed editing with close buttons, `*` modified marker, Save/Discard/Cancel
  prompts on tab close and app exit
- Line numbers, brace matching, word-wrap toggle (**View → Word Wrap**)
- **Dark and light themes** (View → Theme), full application palette plus editor
  and syntax colors for keywords, strings, comments, numbers, functions and brackets
- Syntax highlighting: Kotlin, Swift (custom lexers), Java, Gradle, XML, JSON,
  Markdown (QScintilla built-ins), plain text
- Binary detection (NUL heuristic) with refusal dialog
- Files over 2 MiB open as plain text without a lexer
- Encoding preserved on save: UTF-8 (with/without BOM), UTF-16 LE/BE,
  Latin-1 fallback for invalid UTF-8
- Line endings preserved: LF, CRLF, CR (mixed endings normalize to the dominant
  style only when a modified file is saved; unmodified files are never rewritten)
- Status bar: language, encoding, line:column, indentation style (tabs/spaces),
  line-ending type, Saved/Modified/Read-only indicator
- **Recently opened** folders and files (File → Open Recent)
- **Quick Open** (Ctrl+P, VS Code style): fuzzy file jump — basename-prefix
  matches first, then basename/path substrings, then subsequences; Up/Down,
  Enter/double-click to open, Esc to close; indexes in the background chunked
  (no threads)
- **Find/replace in file** (Ctrl+F): incremental highlight-all (never moves
  your caret or viewport), match-case, whole-word, next/prev (F3/Shift+F3),
  replace and replace-all
- **Find in project** (Ctrl+Shift+F): scans the open folder chunked and
  single-threaded with a live progress phase (indexing → scanning), skips
  ignored dirs/binary/large files, capped at 500 results, click a result to
  jump to the line
- **External-change guard**: saving a file modified on disk offers
  Reload/Overwrite/Cancel; File → Reload (F5) refreshes manually
- **Reveal in tree**: optional sync (View → Sync Tree to Editor) selects the
  current file in the project tree
- Go to line (Ctrl+G), Save All (Ctrl+Shift+S), editor font zoom
  (Ctrl+= / Ctrl+- / Ctrl+0)
- Tab context menu: Close, Close Others, Close All, Copy Path, Copy File Name
- Settings persisted via QSettings: window geometry, splitter layout, theme,
  word wrap, ignored-folder toggle, tree sync, zoom, find options, recents
- **Command console** (Ctrl+`, bottom dock): run shell commands in the project
  root with streamed output, filter, session history and Stop
- **Embedded terminal** (Ctrl+Shift+`, optional): a real shell in the project
  root via qtermwidget; compiled in only when the library is present
- **Editor productivity**: toggle comment (Ctrl+/), duplicate line (Ctrl+D),
  move lines (Alt+Up/Down), go to matching brace, bookmarks (Ctrl+F2/F2),
  reopen closed tab (Ctrl+Shift+T), middle-click close, whitespace/indent
  guides, EOL conversion, trim trailing whitespace + final newline
- **Go to Symbol in File** (Ctrl+Shift+O): filtered outline of every
  declaration in the current file
- **Session restore** (open files + cursors) and **auto-reload of unmodified
  files** on window activation (both toggleable in View)
- **.editorconfig** indentation support (indent_style/indent_size/tab_width)
- **Replace in project**: confirmation dialog, capped, encoding-preserving
- **Recovery autosave** (opt-in in View): snapshots while dirty, startup
  prompt to reopen recovered text
- **Editor font picker** (View → Editor Font...), persisted
- Linux install ships a `.desktop` entry + icon
- **Git diff view** (bottom dock + Git menu): working-tree and staged diffs,
  project-wide or per-file, syntax-colored with the QScintilla diff lexer;
  appears only for git repositories
- **Simple code intelligence**: identifier completion while typing
  (Ctrl+Space for explicit), dot-member options from names used in the file,
  and **Go to Definition** (Ctrl+Click / F12, Alt+Left to go back) with
  per-language heuristics (Kotlin/Swift/Java/TS/JS/Dart/C/C++) and a bounded
  project-wide fallback — no language server involved
- **Task presets**: optional `.lite-studio/tasks.json` entries appear in a
  **Tasks** menu and run in the Console, e.g.
  `{"tasks":[{"name":"Build debug","command":"./gradlew","args":["assembleDebug"]}]}`
- **Project detection** on folder open: Android / iOS / Flutter / React Native /
  Expo / monorepo, summarized in the status bar; the **Android**/**iOS** menus
  only appear for matching projects (iOS also requires macOS + Xcode), and the
  **Tasks** menu only when task presets exist
- **Android panel** (bottom dock + Android menu): AVD list and start/stop,
  device list with refresh, Build Debug / Test Debug / Install Debug / Run App,
  Start/Stop Logcat (filtered, capped, batched), and Stop Gradle Daemons;
  requires `ANDROID_HOME` and a project `gradlew` (debug builds only, no
  signing)
- **Build resource limits** (default: 2 CPUs, 4 GB, low priority): Gradle
  heap/worker flags plus a Linux systemd user scope that caps the whole build
  tree (Gradle + Kotlin daemons, test workers). Also applied to Console
  commands (toggle) and task presets; adjustable in the Android panel
- **iOS panel** (macOS + Xcode only, bottom dock + iOS menu): simulator
  boot/shutdown, scheme list, Build Debug (`xcodebuild`), Install
  (`simctl install`), Run (`simctl launch`), and a filtered/batched log stream
- Command line: `lite-studio [path]` — opens a folder, or a file plus its parent

## Architecture

```
lite-studio/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp            App setup, CLI argument
│   ├── MainWindow.h/.cpp   Menus, tabs, status bar, find wiring, settings
│   ├── FileTreeView.h/.cpp Lazy-loading file tree (no watcher, no threads)
│   ├── EditorTab.h/.cpp    QScintilla subclass: one open file + its state
│   ├── FindBar.h/.cpp      Find/replace bar UI (signals only)
│   ├── ProjectSearchPanel.h/.cpp  Chunked single-thread project search
│   ├── FileCollector.h/.cpp       Chunked project file walker (shared)
│   ├── FuzzyMatch.h/.cpp          Quick-open ranking (pure function)
│   ├── QuickOpen.h/.cpp           Ctrl+P quick file open dialog
│   ├── ProcessUtil.h/.cpp         ANSI stripping, newline normalization
│   ├── ProcessRunner.h/.cpp       QProcess wrapper (stream, cancel, capped)
│   ├── ProcessOutputView.h/.cpp   Shared capped output view (filter, Stop)
│   ├── LineBuffer.h/.cpp          Bounded, filterable line buffer
│   ├── ProjectDetector.h/.cpp     Cheap project detection (no SDK needed)
│   ├── ConsolePanel.h/.cpp        Command console dock tab
│   ├── CodeTools.h/.cpp           Completion + go-to-definition heuristics
│   ├── GitTooling.h/.cpp          Git discovery + diff output parsing
│   ├── GitPanel.h/.cpp            Git diff dock (working tree / staged)
│   ├── TaskPresets.h/.cpp         .lite-studio/tasks.json loader
│   ├── TerminalPanel.h/.cpp       Optional qtermwidget terminal (guarded)
│   ├── AndroidTooling.h/.cpp      SDK discovery + AVD/adb output parsing
│   ├── AndroidPanel.h/.cpp        Android run panel (build/install/run/AVDs)
│   ├── AppleTooling.h/.cpp        Xcode tool discovery + JSON parsers
│   ├── ApplePanel.h/.cpp          iOS panel (macOS only; simulators/run/logs)
│   ├── LexerFactory.h/.cpp Extension → language name + lexer mapping
│   ├── SimpleLexer.h/.cpp  QsciLexerCustom for Kotlin/Swift (byte-safe styling)
│   ├── Theme.h/.cpp        Dark/light palettes, editor chrome, lexer colors
│   └── FileIO.h/.cpp       Binary/encoding/EOL detection, atomic save
├── tests/
│   ├── FileIOTest.cpp      Encoding/BOM/EOL/binary round trips
│   ├── EditorTabTest.cpp   Headless load/save flow, themes, lexer styling
│   ├── ProcessTest.cpp     Streaming, cancellation, ANSI strip, exit codes
│   ├── ProjectTest.cpp     Project detection fixture trees
│   ├── AndroidToolingTest.cpp  SDK/AVD/device/applicationId parsers
│   ├── AppleToolingTest.cpp    simctl/xcodebuild JSON parsers
│   ├── TaskPresetsTest.cpp     tasks.json parsing
│   ├── CodeToolsTest.cpp       completion/declaration heuristics
│   └── GitToolingTest.cpp      git parsing + repo detection fixtures
└── .deps/                  Optional local QScintilla (see below)
```

Key decisions:

- **No `QFileSystemModel`**: it uses a background thread and a file watcher.
  The tree is a `QTreeWidget` populated one directory at a time on expand.
- **Scintilla positions are byte offsets**, so the custom lexer styles in
  UTF-8 byte lengths, not QString character counts.
- QScintilla does not own lexers; lexers are parented to their editor.
- Idle CPU is ~0%: no timers, no watchers, no repaints without input.

## Requirements

- Qt ≥ 6.4 (Widgets; `QStringDecoder`/`QStringEncoder` from QtCore)
- QScintilla 2.14 (Qt6 build), CMake ≥ 3.21, a C++17 compiler, Ninja (optional)

### Install dependencies

**Fedora:**
```bash
sudo dnf install -y cmake ninja-build gcc-c++ qt6-qtbase-devel qscintilla-qt6-devel
# optional embedded terminal:
sudo dnf install -y qtermwidget-devel
```

**Debian/Ubuntu:**
```bash
sudo apt install -y cmake ninja-build g++ qt6-base-dev libqscintilla2-qt6-dev
```

**Arch:**
```bash
sudo pacman -S --needed cmake ninja gcc qt6-base qscintilla-qt6
```

**No root?** The build also works with local libraries: download the RPMs
(`dnf download qscintilla-qt6-devel qtermwidget-devel`) and extract them into
`.deps/` (`rpm2cpio pkg.rpm | cpio -idm -D .deps`). CMake checks `.deps/`
first, so it takes precedence over a later system install. The terminal is
optional: without qtermwidget the Terminal tab is simply not compiled in.

## Build & run

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/lite-studio                     # empty window
./build/lite-studio ~/projects/demo     # open a folder
./build/lite-studio src/Main.kt         # open a file + its parent folder
```

Release:
```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/lite-studio
```

Install: `cmake --install build-release --prefix ~/.local` (binary → `bin/`).

## Tests

```bash
cmake --build build --target fileio-test editortab-test process-test project-test android-test apple-test task-test codetools-test git-test
./build/fileio-test       # encoding/BOM/EOL/binary round trips
./build/editortab-test    # headless (offscreen) load/save/theme/lexer checks
./build/process-test      # process streaming, cancellation, ANSI stripping
./build/project-test      # project detection fixture trees
./build/android-test      # Android SDK/AVD/device/applicationId parsing
./build/apple-test        # simctl/xcodebuild JSON parsing (runs anywhere)
./build/task-test         # .lite-studio/tasks.json parsing
./build/codetools-test    # completion/declaration heuristics
./build/git-test          # git parsing + repo detection fixtures
```

## Intentionally not included

- Language servers, autocomplete, call tips, diagnostics, refactoring
- Git integration or scanning
- Gradle/build/run integration
- Project indexing, global search, find/replace
- File watching, auto-reload, external-change conflict detection
- New/untitled files, Save As, rename/delete in the tree
- Split editors, folding UI, outline/function list, minimap
- Terminal, debugging
- Plugins/extensions, custom themes beyond dark/light
- Settings/session persistence, recent folders
- Encoding/EOL pickers or conversion UI
- Binary/hex viewer
- Drag and drop, file-tree context-menu operations
- Auto-formatting, trailing-whitespace trimming, auto-indent (all disabled)

## Roadmap ideas (later stages)

- **Android/iOS tooling** (build/install/run, emulator & simulator management,
  logcat/log stream) — see [run-plan.md](run-plan.md)
- `.editorconfig`-based indent detection
- Foldable code blocks; symlink-loop protection in the file tree
- Windows/macOS packaging; system QScintilla discovery via pkg-config fallback
- Bracket-pair colorization and indent guides in SimpleLexer

## License note

QScintilla is GPLv3/commercial — review licensing before distributing binaries.
