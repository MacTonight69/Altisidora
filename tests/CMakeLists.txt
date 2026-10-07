# Test infrastructure for MusicRep

# Test-specific compile definitions
set(TEST_COMPILE_DEFINITIONS
    QT_TESTCASE_SOURCEDIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

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

# Mock objects library
add_library(TestMocks STATIC
    mocks/MockDatabase.cpp
    mocks/MockDatabase.h
    mocks/MockPlayer.cpp
    mocks/MockPlayer.h
)

target_link_libraries(TestMocks PUBLIC
    Qt6::Core
    Qt6::Test
    Qt6::Multimedia
    Qt6::Sql
)

target_include_directories(TestMocks PUBLIC
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
)

# Production sources exercised by specific tests
set(tst_settings_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/app/SettingsController.cpp
)
set(tst_playlist_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/app/PlaylistController.cpp
    ${CMAKE_SOURCE_DIR}/src/database/DatabaseManager.cpp
)
set(tst_library_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/library/LibraryController.cpp
    ${CMAKE_SOURCE_DIR}/src/library/Scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/library/MetadataReader.cpp
    ${CMAKE_SOURCE_DIR}/src/database/DatabaseManager.cpp
)

foreach(test_name ${UNIT_TESTS})
    add_executable(${test_name}
        unit/${test_name}.cpp
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

    target_compile_definitions(${test_name} PRIVATE
        ${TEST_COMPILE_DEFINITIONS}
    )

    target_link_libraries(${test_name} PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Multimedia
        Qt6::Sql
        TestHelpers
        TestMocks
    )

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
    )

    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 60
    )
endforeach()

# Integration tests
set(INTEGRATION_TESTS
    tst_library_scan
    tst_playback
)

# Production sources exercised by specific integration tests
set(tst_library_scan_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/library/LibraryController.cpp
    ${CMAKE_SOURCE_DIR}/src/library/Scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/library/MetadataReader.cpp
    ${CMAKE_SOURCE_DIR}/src/database/DatabaseManager.cpp
)

foreach(test_name ${INTEGRATION_TESTS})
    add_executable(${test_name}
        integration/${test_name}.cpp
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

    target_compile_definitions(${test_name} PRIVATE
        ${TEST_COMPILE_DEFINITIONS}
    )

    target_link_libraries(${test_name} PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Multimedia
        Qt6::Sql
        TestHelpers
        TestMocks
    )

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
    )

    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 120
    )
endforeach()
