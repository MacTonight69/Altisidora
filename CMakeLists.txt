cmake_minimum_required(VERSION 3.20)

project(Altisidora
    VERSION 0.1.0
    DESCRIPTION "Native Linux Music Player"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickControls2
    Sql
    Concurrent
    DBus
    Test
)

# Find TagLib
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED taglib)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

# Shared production core: compiled once as a static library and linked by both
# the application and every test target. Previously each test recompiled the
# production .cpp files it needed (e.g. DatabaseManager.cpp up to five times),
# which dominated clean and incremental build time.
set(CORE_SOURCES
    src/audio/PlayerController.cpp
    src/audio/PlaybackState.h
    src/app/PlaylistController.cpp
    src/app/SettingsController.cpp
    src/library/LibraryController.cpp
    src/library/SongListModel.cpp
    src/library/Scanner.cpp
    src/library/MetadataReader.cpp
    src/database/DatabaseManager.cpp
    src/mpris/MprisPlayer.cpp
    src/mpris/MprisPlayer.h
    src/models/Song.cpp
)

add_library(altisidora_core STATIC ${CORE_SOURCES})

# The public headers pull in these dependencies, so consumers (the app and the
# tests) inherit the include directories and link libraries transitively.
target_include_directories(altisidora_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(altisidora_core PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Concurrent
    Qt6::DBus
    ${TAGLIB_LIBRARIES}
    PkgConfig::MPV
)

# Resources
qt_add_resources(RESOURCES resources/qml.qrc)

# Create executable: only main.cpp plus the QML/resources; the audio, library
# and database code lives in altisidora_core.
add_executable(${PROJECT_NAME} src/main.cpp ${RESOURCES})

# The installed name (and desktop Exec=) is `altisidora`.
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME altisidora)

# GUI stack, needed only by the application (tests link the subset they use).
target_link_libraries(${PROJECT_NAME} PRIVATE
    altisidora_core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)

# Compiler warnings: the core carries its own copy for its sources; the app
# keeps the same set for main.cpp.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(altisidora_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
    )
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
    )
endif()

# Tests
enable_testing()
add_subdirectory(tests)

# Install: binary (`altisidora`), desktop entry and icon.
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(FILES ${CMAKE_SOURCE_DIR}/altisidora.desktop
    DESTINATION share/applications
    RENAME altisidora.desktop
)

if(EXISTS ${CMAKE_SOURCE_DIR}/images/LogoMenu.png)
    install(FILES ${CMAKE_SOURCE_DIR}/images/LogoMenu.png
        DESTINATION share/icons/hicolor/512x512/apps
        RENAME altisidora.png
    )
endif()
