cmake_minimum_required(VERSION 3.29)
include(FetchContent)

set(DS_EXE_NAME test_ds_settings)

project(${DS_EXE_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Quick Test)
qt_standard_project_setup(REQUIRES 6.9)

enable_testing()

# When built as a subdirectory of the main library, link against Core directly.
# When built standalone, compile the settings sources ourselves.
if(TARGET Dsqt::Core)
    set(DSQT_SETTINGS_TEST_STANDALONE OFF)
else()
    set(DSQT_SETTINGS_TEST_STANDALONE ON)
endif()

qt_add_executable(${DS_EXE_NAME}
    tst_dssettingstest.cpp
)

if(DSQT_SETTINGS_TEST_STANDALONE)
    # Standalone build: compile settings sources and fetch dependencies
    set(DS_QT_PLATFORM_100 "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
    set(DS_QT_PLATFORM_100_LIBRARY "${DS_QT_PLATFORM_100}/Library/Dsqt")

    find_package(Qt6 REQUIRED COMPONENTS Gui Sql Qml)

    target_sources(${DS_EXE_NAME} PRIVATE
        "${DS_QT_PLATFORM_100_LIBRARY}/settings/dsQmlSettingsProxy.cpp"
        "${DS_QT_PLATFORM_100_LIBRARY}/settings/dsSettings.cpp"
        "${DS_QT_PLATFORM_100_LIBRARY}/settings/dsSettingsBase.cpp"
        "${DS_QT_PLATFORM_100_LIBRARY}/settings/dsSettingsCollections.cpp"
        "${DS_QT_PLATFORM_100_LIBRARY}/settings/dsSettingsDateTime.cpp"
        "${DS_QT_PLATFORM_100_LIBRARY}/settings/dsSettingsGeomCommon.cpp"
        "${DS_QT_PLATFORM_100_LIBRARY}/settings/dsSettingsGeomGlm.cpp"
        "${DS_QT_PLATFORM_100_LIBRARY}/settings/dsSettingsGeomQt.cpp"
        "${DS_QT_PLATFORM_100_LIBRARY}/settings/dsSettingsQColor.cpp"
        "${DS_QT_PLATFORM_100_LIBRARY}/core/dsEnvironment.cpp"
    )

    FetchContent_Declare(tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(tomlplusplus)

    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
    )
    FetchContent_MakeAvailable(glm)

    target_include_directories(${DS_EXE_NAME} PRIVATE
        ${DS_QT_PLATFORM_100_LIBRARY}
        ${tomlplusplus_SOURCE_DIR}/include
        ${glm_SOURCE_DIR}
    )

    target_link_libraries(${DS_EXE_NAME} PRIVATE
        Qt6::Core Qt6::Quick Qt6::Gui Qt6::Sql Qt6::Qml Qt6::Test
        tomlplusplus::tomlplusplus
    )
else()
    # Subdirectory build: link against the already-built Core target
    target_link_libraries(${DS_EXE_NAME} PRIVATE
        Dsqt::Core Qt6::Test
    )
endif()

# Console output is required for test runners (ctest, CI)
set_target_properties(${DS_EXE_NAME} PROPERTIES
    WIN32_EXECUTABLE FALSE
)

# Copy test data to build directory at build time so edits are picked up
# without needing a reconfigure.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/settings)
add_custom_command(TARGET ${DS_EXE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/settings/test_settings.toml"
        "${CMAKE_CURRENT_BINARY_DIR}/settings/test_settings.toml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/settings/engine.toml"
        "${CMAKE_CURRENT_BINARY_DIR}/settings/engine.toml"
    COMMENT "Copying test settings to build directory"
)

# Register with ctest — output JUnit XML for CI reporting
add_test(NAME ${DS_EXE_NAME} COMMAND ${DS_EXE_NAME}
    -o "${CMAKE_CURRENT_BINARY_DIR}/${DS_EXE_NAME}.xml,junitxml"
    -o -,txt
)
set_tests_properties(${DS_EXE_NAME} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Show test settings in IDE
file(GLOB_RECURSE TEST_SETTINGS LIST_DIRECTORIES false ${CMAKE_CURRENT_SOURCE_DIR}/settings/*)
add_custom_target(test_settings_files SOURCES ${TEST_SETTINGS})
