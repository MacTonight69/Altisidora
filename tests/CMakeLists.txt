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
    tst_detail_view
)

# Integration tests
set(INTEGRATION_TESTS
    tst_library_scan
    tst_playback
)

# All tests link the shared production core (compiled once) instead of each
# recompiling the production .cpp files it needs. `altisidora_core` PUBLIC-links
# Qt Core/Sql/Concurrent/DBus, TagLib and mpv, so consumers inherit them.
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

    target_link_libraries(${test_name} PRIVATE
        altisidora_core
        Qt6::Test
        TestHelpers
    )

    # tst_theme_fonts renders the QML theme files directly.
    if(test_name STREQUAL "tst_theme_fonts")
        target_link_libraries(${test_name} PRIVATE Qt6::Qml Qt6::Quick)
        target_compile_definitions(${test_name} PRIVATE
            THEME_QML_DIR="${CMAKE_SOURCE_DIR}/resources/qml/themes"
        )
    endif()

    # tst_detail_view loads the real drill-down view (which no other test
    # ever instantiates) against the real Theme.
    if(test_name STREQUAL "tst_detail_view")
        target_link_libraries(${test_name} PRIVATE Qt6::Qml Qt6::Quick)
        target_compile_definitions(${test_name} PRIVATE
            THEME_QML_DIR="${CMAKE_SOURCE_DIR}/resources/qml/themes"
            VIEWS_QML_DIR="${CMAKE_SOURCE_DIR}/resources/qml/views"
        )
    endif()

    # tst_mpris registers a D-Bus service: link DBus (already pulled in by the
    # core) and run it inside a private bus.
    if(test_name STREQUAL "tst_mpris")
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
