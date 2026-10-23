cmake_minimum_required(VERSION 3.29)

set(DS_EXE_NAME test_ds_bridge_query)

project(${DS_EXE_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Multimedia MultimediaWidgets Sql Test)
qt_standard_project_setup(REQUIRES 6.9)

enable_testing()

qt_add_executable(${DS_EXE_NAME}
    tst_dsbridgequerytest.cpp
)

target_link_libraries(${DS_EXE_NAME} PRIVATE
    Dsqt::Bridge Qt6::Sql Qt6::Test
)

# Console output is required for test runners (ctest, CI).
set_target_properties(${DS_EXE_NAME} PROPERTIES
    WIN32_EXECUTABLE FALSE
)

add_test(NAME ${DS_EXE_NAME} COMMAND ${DS_EXE_NAME}
    -o "${CMAKE_CURRENT_BINARY_DIR}/${DS_EXE_NAME}.xml,junitxml"
    -o -,txt
)
set_tests_properties(${DS_EXE_NAME} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

find_package(Qt6 6.12 REQUIRED COMPONENTS Qml Quick)
qt_add_executable(test_ds_content_launcher tst_dscontentlauncher.cpp)
target_link_libraries(test_ds_content_launcher PRIVATE
    Dsqt::Bridge Dsqt::Waffles Qt6::Qml Qt6::Quick Qt6::Test)
target_compile_definitions(test_ds_content_launcher PRIVATE
    DSQT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set_target_properties(test_ds_content_launcher PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME test_ds_content_launcher COMMAND test_ds_content_launcher
    -o "${CMAKE_CURRENT_BINARY_DIR}/content-launcher-results.xml,junitxml" -o -,txt)
set_tests_properties(test_ds_content_launcher PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    TIMEOUT 120)
