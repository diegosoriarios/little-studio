# Next steps

Remaining work after the production-readiness phases. Ordered roughly by
dependency, not urgency.

## 1. macOS verification (needs a Mac)

Validated on macOS 26.5 (arm64) with Xcode 26.2, iOS 26.3 simulators, and
Homebrew Qt 6.12/QScintilla 2.14. Use `run-plan.md` (Stage 11) for the full
record:

- [x] Build with Homebrew Qt/QScintilla and confirm CMake discovery.
- [x] Open an iOS project; verify scheme listing, simulator boot/shutdown,
      Build/Install/Run, and the log stream.
- [x] Confirm the `.app` bundle launches outside the build tree.
- [x] Record any fixes in the plan and `CHANGELOG.md`.

## 2. macOS signing and notarization (needs a Mac + Apple account)

Deferred deliberately; the build script and full procedure already exist.

- [ ] Obtain a Developer ID Application certificate.
- [ ] Store notarytool credentials (`xcrun notarytool store-credentials`).
- [ ] Run `build-dmg.sh` with `CODESIGN_IDENTITY` and `NOTARIZE=1`.
- [ ] Verify with `spctl`, `codesign --verify --deep --strict`, and
      `stapler validate`.
- [ ] Wire the same steps into `.github/workflows/release.yml` with secrets.

See [packaging/macos/SIGNING.md](packaging/macos/SIGNING.md) for commands.

## 3. Quality tooling

No formatter, linter, or coverage configuration exists yet. Add it once the
current working tree is committed (the format commit will touch most files).

### 3.1 Formatting

- [ ] Add `.clang-format` (suggested base: LLVM, 4-space indent, 100-column
      limit) and a `format` CMake target.
- [ ] Apply it in one dedicated commit, then add a CI check:
      `clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h')`.
- [ ] Decide whether `.editorconfig`-adjacent files (CMake, YAML, Markdown) are
      out of scope (recommended: out of scope).

### 3.2 Static analysis

- [ ] Add `.clang-tidy` with a curated check set (`bugprone-*`,
      `performance-*`, selected `modernize-*`/`readability-*`), and triage the
      existing diagnostics before enabling it in CI.
- [ ] Add a CI job or `tidy` target, scoped to changed files if the full run is
      slow.
- [ ] Optional: add `cppcheck` (`--enable=warning,performance`) as a
      lower-noise second opinion.

### 3.3 Warnings and process

- [ ] Keep `-Werror` (`LITE_STUDIO_WERROR`) but grow the flag list explicitly
      instead of `-Weverything`; add `-Wshadow`, `-Wconversion` (or narrower
      variants) one at a time with fixes.
- [ ] Optional: coverage job (`gcovr` or `llvm-cov`) over the 13 CTest suites,
      reported as an artifact, not a gate initially.
- [ ] Optional: `pre-commit` hook running clang-format (and clang-tidy on
      changed files) to catch issues before CI.

## 4. Release housekeeping

- [ ] Commit the current work, then tag `v0.1.0` (`git tag -a v0.1.0 -m ...`) so
      the version workflow and changelog line up.
- [ ] Add screenshots to `packaging/io.github.diegosoriarios.LiteStudio.metainfo.xml`
      before submitting to Flathub (AppStream validation currently only emits a
      pedantic warning about the uppercase app ID).
- [ ] Run the Flatpak manifest end to end (`org.flatpak.Builder`) and confirm the
      qscintilla/qtermwidget modules build against the KDE 6.10 SDK.
