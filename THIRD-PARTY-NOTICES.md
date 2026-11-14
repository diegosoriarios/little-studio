# Third-party notices

Lite Studio is distributed under the GNU General Public License, version 3
(see [LICENSE](LICENSE)). It links against the following third-party
components. Each component remains under its own license; the notices below
are provided for attribution and compliance.

## Qt 6 (Core, Widgets)

- Copyright (C) The Qt Company Ltd. and other contributors.
- License: LGPL-3.0-only OR GPL-3.0-only (Qt 6 dual licensing), or a
  commercial Qt license.
- Lite Studio links Qt dynamically and does not modify it. Users may replace
  the Qt libraries with compatible versions; source code for the exact Qt
  version used to build a binary is available from
  <https://download.qt.io/> and from the distribution's package sources.
- <https://www.qt.io/licensing/>

## QScintilla 2.14 (Qt 6 build)

- Copyright (C) Riverbank Computing Limited.
- License: GPL-3.0-only, or a commercial QScintilla license from Riverbank
  Computing.
- Lite Studio uses QScintilla under the GPLv3 option, which is why the whole
  program is distributed under the GPLv3.
- <https://riverbankcomputing.com/software/qscintilla/license>

## Scintilla

- QScintilla embeds the Scintilla editing component.
- Copyright (C) Neil Hodgson and contributors.
- License: Historical Permission Notice and Disclaimer (HPND), a permissive
  license compatible with the GPL.
- <https://www.scintilla.org/License.txt>

## qtermwidget (optional)

- The embedded terminal is compiled only when qtermwidget is available.
- Copyright (C) The qtermwidget authors (LXQt project).
- License: GPL-2.0-or-later, compatible with the GPLv3.
- <https://github.com/lxqt/qtermwidget>

## Build and test dependencies (not distributed)

CMake, Ninja, GCC/Clang, and the Qt Test module are used only to build and
test Lite Studio. They are not part of the distributed binaries and are not
covered by this notice.
