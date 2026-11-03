# Run Support (Android + iOS) — Plan

Status: **Stages 8–14 + Git diff view (Stage 15) implemented** (Stage 11 runtime verification pending on macOS). Stage 14 = editor quick wins (comment toggle, line ops, bookmarks, whitespace/guides, EOL conversion, trim/final newline), symbol outline, session restore, auto-reload, .editorconfig indentation, font picker, Linux packaging, replace-in-project, and opt-in autosave/recovery. Heuristic code intelligence (no LSP) shipped in Stage 13. Remaining future work: remote Mac/SSH, iOS physical devices, release archives, AVD creation, LSP.

Goal: add a lightweight, opt-in run layer to Lite Studio — build, install, run,
manage devices/emulators/simulators and stream logs — while keeping the app
fast, quiet and free of language servers, Gradle/Xcode project models,
debuggers or background work.

All actions are plain terminal commands mapped to `QProcess`.

---

## 1. Platform matrix

| Platform | Android tooling | iOS (Xcode) tooling |
|---|---|---|
| Linux (this machine) | ✅ `adb`, `emulator` | ❌ iOS tab hidden/disabled |
| macOS | ✅ (if SDK installed) | ✅ `xcrun simctl`, `xcodebuild` |

Xcode tooling exists **only on macOS**. On Linux the iOS panel stays
unavailable (`isAvailable() == false`) with a tooltip; the code remains portable.

## 2. Constraints

- **No threads**: `QProcess` is event-loop based. No `QThread`, no host-side
  polling loops while idle. Idle CPU stays ~0%.
- **Explicit actions only**: every command starts from a button/menu click.
  Nothing is auto-run on project open, nothing is auto-built on save.
- **Bounded resource use**: output buffers are capped; one heavy job at a time;
  long-lived children (emulator, simulator, logs) have their own Stop controls.
- **Debug workflow only**: Android `assembleDebug`/`installDebug`; iOS simulator
  Debug builds (no signing/provisioning involved).

This moves the app beyond the original "viewer, not an IDE" boundary; README
constraints will be amended to:

> Lite Studio is a lightweight code editor with an optional Android/iOS runner.
> It still has no Gradle/Xcode project model, no debugger, no signing UI, no
> language servers, no extensions and no background indexing.

### Footprint baseline (measured, Stage 15)

| Metric | Value |
|---|---|
| Release binary | ~885 KB (Debug ~10 MB with symbols) |
| Idle RSS (folder open) | ~44 MB (Qt Widgets floor) |
| Idle CPU | 0 jiffies |
| Source size | ~12.1k lines |

Guardrails: explicit actions only, lazy panel/list population, output capped at
5000 lines, no watchers/polling, optional dependencies stay optional. Heavy
toolchains (Gradle daemon, emulators) are child processes, never app memory.

## 3. Environment findings

### This Linux machine

| Item | Value |
|---|---|
| `ANDROID_HOME` | `/home/diego/Android/Sdk` |
| SDK contents | `emulator`, `platform-tools`, `build-tools`, `ndk`, `platforms`, `system-images` |
| `adb` | `/usr/bin/adb` (also `$ANDROID_HOME/platform-tools/adb`) |
| `emulator` | `$ANDROID_HOME/emulator/emulator` |
| AVDs | `Medium_Phone_API_35`, `Pixel_3a_XL`, `Wear_OS_Small_Round_API_30` |
| `cmdline-tools` | **absent** → no `sdkmanager`/`avdmanager` (AVD creation deferred) |
| `gradle` binary | absent (projects use their own `./gradlew`) |
| `java` | `/usr/bin/java` |
| `xcodebuild` / `xcrun` / `simctl` | **absent** (macOS-only) |

### macOS (to verify during Stage 11)

- Xcode + Command Line Tools installed; `xcodebuild -version` works
- Qt 6 and QScintilla available (Homebrew: `qt`, `qscintilla2`)
- At least one iOS simulator runtime installed

## 4. Confirmed decisions

### Android
- Separate **Build / Install / Run** buttons.
- **Logcat included**: streaming + client-side filter + ~5000-line buffer.
- Android-specific actions now; generic JSON command presets later.
- Debug-only, root-level tasks; no signing, no module detection, no AVD
  creation until `cmdline-tools` exists.

### iOS
- Lite Studio will also be **built on macOS**, so the iOS tab is useful.
- **Simulator-only, Debug-only**, no physical devices, no signing, no
  `pod install`, no archives/TestFlight.
- Workspace preferred over project; scheme selected from `xcodebuild -list`.
- iOS planning lives in this combined document.

### Terminal
- **Console tab now** (Stage 8): a command console, not a terminal emulator.
  Runs `/bin/sh -c <command>` (or `cmd.exe /c` on Windows) in the project root.
- A **real embedded terminal** (qtermwidget) is available as an *optional*
  build-time dependency (Stage 12): the Terminal tab is compiled in only when
  the library is found, starts its shell lazily, and can be restarted.
- An external-terminal launcher button remains a cheap future freebie.

## 5. Architecture

```
src/ProcessUtil.h/.cpp       ANSI/OSC stripping, newline normalization.

src/ProcessRunner.h/.cpp     QProcess wrapper: run a command, stream merged
                             stdout/stderr line by line, strip ANSI, UTF-8
                             decode, bounded output, cancel, finished(code).
                             Linux: child in its own process group so Gradle's
                             children are killed too.

src/ProcessOutputView.h/.cpp Shared output widget: read-only capped view,
                             client-side filter field, Stop button, status line.

src/ProjectDetector.h/.cpp   Cheap bounded detection (root + one level):
                             Android / iOS / Flutter / React Native / Expo /
                             monorepo; resolves androidRoot and appleRoot.
                             Auto-runs on folder open (no watchers).

src/ConsolePanel.h/.cpp      Command console tab: shell commands in the
                             project root, session history, Stop. No TTY.

src/CodeTools.h/.cpp         Heuristic code intelligence: identifier/member
                             extraction, per-language declaration search.
src/TaskPresets.h/.cpp       Optional .lite-studio/tasks.json loader
                             (name/command/args/cwd/shell) for the Tasks menu.

src/TerminalPanel.h/.cpp     Optional embedded terminal (qtermwidget),
                             compiled only when the library is found.

src/AndroidPanel.h/.cpp      Android panel (Stage 9 — done):
                             AVD combo + Start/Stop, device combo + Refresh,
                             Build / Install / Run, logcat toggle (Stage 10).

src/AndroidTooling.h/.cpp    Pure, dependency-injected helpers (testable):
                             discoverSdk(envHome, fallbacks),
                             parseAvdList, parseAdbDevices, parsePids,
                             extractApplicationId (Groovy/KTS/namespace),
                             findGradleWrapper.

src/AppleTooling.h/.cpp      Pure helpers (testable on Linux):
                             parseSimctlDevices(json),
                             parseXcodebuildList(json) -> schemes,
                             parseBuildSettings(json) -> app path + bundle id,
                             findWorkspaceOrProject(projectRoot).

src/ApplePanel.h/.cpp        iOS panel:
                             simulator combo + Boot/Shutdown, scheme combo,
                             Build / Install / Run, logs toggle + filter.
                             isAvailable() false unless on macOS with Xcode.
```

Modified files:

| File | Change |
|---|---|
| `src/MainWindow.*` | `Android` and `iOS` menus; panels tabified in the bottom dock; `setProjectRoot()` on folder open |
| `CMakeLists.txt` | new sources; Homebrew hints; `MACOSX_BUNDLE` on Apple; extend headless test target |
| `README.md` | feature list, amended constraints, roadmap link |

### Bottom dock layout

Panels are `QDockWidget`s `tabifyDockWidget()`-ed with *Find in Project*:

```
[ Find in Project ] [ Console ] [ Android ] [ iOS ]
```

Console is available now; Android/iOS tabs appear in Stages 9/11 when their
panels exist and the project is detected. No layout refactor.

## 6. Project detection

Runs once when a folder opens (and again when the run panel refreshes) and is
pure/testable. Bounded: root + one level, small capped reads
(`build.gradle*`/`settings.gradle*` ≤16 KiB, `package.json` ≤64 KiB).

| Kind | Signals |
|---|---|
| Flutter | `pubspec.yaml` (+ `.metadata` `project_type: app`, `android/`, `ios/`) |
| React Native | `package.json` deps (`dependencies`/`devDependencies`/`peerDependencies`) with `react-native` + native dirs |
| Expo (managed) | `package.json` with `expo`, no native dirs |
| Android | `settings.gradle(.kts)`, `gradlew`, `app/src/main/AndroidManifest.xml`, `build.gradle(.kts)` with `com.android.*` |
| iOS | `*.xcodeproj`, `*.xcworkspace`, `Podfile`, `project.yml` |
| Monorepo | both native Android and iOS markers at the root |

`ProjectInfo` exposes `kind`, `hasAndroid`, `hasApple`, `androidRoot`,
`appleRoot`, `evidence` (human-readable reasons) and a `summary()` for the
status bar, e.g. `Flutter (android, ios)`. Flutter/React Native roots point at
`<root>/android` and `<root>/ios`, so the native tooling works unchanged.

Priority for the label: Flutter → React Native → Android → iOS → Unknown.
`hasAndroid`/`hasApple` remain independent, so monorepos enable both tabs.
On unknown folders the run tabs stay hidden and the app behaves as before.

Limitations: detection is root-level (open the project root, not a subfolder);
dynamically computed Gradle IDs still fall back to `namespace` (Stage 9).

## 7. Console (Stage 8)

A lightweight command console in the bottom dock (View → Console, Ctrl+`):

- Runs the typed command through `/bin/sh -c` (Unix) or `cmd.exe /c` (Windows)
  with the project root as working directory — pipes, `&&` and PATH lookups
  work.
- Streams merged output with ANSI stripping, 5000-line cap, client-side filter
  field and a Stop button (SIGTERM to the process group, SIGKILL after 2 s).
- Session-only history with ↑/↓; `$ command` echo and `[done, exit N, X ms]`
  footer.
- No TTY: interactive full-screen programs (vim/htop) and stdin prompts are not
  supported. The real-terminal option is deferred (see Terminal decisions).

## 8. Android commands

| Action | Command |
|---|---|
| List AVDs | `<sdk>/emulator/emulator -list-avds` |
| Start emulator | `<sdk>/emulator/emulator -avd <name>` (long-lived child) |
| Wait for boot | `adb -s <serial> wait-for-device shell 'while [ -z "$(getprop sys.boot_completed)" ]; do sleep 1; done'` (loop runs on-device) |
| List devices | `adb devices -l` |
| Build | `./gradlew assembleDebug --console=plain` |
| Test | `./gradlew testDebugUnitTest --console=plain` |
| Install | `./gradlew installDebug` |
| Run | `adb -s <serial> shell monkey -p <applicationId> -c android.intent.category.LAUNCHER 1` |
| Logcat | `adb -s <serial> logcat` (streamed) |
| Stop daemons | `./gradlew --stop` |

- Working directory for Gradle commands = project root.
- `--console=plain` keeps Gradle output line-based (no ANSI progress redraw).
- One connected device → auto-selected; otherwise the user picks one.
- `applicationId` extracted by regex from `build.gradle`/`build.gradle.kts`
  (fallback `namespace`); Run disabled with a tooltip if neither is found.

### Android panel behavior (Stage 9 — done)

- Bottom dock tab **Android**, plus an **Android** menu with: Show Android
  Panel, Refresh Devices, Build Debug, Install Debug, Run App, Stop Gradle
  Daemons. Menu actions stay disabled unless the project is detected as
  Android.
- `ANDROID_HOME`/`ANDROID_SDK_ROOT` discovery with common fallbacks; the
  status/output view reports a missing SDK or missing `gradlew`.
- Device lists are refreshed on folder open and on demand (no polling);
  AVD and device lists are probed through the same `ProcessRunner`.
- `installDebug` receives `ANDROID_SERIAL=<selected device>`; Run uses
  `adb -s <serial> shell monkey ...`.
- Emulator start is a long-lived child; Stop sends SIGTERM to its process
  group. The app also runs `adb wait-for-device` after starting an emulator,
  then refreshes the device list.
- Closing the app stops emulator/logcat children it started (they are in the
  app's process groups).

### Resource limits (defaults on: 2 CPUs, 4 GB, low priority)

Build/Test/Install jobs and Console commands are wrapped so a heavy build does
not freeze the desktop:
- Gradle flags: `-Dorg.gradle.jvmargs=-Xmx<RAM>g` (Gradle automatically starts
  a fresh daemon when this differs) and `-Dorg.gradle.workers.max=<CPUs>`.
- Linux with `systemd-run`: the whole job runs in a user scope
  (`systemd-run --user --scope --quiet -p CPUQuota=N00% -p MemoryMax=G`), which
  also caps the Kotlin daemon and test worker processes. Low priority uses
  `nice -n 19` *inside* the scope (scope units have no `Nice` property).
- Without systemd (or on macOS): `nice -n 19` fallback; CPU/memory caps degrade
  to the Gradle flags only.
- Settings persist: Android panel CPU/RAM/low-priority, Console “Limits”
  toggle (on by default). Task presets inherit the Console limits.
- A too-small memory cap OOM-kills the build (visible in the output). Tune the
  Kotlin daemon with `kotlin.daemon.jvmargs` in `gradle.properties` if needed.
- This does not throttle Android Studio itself; to tame it globally, set
  `org.gradle.jvmargs`/`org.gradle.workers.max` in `~/.gradle/gradle.properties`.

### Git diff view (Stage 15 — done)

- Pure `git` subprocess (`git diff [--cached] [-- path]`) rendered with
  `QsciLexerDiff`; no libgit2, no background threads
- Modes: Working tree and Staged (HEAD); project-wide or single-file
- Repo gating: one `.git` existence check on folder open; the Git menu/dock
  stay hidden for non-repo folders
- Future: side-by-side split, history/log view, tree context-menu diff,
  word-level diff

## 9. iOS commands (macOS only)

| Action | Command |
|---|---|
| List simulators | `xcrun simctl list devices available --json` |
| List schemes | `xcodebuild -list -json -workspace <W>` (or `-project <P>`) |
| Boot simulator | `xcrun simctl boot <UDID>` + `open -a Simulator` |
| Wait for boot | `xcrun simctl bootstatus <UDID> -b` (blocks in the child, no app polling) |
| Build | `xcodebuild -workspace <W> -scheme <S> -configuration Debug -sdk iphonesimulator -destination 'platform=iOS Simulator,id=<UDID>' -derivedDataPath <cache>/DerivedData-<hash> build` |
| Read settings | `xcodebuild ... -showBuildSettings -json` → `TARGET_BUILD_DIR`, `FULL_PRODUCT_NAME`, `PRODUCT_BUNDLE_IDENTIFIER` |
| Install | `xcrun simctl install <UDID> <App.app>` |
| Launch | `xcrun simctl launch <UDID> <bundleId>` |
| Logs | `xcrun simctl spawn <UDID> log stream --style compact --level debug --predicate 'processImagePath CONTAINS "<Scheme>.app"'` |
| Shutdown | `xcrun simctl shutdown <UDID>` |

Details:
- Discover `<root>/*.xcworkspace` first, fall back to `<root>/*.xcodeproj`;
  if multiple, show a picker (v1: first match alphabetically + a combo).
- Device list filters `isAvailable == true` and iOS runtimes; state shown
  (Booted / Shutdown).
- Derived data lives in the per-project cache dir, never in the repo.
- `pod install` is **not** run automatically; use the existing workspace.
- No signing flags needed for the simulator; `CODE_SIGNING_ALLOWED=NO` may be
  added if a project's build settings demand signing even for simulator.

## 10. Shared UI behavior

- **Lazy population**: device/target lists refresh when a panel is first shown
  and on **Refresh**; nothing runs at startup.
- **One heavy job at a time per panel**: Build/Install/Run share a job slot;
  other buttons disable while running. **Stop** cancels (terminate → kill after
  a 2 s grace period).
- **Long-lived children**: emulator/simulator and log streams have their own
  Stop/toggle.
- **Menu visibility**: the Android/iOS menus appear only for detected
  projects (the iOS menu also requires a working Xcode toolchain), and the
  Tasks menu only when `.lite-studio/tasks.json` defines at least one task.
- **Enablement**:
  - Build: wrapper/workspace found and no job running
  - iOS actions additionally require macOS/Xcode (`isAvailable()`)
  - Install/Run: also require a device/simulator in a ready/booted state
  - Run (Android): also requires a parsed `applicationId`
- **Output view**: monospace, read-only, `setMaximumBlockCount(5000)`,
  auto-scroll only when already at the bottom; exit code + duration in the
  status line and a status-bar message on completion.
- **Logs**: the filter is applied client-side over the capped buffer, so
  changing it never restarts `adb logcat` / `log stream`. Logcat lines are
  batched (120 ms) before hitting the view to keep bursts smooth.

## 11. macOS build prerequisites

```bash
brew install qt qscintilla2 cmake ninja
# optional embedded terminal (if the formula is available):
brew install qtermwidget
```

- CMake needs Homebrew hints for Qt/QScintilla discovery, e.g.
  `${HOMEBREW_PREFIX}/include`, `${HOMEBREW_PREFIX}/lib`
  (`/opt/homebrew` on Apple Silicon, `/usr/local` on Intel). Verify the
  QScintilla header layout (`Qsci/...`) and library name (`qscintilla2_qt6`)
  during Stage 11.
- Build a proper app bundle: `MACOSX_BUNDLE` + bundle identifier metadata via
  `qt_add_executable`/`set_target_properties`.
- `xcodebuild` requires the Xcode license to be accepted and CLT selected
  (`xcode-select -p`); surface the error output if either is missing.

## 12. Tests (headless, no SDK/device/simulator needed)

`ProcessUtil` / `ProcessRunner` / `LineBuffer` (`process-test`, `/bin/sh` fakes):
- ANSI/OSC stripping, CR/CRLF normalization
- merged stdout/stderr ordering and exit code
- cancel of `sleep 30` completes in well under a second
- CR progress output becomes separate lines
- failed-to-start reported as `finished(-1)`, working directory respected
- LineBuffer trimming, batch append and case-insensitive filtering

`ProjectDetector` (`project-test`, fixture trees):
- native Android Groovy and Kotlin DSL
- iOS via `.xcodeproj`, `.xcworkspace`, `Podfile`
- Flutter app (`.metadata` + `android/` + `ios/`) and Flutter package
- React Native (`dependencies` and `devDependencies`), Expo managed and
  prebuilt
- monorepo with both platforms, unknown/missing folders, false-positive guard

`AndroidTooling` (fixtures):
- AVD list parsing; `adb devices -l` states (`device`, `offline`,
  `unauthorized`, unplugged, model); `pidof` parsing; `applicationId` in
  Groovy/KTS/`namespace`; `gradlew` detection; SDK discovery with injected env.
- Resource-limit builders: Gradle `-D` flags, systemd scope prefixes, nice
  fallback, and `wrapCommand` strategy selection.

`AppleTooling` (`apple-test`, fixtures, runs on Linux):
- `simctl list devices available --json` parsing (runtimes, availability, state)
- `xcodebuild -list -json` scheme extraction (workspace and project shapes)
- `-showBuildSettings -json` app path + bundle id extraction
- workspace/project discovery in a temp tree (workspace preferred)
- tool discovery from an explicit PATH, and `isAvailable() == false` on
  non-macOS hosts

`EditorConfig` (`editorconfig-test`, fixtures):
- missing file -> unset; global + per-extension override (later wins);
  exact-name sections; tab_width; non-matching extension; comments

`RecoveryStore` (`recovery-test`):
- empty store; write/read round trip (path + text); deterministic snapshot
  path; multiple snapshots; clear

`CodeTools` (`codetools-test`, fixtures):
- language mapping by extension; member collection (unique/sorted/capped)
- identifier boundaries (clicking past an identifier resolves to it)
- declaration patterns per language; calls and comments excluded;
  `.member` uses never match

`CodeTools::replaceAll` (in `codetools-test`): case sensitivity,
  whole-word boundaries, no-match and empty-term cases

`GitTooling` (`git-test`, fixtures):
- git discovery from injected PATH; `.git` dir/file/absent repo detection
- `--name-status` parsing (M/A/D/R100/C75, quoted paths, noise lines)
- unified-diff section offsets: two-file slicing, diff.noprefix headers,
  deleted files (`+++ /dev/null`)

`TaskPresets` (`task-test`, fixtures):
- missing file -> empty/no error; valid presets (args, cwd, shell mode)
- malformed JSON and non-object roots report errors
- invalid entries are skipped

Log helper: buffer trim to N lines, case-insensitive filter.

## 13. Staged implementation

Each stage ends with Debug **and** Release builds green (on its platform),
all headless tests passing, and idle CPU still ~0%.

### Stage 8 — Process foundation, console, project detection — **done**
- `ProcessUtil` + `ProcessRunner` (streaming, cancel, ANSI strip, process group)
- `ProcessOutputView` (capped output, filter, status, Stop)
- `ConsolePanel` in a bottom dock tab (Ctrl+`): shell commands in the project
  root, session history, Stop
- `ProjectDetector`: auto-detect on folder open, status-bar summary, resolves
  `androidRoot`/`appleRoot`
- tests: `process-test` (8 cases), `project-test` (13 cases)

### Stage 9 — Android awareness — **done**
- `AndroidTooling`: SDK discovery, AVD/`adb devices`/`pidof` parsing,
  `applicationId` extraction (Groovy/KTS/`namespace`), `gradlew` discovery
- `AndroidPanel` dock tab + Android menu (refresh, build, install, run,
  stop Gradle daemons)
- AVD list/start/stop, device list with ready-state filtering, `ANDROID_SERIAL`
  for installs, emulator boot wait
- `android-test` (6 groups) + `ProcessRunner` environment test

### Stage 10 — Android logcat and polish — **done**
- Logcat tab in the Android panel: streamed `adb -s <serial> logcat`, toggled
  on/off, client-side filter, batched UI updates (120 ms) and a bounded
  5000-line buffer (`LineBuffer`)
- Status-bar job indicator (`Android: build …` / `… — done`)
- Full button enable/disable state machine (build/install/run/logcat/refresh)
- Logcat stops automatically when the selected device changes or the project
  changes; closing the app terminates logcat/emulator children
- README/constraints update and idle-CPU verification

### Stage 11 — Apple tooling (macOS) — **implemented, pending Mac verification**
- `AppleTooling`: tool discovery, `simctl`/`xcodebuild -list`/`-showBuildSettings`
  parsers, workspace/project discovery
- `ApplePanel` dock tab + iOS menu: simulator list/boot/shutdown, scheme list,
  Build Debug (`xcodebuild` with a per-project derived-data cache dir),
  Install (`simctl install`), Run (`simctl launch`), log stream with filter and
  batched bounded buffer
- iOS actions are disabled on non-macOS hosts; the panel explains that Xcode is
  required. `apple-test` runs on Linux.
- CMake: Homebrew prefix discovery for Qt/QScintilla and `MACOSX_BUNDLE`
  metadata are in place (guarded by `APPLE`)
- Remaining: a first real run on a Mac (see macOS build prerequisites)

### Stage 13 — Simple code intelligence — **done**
- Word completion from the current buffer while typing (`AcsDocument`,
  threshold 3, Ctrl+Space explicit)
- Dot-member completion: typing `var.` pops every member already used with
  that prefix in the file (`CodeTools::collectMembers`, capped)
- **Go to Definition**: Ctrl+Click + **F12** — per-language declaration
  patterns (Kotlin, Swift, Java, TS/JS, Dart, generic C/C++, `#define`),
  comments/calls excluded; in-file first, then bounded project-wide fallback
  (chunked walk, ≤2000 files / 500 scanned, skip binary/>2 MB, 2 s guard);
  multiple matches → popup; **Alt+Left** Go Back via per-tab position stack
- `codetools-test` (10 groups)
- Still no LSP/type-aware completion (documented heuristic limits)

### Stage 14 — Editor productivity + sessions + packaging — **done**
- Toggle comment (Ctrl+/), duplicate line (Ctrl+D), move lines (Alt+Up/Down),
  go to matching brace (Ctrl+Shift+\), bookmarks (Ctrl+F2/F2/Shift+F2)
- Reopen closed tab (Ctrl+Shift+T), middle-click closes a tab
- Whitespace + indentation-guide toggles; EOL conversion submenu; trim
  trailing whitespace + ensure final newline (explicit actions)
- Symbol outline popup (Ctrl+Shift+O) with filter
- Session restore: open files + cursor positions per project (default ON)
- Auto-reload unmodified files on window activation (default ON)
- `.editorconfig`: indent style/size/tab_width (overrides detection)
- Editor font picker (family + size, persisted)
- Linux packaging: .desktop + scalable icon in the install target
- Replace in project with confirmation, capped, encoding-preserving
- Opt-in autosave recovery snapshots (30 s while dirty) + startup prompt

### Stage 15 — Git diff view — **done**
- `GitTooling`: git discovery from PATH, `.git` presence check (dir or file
  for worktrees), `--name-status` parsing (M/A/D/R/C, quoted paths, noise),
  unified-diff section offsets (per-file slicing, a/ b/ prefix stripping,
  diff.noprefix-safe)
- `GitPanel` dock: mode combo (Working tree / Staged HEAD), file combo
  (all changed files / single file), Refresh + Stop, read-only diff view with
  `QsciLexerDiff` coloring (theme-aware dark/light), batched streaming
  (120 ms), 2 MB cap with truncation marker
- **Git** menu: Show Git Panel, Refresh, Diff Working Tree, Diff Staged —
  hidden entirely unless the opened folder is a git repo and git is on PATH
- `git-test` (6 groups) + runtime verification with a real temp repository
  (probes + diff rendered correctly)

### Stage 12 — Extras — **partially done**
Implemented:
- **Generic task presets**: optional `<project>/.lite-studio/tasks.json` with
  `name`, `command`, `args`, `cwd`, `shell`; shown in a **Tasks** menu and run
  in the Console dock. Reload action; errors surfaced in the status bar.
- **Optional embedded terminal** (qtermwidget): **Terminal** dock tab with a
  lazily started shell in the project root, View → Restart Terminal, and clean
  fallback when the library is absent (tab not compiled in).
- Note: Android **physical devices** already work through `adb` (device list,
  install, run, logcat) — no extra work needed.

Still future:
- Remote Mac over SSH (Linux host driving `xcodebuild` on a Mac)
- iOS physical devices (`devicectl` + signing/provisioning setup)
- Release builds / archives (signing stays the user's configuration)
- AVD creation via `avdmanager`, once `cmdline-tools` is installed

## 14. Known trade-offs

- Gradle daemon persists after builds (normal; use *Stop Gradle Daemons* or add
  `--no-daemon` later at the cost of slower builds).
- Emulator/simulator memory usage is heavy by nature; user-initiated only.
- `Install`/`Run` need a booted device/simulator; disabled until ready.
- AVD creation is out of scope until `cmdline-tools` is installed.
- `applicationId` extraction is regex-based; dynamically computed IDs fall back
  to `namespace`, and Run is disabled with a tooltip if neither is found.
- iOS support is macOS-only; on Linux the tab is unavailable.
- `pod install`, signing, archives and physical iOS devices are out of scope v1.
- Xcode behavior varies by version; only stable CLI surfaces are used
  (`xcodebuild`, `simctl`).
- Killing a job kills its process group, but system-level orphans (e.g. an
  emulator started outside the app) are not managed.
- The console has no TTY: interactive full-screen programs and stdin prompts
  are not supported (pipes/`&&`/PATH lookups do work).
- Project detection is root-level and heuristic; open the project root, not a
  subfolder. False negatives are possible for unusual layouts.
- Console output is capped at 5000 lines; older lines are dropped from the view
  and the filter buffer.
- Console history is session-only (not persisted).
