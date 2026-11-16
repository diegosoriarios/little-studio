# Changelog

All notable changes to Lite Studio are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Versions are
derived from git tags (`vX.Y.Z`).

## [Unreleased]

### Added

- AppImage build script (`packaging/appimage/build-appimage.sh`).
- Flatpak manifest (`packaging/flatpak/io.github.diegosoriarios.LiteStudio.yml`).
- macOS `.app`/`.dmg` build script with optional Developer ID signing and
  notarization (`packaging/macos/`).
- Release workflow that builds Linux and macOS artifacts on version tags.
- AppStream metadata for software centers.
- `--version`/`-v` and `--help`/`-h` command line flags; the version is derived
  from the nearest git tag.
- GPLv3 license and third-party notices.
- Headless test suites for file collection and the file tree; CTest target
  `check`.
- Auto-indent on Return (View → Auto Indent, on by default): copies the
  previous line's indentation and follows the project's tab/space style.
- Far-right icon rail for Terminal, Find in Project, Git, Android, Android
  details (Logcat), iOS Simulator and Console. Icons follow the active theme.
- GitHub Actions CI: Linux (GCC/Clang, Debug/Release), sanitizers, macOS.
- macOS build and iOS panel validated with Homebrew Qt 6.12/QScintilla 2.14,
  Xcode 26.2 and iOS 26.3 simulators; the generated `.app` launches outside
  the build tree and the unsigned `.dmg` passes `codesign --verify`. README
  now documents the macOS prerequisites.

### Changed

- The embedded Terminal is now a fixed bottom dock and can stay open alongside
  one right-side panel. Other panels still switch one at a time.
- Tests are built by default and run through CTest.
- macOS bundle identifier is now `io.github.diegosoriarios.LiteStudio`.
- New application icon used for the window/taskbar, Linux hicolor themes,
  AppImage/Flatpak packages and the macOS bundle.

### Fixed

- The embedded terminal now follows the active dark/light theme instead of
  qtermwidget's default light-gray background.
- Recursive symlinks no longer cause infinite traversal in Quick Open, project
  search, or go-to-definition; the tree shows them as disabled nodes.
- Duplicate `kMaxDiffBytes` definition removed from `GitPanel.cpp`.
- Dot-member completion no longer renders every member in a single horizontal
  row after the document word-completion list has been shown.
- Project search no longer emits a Qt `QObject::connect` warning on startup;
  the results list is now connected after it is constructed.
- macOS builds no longer assume the embedded terminal is available: Homebrew
  has no `qtermwidget` formula, so the Terminal tab is omitted (Console still
  works).

## [0.1.0] - 2026-11-14

First tagged release.

### Added

- Folder tree with lazy loading, ignored-folder filtering, and symlink-cycle
  protection.
- Tabbed QScintilla editor with dark/light themes, syntax highlighting for
  Kotlin, Swift, Java, Gradle, XML, JSON, Markdown and plain text.
- Preserved encodings (UTF-8, UTF-16 LE/BE, Latin-1 fallback) and line endings
  (LF, CRLF, CR); binary detection; large-file plain-text mode.
- Quick Open, find/replace in file, find/replace in project, go to line, go to
  symbol in file.
- Editor productivity: toggle comment, duplicate/move lines, bookmarks,
  reopen closed tab, matching brace, EOL conversion, trim trailing whitespace.
- Identifier completion, dot-member completion, and Go to Definition without a
  language server.
- Command console, optional embedded terminal (qtermwidget), Git diff view,
  and task presets (`.lite-studio/tasks.json`).
- Project detection (Android, iOS, Flutter, React Native, Expo, monorepos) with
  Android and iOS build/run panels and resource-limited builds.
- Session restore, recovery autosave, external-change guard, .editorconfig
  indentation, recently opened folders and files.
- Settings persisted via QSettings; Linux `.desktop` entry and icon.
