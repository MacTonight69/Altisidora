# Test infrastructure for Altisidora

# Helper library for shared test utilities
add_library(TestHelpers STATIC
    TestHelpers.cpp
    TestHelpers.h
)

target_link_libraries(TestHelpers PUBLIC
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

target_include_directories(TestHelpers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Unit tests
set(UNIT_TESTS
    tst_database
    tst_models
    tst_player
    tst_scanner
    tst_metadata
    tst_library
    tst_playlist
    tst_settings
    tst_theme_fonts
    tst_mpris
)

# Integration tests
set(INTEGRATION_TESTS
    tst_library_scan
    tst_playback
)

# Production sources exercised by specific tests.
# Keyed by test name: <test>_PROD_SOURCES.
set(tst_models_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/models/Song.cpp
)
set(tst_database_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/database/DatabaseManager.cpp
)
set(tst_player_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/audio/PlayerController.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/PlayerController.h
    ${CMAKE_SOURCE_DIR}/src/audio/PlaybackState.h
)
set(tst_scanner_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/library/Scanner.cpp
)
set(tst_settings_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/app/SettingsController.cpp
)
set(tst_theme_fonts_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/app/SettingsController.cpp
)
set(tst_metadata_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/library/MetadataReader.cpp
    ${CMAKE_SOURCE_DIR}/src/library/Scanner.cpp
)
set(tst_playlist_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/app/PlaylistController.cpp
    ${CMAKE_SOURCE_DIR}/src/database/DatabaseManager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/Song.cpp
)
set(tst_library_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/library/LibraryController.cpp
    ${CMAKE_SOURCE_DIR}/src/library/Scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/library/MetadataReader.cpp
    ${CMAKE_SOURCE_DIR}/src/database/DatabaseManager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/Song.cpp
)
set(tst_mpris_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/mpris/MprisPlayer.cpp
    ${CMAKE_SOURCE_DIR}/src/mpris/MprisPlayer.h
    ${CMAKE_SOURCE_DIR}/src/audio/PlayerController.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/PlayerController.h
    ${CMAKE_SOURCE_DIR}/src/audio/PlaybackState.h
)
set(tst_library_scan_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/library/LibraryController.cpp
    ${CMAKE_SOURCE_DIR}/src/library/Scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/library/MetadataReader.cpp
    ${CMAKE_SOURCE_DIR}/src/database/DatabaseManager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/Song.cpp
)
set(tst_playback_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/audio/PlayerController.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/PlayerController.h
    ${CMAKE_SOURCE_DIR}/src/audio/PlaybackState.h
    ${CMAKE_SOURCE_DIR}/src/library/LibraryController.cpp
    ${CMAKE_SOURCE_DIR}/src/library/Scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/library/MetadataReader.cpp
    ${CMAKE_SOURCE_DIR}/src/database/DatabaseManager.cpp
    ${CMAKE_SOURCE_DIR}/src/models/Song.cpp
)

# All tests share the same sources/linking rules; unit tests live under unit/
# with a 60s timeout, integration tests under integration/ with 120s.
foreach(test_name ${UNIT_TESTS} ${INTEGRATION_TESTS})
    if(test_name IN_LIST INTEGRATION_TESTS)
        set(test_subdir integration)
        set(test_timeout 120)
    else()
        set(test_subdir unit)
        set(test_timeout 60)
    endif()

    add_executable(${test_name}
        ${test_subdir}/${test_name}.cpp
    )

    # Link real production sources when the test exercises them
    if(DEFINED ${test_name}_PROD_SOURCES)
        target_sources(${test_name} PRIVATE ${${test_name}_PROD_SOURCES})
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${TAGLIB_INCLUDE_DIRS}
        )
        target_link_libraries(${test_name} PRIVATE
            ${TAGLIB_LIBRARIES}
        )
    endif()

    target_link_libraries(${test_name} PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Sql
        Qt6::Concurrent
        TestHelpers
        PkgConfig::MPV
    )

    # tst_theme_fonts renders the QML theme files directly.
    if(test_name STREQUAL "tst_theme_fonts")
        target_link_libraries(${test_name} PRIVATE Qt6::Qml Qt6::Quick)
        target_compile_definitions(${test_name} PRIVATE
            THEME_QML_DIR="${CMAKE_SOURCE_DIR}/resources/qml/themes"
        )
    endif()

    # tst_mpris registers a D-Bus service: link DBus and run it inside a
    # private bus.
    if(test_name STREQUAL "tst_mpris")
        target_link_libraries(${test_name} PRIVATE Qt6::DBus)
        add_test(
            NAME ${test_name}
            COMMAND dbus-run-session -- $<TARGET_FILE:${test_name}>
        )
    else()
        add_test(
            NAME ${test_name}
            COMMAND ${test_name}
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${test_name} PRIVATE
            -Wall
            -Wextra
        )
    endif()

    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT ${test_timeout}
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endforeach()
