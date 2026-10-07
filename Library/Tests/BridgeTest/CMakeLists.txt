cmake_minimum_required(VERSION 3.29)

# Unit tests for the generic Bridge types in dsBridgeDatabase.h: DatabaseRecord, DatabaseResource,
# DatabaseContent, DatabaseTree and the event helpers. They do not use a database or any project schema; records are
# built by hand, with the same value types that DsBridgeSqlQuery produces for each CMS field type.
#
# Enabled by -DDSQT_BUILD_TESTS=ON (see Library/CMakeLists.txt and run-tests.ps1).
# Some tests are marked with QEXPECT_FAIL: they document known bugs and will report "XPASS" once the bug is fixed.

project(test_ds_bridge VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Test)
qt_standard_project_setup(REQUIRES 6.9)

enable_testing()

# One executable per test class, so that every test gets its own result file.
function(add_bridge_test NAME)
    qt_add_executable(${NAME}
        ${NAME}.cpp
        testHelpers.h
    )

    target_link_libraries(${NAME} PRIVATE
        Dsqt::Bridge Qt6::Test
    )

    # Console output is required for test runners (ctest, CI)
    set_target_properties(${NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
    )

    # Register with ctest, and write JUnit XML for CI reporting.
    add_test(NAME ${NAME} COMMAND ${NAME}
        -o "${CMAKE_CURRENT_BINARY_DIR}/${NAME}.xml,junitxml"
        -o -,txt
    )
    set_tests_properties(${NAME} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()

add_bridge_test(tst_databaseresource)
add_bridge_test(tst_databaserecord)
add_bridge_test(tst_databasecontent)
add_bridge_test(tst_databaseevents)
