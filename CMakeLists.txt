cmake_minimum_required(VERSION 3.21)

# Version comes from the nearest git tag (vX.Y.Z, optionally -N-gHASH and
# -dirty). Source tarballs without .git fall back to the release version.
set(LITE_STUDIO_FALLBACK_VERSION 0.1.0)
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v[0-9]*" --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE LITE_STUDIO_FULL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE LITE_STUDIO_GIT_RESULT
        ERROR_QUIET
    )
endif()
if(DEFINED LITE_STUDIO_GIT_RESULT AND LITE_STUDIO_GIT_RESULT EQUAL 0 AND
   LITE_STUDIO_FULL_VERSION MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(LITE_STUDIO_VERSION ${CMAKE_MATCH_1})
    string(REGEX REPLACE "^v" "" LITE_STUDIO_FULL_VERSION
        "${LITE_STUDIO_FULL_VERSION}")
else()
    set(LITE_STUDIO_VERSION ${LITE_STUDIO_FALLBACK_VERSION})
    set(LITE_STUDIO_FULL_VERSION ${LITE_STUDIO_VERSION})
endif()
message(STATUS "Lite Studio version: ${LITE_STUDIO_FULL_VERSION}")

project(LiteStudio
    VERSION ${LITE_STUDIO_VERSION}
    DESCRIPTION "Lightweight project-aware code viewer/editor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LITE_STUDIO_WERROR "Treat compiler warnings as errors (CI)" OFF)
option(LITE_STUDIO_SANITIZERS "Build with AddressSanitizer and UBSan" OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(LITE_STUDIO_WERROR)
        add_compile_options(-Werror)
    endif()
endif()

if(LITE_STUDIO_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Building with AddressSanitizer + UBSan")
    add_compile_options(
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )
    add_link_options(-fsanitize=address,undefined)
endif()

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
        /opt/homebrew/opt/qscintilla2/include
        /usr/local/opt/qscintilla2/include
        /opt/homebrew/include
        /usr/local/include
        /usr/include/qt6
        /usr/include
        /app/include
)

find_library(QSCINTILLA_LIBRARY
    NAMES qscintilla2_qt6
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/.deps/usr/lib64
        /opt/homebrew/opt/qscintilla2/lib
        /usr/local/opt/qscintilla2/lib
        /opt/homebrew/lib
        /usr/local/lib
        /usr/lib64
        /usr/lib
        /app/lib
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
        /opt/homebrew/opt/qtermwidget/include/qtermwidget6
        /usr/local/opt/qtermwidget/include/qtermwidget6
        /opt/homebrew/include/qtermwidget6
        /usr/local/include/qtermwidget6
        /usr/include/qtermwidget6
        /app/include/qtermwidget6
)
find_library(QTERMWIDGET_LIBRARY
    NAMES qtermwidget6
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/.deps/usr/lib64
        /opt/homebrew/opt/qtermwidget/lib
        /usr/local/opt/qtermwidget/lib
        /opt/homebrew/lib
        /usr/local/lib
        /usr/lib64
        /usr/lib
        /app/lib
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

# Embedded window/taskbar icon. PNG data needs no extra Qt image plugin.
qt_add_resources(lite-studio "app-icons"
    PREFIX "/"
    BASE "packaging/icons"
    FILES
        packaging/icons/lite-studio-16.png
        packaging/icons/lite-studio-32.png
        packaging/icons/lite-studio-48.png
        packaging/icons/lite-studio-64.png
        packaging/icons/lite-studio-128.png
        packaging/icons/lite-studio-256.png
        packaging/icons/lite-studio-512.png
        packaging/icons/lite-studio-1024.png
)

qt_add_resources(lite-studio "panel-icons"
    PREFIX "/panel-icons"
    BASE "resources/panel-icons"
    FILES
        resources/panel-icons/android-logcat.png
        resources/panel-icons/android.png
        resources/panel-icons/console.png
        resources/panel-icons/git.png
        resources/panel-icons/ios.png
        resources/panel-icons/search.png
        resources/panel-icons/terminal.png
)

qt_add_resources(lite-studio "terminal-schemes"
    PREFIX "/qtermwidget"
    BASE "resources/qtermwidget"
    FILES
        resources/qtermwidget/color-schemes/LiteStudioDark.colorscheme
        resources/qtermwidget/color-schemes/LiteStudioLight.colorscheme
)

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
    set(LITE_STUDIO_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/LiteStudio.icns")
    set_source_files_properties("${LITE_STUDIO_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(lite-studio PRIVATE "${LITE_STUDIO_ICNS}")
    set_target_properties(lite-studio PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.diegosoriarios.LiteStudio"
        MACOSX_BUNDLE_BUNDLE_NAME "Lite Studio"
        MACOSX_BUNDLE_BUNDLE_VERSION "${LITE_STUDIO_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${LITE_STUDIO_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "LiteStudio.icns"
    )
endif()

target_compile_definitions(lite-studio PRIVATE
    LITE_STUDIO_VERSION="${LITE_STUDIO_FULL_VERSION}")

install(TARGETS lite-studio
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .)
install(FILES LICENSE THIRD-PARTY-NOTICES.md
    DESTINATION share/doc/lite-studio)
if(UNIX AND NOT APPLE)
    install(FILES packaging/lite-studio.desktop DESTINATION share/applications)
    install(FILES packaging/io.github.diegosoriarios.LiteStudio.metainfo.xml
        DESTINATION share/metainfo)
    install(FILES packaging/lite-studio.svg
        DESTINATION share/icons/hicolor/scalable/apps)
    foreach(icon_size 16 32 48 64 128 256 512 1024)
        install(FILES packaging/icons/lite-studio-${icon_size}.png
            DESTINATION share/icons/hicolor/${icon_size}x${icon_size}/apps
            RENAME lite-studio.png)
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Headless test targets (no GUI interaction required; widget tests run with
# the offscreen platform).
# ---------------------------------------------------------------------------
qt_add_executable(fileio-test
    tests/FileIOTest.cpp
    src/FileIO.h
    src/FileIO.cpp
)
target_include_directories(fileio-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fileio-test PRIVATE Qt6::Core)

qt_add_executable(editortab-test
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
    tests/ProjectTest.cpp
    src/ProjectDetector.h
    src/ProjectDetector.cpp
)
target_include_directories(project-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(project-test PRIVATE Qt6::Core)

qt_add_executable(android-test
    tests/AndroidToolingTest.cpp
    src/AndroidTooling.h
    src/AndroidTooling.cpp
)
target_include_directories(android-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(android-test PRIVATE Qt6::Core)

qt_add_executable(apple-test
    tests/AppleToolingTest.cpp
    src/AppleTooling.h
    src/AppleTooling.cpp
)
target_include_directories(apple-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(apple-test PRIVATE Qt6::Core)

qt_add_executable(task-test
    tests/TaskPresetsTest.cpp
    src/TaskPresets.h
    src/TaskPresets.cpp
)
target_include_directories(task-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(task-test PRIVATE Qt6::Core)

qt_add_executable(codetools-test
    tests/CodeToolsTest.cpp
    src/CodeTools.h
    src/CodeTools.cpp
)
target_include_directories(codetools-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(codetools-test PRIVATE Qt6::Core)

qt_add_executable(recovery-test
    tests/RecoveryStoreTest.cpp
    src/RecoveryStore.h
    src/RecoveryStore.cpp
)
target_include_directories(recovery-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(recovery-test PRIVATE Qt6::Core)

qt_add_executable(editorconfig-test
    tests/EditorConfigTest.cpp
    src/EditorConfig.h
    src/EditorConfig.cpp
)
target_include_directories(editorconfig-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(editorconfig-test PRIVATE Qt6::Core)

qt_add_executable(git-test
    tests/GitToolingTest.cpp
    src/GitTooling.h
    src/GitTooling.cpp
)
target_include_directories(git-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(git-test PRIVATE Qt6::Core)

qt_add_executable(filecollector-test
    tests/FileCollectorTest.cpp
    src/FileCollector.h
    src/FileCollector.cpp
    src/FileTreeView.h
    src/FileTreeView.cpp
)
target_include_directories(filecollector-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(filecollector-test PRIVATE Qt6::Widgets)

qt_add_executable(filetree-test
    tests/FileTreeViewTest.cpp
    src/FileTreeView.h
    src/FileTreeView.cpp
)
target_include_directories(filetree-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(filetree-test PRIVATE Qt6::Widgets)

# ---------------------------------------------------------------------------
# CTest registration. `ctest --test-dir build --output-on-failure` runs every
# headless suite; the `check` target builds them all first. Widget suites force
# the offscreen platform so they also work on a headless machine/CI runner.
# ---------------------------------------------------------------------------
enable_testing()

set(LITE_STUDIO_TESTS
    fileio editortab process project android apple task codetools
    recovery editorconfig git filecollector filetree
)

set(LITE_STUDIO_TEST_TARGETS "")
foreach(test_name ${LITE_STUDIO_TESTS})
    add_test(NAME ${test_name} COMMAND ${test_name}-test)
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 120
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
    list(APPEND LITE_STUDIO_TEST_TARGETS ${test_name}-test)
endforeach()

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${LITE_STUDIO_TEST_TARGETS}
    USES_TERMINAL
    COMMENT "Building and running all headless test suites"
)
