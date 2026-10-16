cmake_minimum_required(VERSION 3.29)

# The standalone route builds the real Core module without the unrelated media SDKs.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DsqtEffectsTests VERSION 1.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTORCC ON)
    find_package(Qt6 6.9 REQUIRED COMPONENTS QuickTest Qml Quick Widgets)
    qt_standard_project_setup(REQUIRES 6.9)
    enable_testing()

    option(DSQT_EFFECTS_USE_INSTALLED "Test an installed DsQt package" OFF)
    if(DSQT_EFFECTS_USE_INSTALLED)
        find_package(Dsqt CONFIG REQUIRED)
    else()
        find_package(tomlplusplus CONFIG REQUIRED)
        set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")
        add_subdirectory("../../DsQt/Core" "${CMAKE_CURRENT_BINARY_DIR}/Core")
    endif()
else()
    find_package(Qt6 6.9 REQUIRED COMPONENTS QuickTest)
endif()

qt_add_executable(test_ds_multieffect quicktest.cpp)
target_link_libraries(test_ds_multieffect
    PRIVATE
        Qt6::QuickTest
        Qt6::Widgets
        Dsqt::Core
)
target_compile_definitions(test_ds_multieffect
    PRIVATE
        QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
if(TARGET Coreplugin)
    get_target_property(_effects_plugin_type Coreplugin TYPE)
else()
    get_target_property(_effects_plugin_type Dsqt::Coreplugin TYPE)
endif()
if(_effects_plugin_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_ds_multieffect PRIVATE DSQT_EFFECTS_STATIC_PLUGIN)
endif()
set_target_properties(test_ds_multieffect PROPERTIES WIN32_EXECUTABLE FALSE)

add_test(NAME ds_multieffect COMMAND test_ds_multieffect -o -,txt)
set_tests_properties(ds_multieffect PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    TIMEOUT 120
    ENVIRONMENT "QML_DISABLE_DISK_CACHE=1"
)