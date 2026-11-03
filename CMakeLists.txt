cmake_minimum_required(VERSION 3.21)

project(LiteStudio
    VERSION 0.1.0
    DESCRIPTION "Lightweight project-aware code viewer/editor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt >= 6.4 (QStringDecoder/QStringEncoder used by FileIO).
# On macOS, Homebrew installs Qt/QScintilla outside the default search paths.
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)
qt_standard_project_setup()

# ---------------------------------------------------------------------------
# QScintilla (Qt6). Fedora's package ships no CMake config, so we locate the
# headers and library manually. A project-local copy may live in .deps/
# extracted from the Fedora RPMs when root is unavailable.
#
# Fedora:        sudo dnf install qscintilla-qt6-devel
# Debian/Ubuntu: sudo apt install libqscintilla2-qt6-dev
# ---------------------------------------------------------------------------
find_path(QSCINTILLA_INCLUDE_DIR
    NAMES Qsci/qsciscintilla.h
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/.deps/usr/include/qt6
        /opt/homebrew/include
        /usr/local/include
        /usr/include/qt6
        /usr/include
)

find_library(QSCINTILLA_LIBRARY
    NAMES qscintilla2_qt6
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/.deps/usr/lib64
        /opt/homebrew/lib
        /usr/local/lib
        /usr/lib64
        /usr/lib
)

if(NOT QSCINTILLA_INCLUDE_DIR)
    message(FATAL_ERROR
        "QScintilla headers not found. Install qscintilla-qt6-devel "
        "(Fedora) or libqscintilla2-qt6-dev (Debian/Ubuntu).")
endif()
if(NOT QSCINTILLA_LIBRARY)
    message(FATAL_ERROR
        "QScintilla library (libqscintilla2_qt6) not found. "
        "Install qscintilla-qt6-devel (Fedora).")
endif()

message(STATUS "QScintilla include: ${QSCINTILLA_INCLUDE_DIR}")
message(STATUS "QScintilla library: ${QSCINTILLA_LIBRARY}")

# ---------------------------------------------------------------------------
# Optional embedded terminal (qtermwidget). When absent, the Terminal tab is
# simply not compiled in. Fedora: sudo dnf install qtermwidget-devel
# ---------------------------------------------------------------------------
find_path(QTERMWIDGET_INCLUDE_DIR
    NAMES qtermwidget.h
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/.deps/usr/include/qtermwidget6
        /opt/homebrew/include/qtermwidget6
        /usr/local/include/qtermwidget6
        /usr/include/qtermwidget6
)
find_library(QTERMWIDGET_LIBRARY
    NAMES qtermwidget6
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/.deps/usr/lib64
        /opt/homebrew/lib
        /usr/local/lib
        /usr/lib64
        /usr/lib
)
if(QTERMWIDGET_INCLUDE_DIR AND QTERMWIDGET_LIBRARY)
    message(STATUS "qtermwidget found: ${QTERMWIDGET_LIBRARY} (Terminal tab enabled)")
else()
    message(STATUS "qtermwidget not found - embedded Terminal tab disabled")
endif()

qt_add_executable(lite-studio
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/FileTreeView.h
    src/FileTreeView.cpp
    src/EditorTab.h
    src/EditorTab.cpp
    src/FileIO.h
    src/FileIO.cpp
    src/LexerFactory.h
    src/LexerFactory.cpp
    src/SimpleLexer.h
    src/SimpleLexer.cpp
    src/Theme.h
    src/Theme.cpp
    src/FindBar.h
    src/FindBar.cpp
    src/ProjectSearchPanel.h
    src/ProjectSearchPanel.cpp
    src/FileCollector.h
    src/FileCollector.cpp
    src/FuzzyMatch.h
    src/FuzzyMatch.cpp
    src/QuickOpen.h
    src/QuickOpen.cpp
    src/ProcessUtil.h
    src/ProcessUtil.cpp
    src/ProcessRunner.h
    src/ProcessRunner.cpp
    src/ProcessOutputView.h
    src/ProcessOutputView.cpp
    src/LineBuffer.h
    src/LineBuffer.cpp
    src/ProjectDetector.h
    src/ProjectDetector.cpp
    src/ConsolePanel.h
    src/ConsolePanel.cpp
    src/CodeTools.h
    src/CodeTools.cpp
    src/AndroidTooling.h
    src/AndroidTooling.cpp
    src/AndroidPanel.h
    src/AndroidPanel.cpp
    src/AppleTooling.h
    src/AppleTooling.cpp
    src/ApplePanel.h
    src/ApplePanel.cpp
    src/TaskPresets.h
    src/TaskPresets.cpp
    src/RecoveryStore.h
    src/RecoveryStore.cpp
    src/EditorConfig.h
    src/EditorConfig.cpp
    src/GitTooling.h
    src/GitTooling.cpp
    src/GitPanel.h
    src/GitPanel.cpp
)

target_include_directories(lite-studio PRIVATE
    ${QSCINTILLA_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(lite-studio PRIVATE
    Qt6::Widgets
    ${QSCINTILLA_LIBRARY}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lite-studio PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(QTERMWIDGET_INCLUDE_DIR AND QTERMWIDGET_LIBRARY)
    target_sources(lite-studio PRIVATE
        src/TerminalPanel.h
        src/TerminalPanel.cpp
    )
    target_include_directories(lite-studio PRIVATE ${QTERMWIDGET_INCLUDE_DIR})
    target_link_libraries(lite-studio PRIVATE ${QTERMWIDGET_LIBRARY})
    target_compile_definitions(lite-studio PRIVATE LITE_STUDIO_HAVE_QTERMWIDGET)
endif()

# Keep locally extracted libraries resolvable at runtime.
set(LITE_RPATH "")
if(QSCINTILLA_LIBRARY MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}/.deps")
    list(APPEND LITE_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/.deps/usr/lib64")
endif()
if(QTERMWIDGET_LIBRARY MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}/.deps")
    list(APPEND LITE_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/.deps/usr/lib64")
endif()
if(LITE_RPATH)
    set_target_properties(lite-studio PROPERTIES BUILD_RPATH "${LITE_RPATH}")
endif()

if(APPLE)
    set_target_properties(lite-studio PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.litestudio.app"
        MACOSX_BUNDLE_BUNDLE_NAME "Lite Studio"
    )
endif()

install(TARGETS lite-studio RUNTIME DESTINATION bin)
if(UNIX AND NOT APPLE)
    install(FILES packaging/lite-studio.desktop DESTINATION share/applications)
    install(FILES packaging/lite-studio.svg
        DESTINATION share/icons/hicolor/scalable/apps)
endif()

# ---------------------------------------------------------------------------
# Headless test targets (no GUI interaction required; widget tests run with
# the offscreen platform).
# ---------------------------------------------------------------------------
qt_add_executable(fileio-test
    EXCLUDE_FROM_ALL
    tests/FileIOTest.cpp
    src/FileIO.h
    src/FileIO.cpp
)
target_include_directories(fileio-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fileio-test PRIVATE Qt6::Core)

qt_add_executable(editortab-test
    EXCLUDE_FROM_ALL
    tests/EditorTabTest.cpp
    src/CodeTools.h
    src/CodeTools.cpp
    src/EditorTab.h
    src/EditorTab.cpp
    src/FileCollector.h
    src/FileCollector.cpp
    src/FileIO.h
    src/FileIO.cpp
    src/FileTreeView.h
    src/FileTreeView.cpp
    src/FuzzyMatch.h
    src/FuzzyMatch.cpp
    src/LexerFactory.h
    src/LexerFactory.cpp
    src/RecoveryStore.h
    src/RecoveryStore.cpp
    src/SimpleLexer.h
    src/SimpleLexer.cpp
    src/Theme.h
    src/Theme.cpp
)
target_include_directories(editortab-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${QSCINTILLA_INCLUDE_DIR}
)
target_link_libraries(editortab-test PRIVATE
    Qt6::Widgets
    ${QSCINTILLA_LIBRARY}
)

qt_add_executable(process-test
    EXCLUDE_FROM_ALL
    tests/ProcessTest.cpp
    src/LineBuffer.h
    src/LineBuffer.cpp
    src/ProcessUtil.h
    src/ProcessUtil.cpp
    src/ProcessRunner.h
    src/ProcessRunner.cpp
)
target_include_directories(process-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(process-test PRIVATE Qt6::Core)

qt_add_executable(project-test
    EXCLUDE_FROM_ALL
    tests/ProjectTest.cpp
    src/ProjectDetector.h
    src/ProjectDetector.cpp
)
target_include_directories(project-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(project-test PRIVATE Qt6::Core)

qt_add_executable(android-test
    EXCLUDE_FROM_ALL
    tests/AndroidToolingTest.cpp
    src/AndroidTooling.h
    src/AndroidTooling.cpp
)
target_include_directories(android-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(android-test PRIVATE Qt6::Core)

qt_add_executable(apple-test
    EXCLUDE_FROM_ALL
    tests/AppleToolingTest.cpp
    src/AppleTooling.h
    src/AppleTooling.cpp
)
target_include_directories(apple-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(apple-test PRIVATE Qt6::Core)

qt_add_executable(task-test
    EXCLUDE_FROM_ALL
    tests/TaskPresetsTest.cpp
    src/TaskPresets.h
    src/TaskPresets.cpp
)
target_include_directories(task-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(task-test PRIVATE Qt6::Core)

qt_add_executable(codetools-test
    EXCLUDE_FROM_ALL
    tests/CodeToolsTest.cpp
    src/CodeTools.h
    src/CodeTools.cpp
)
target_include_directories(codetools-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(codetools-test PRIVATE Qt6::Core)

qt_add_executable(recovery-test
    EXCLUDE_FROM_ALL
    tests/RecoveryStoreTest.cpp
    src/RecoveryStore.h
    src/RecoveryStore.cpp
)
target_include_directories(recovery-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(recovery-test PRIVATE Qt6::Core)

qt_add_executable(editorconfig-test
    EXCLUDE_FROM_ALL
    tests/EditorConfigTest.cpp
    src/EditorConfig.h
    src/EditorConfig.cpp
)
target_include_directories(editorconfig-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(editorconfig-test PRIVATE Qt6::Core)

qt_add_executable(git-test
    EXCLUDE_FROM_ALL
    tests/GitToolingTest.cpp
    src/GitTooling.h
    src/GitTooling.cpp
)
target_include_directories(git-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(git-test PRIVATE Qt6::Core)
