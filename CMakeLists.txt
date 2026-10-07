cmake_minimum_required(VERSION 3.20)

project(Benzaiten
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
    Multimedia
    Sql
    Test
)

# Find TagLib
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED taglib)

# Source files
set(SOURCES
    src/main.cpp
    src/audio/PlayerController.cpp
    src/audio/PlaybackState.h
    src/app/PlaylistController.cpp
    src/app/SettingsController.cpp
    src/library/LibraryController.cpp
    src/library/Scanner.cpp
    src/library/MetadataReader.cpp
    src/database/DatabaseManager.cpp
    src/models/Song.cpp
    src/models/Artist.cpp
    src/models/Album.cpp
    src/models/Genre.cpp
)

# Resources
qt_add_resources(RESOURCES resources/qml.qrc)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${RESOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Sql
    ${TAGLIB_LIBRARIES}
)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
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

# Install
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
