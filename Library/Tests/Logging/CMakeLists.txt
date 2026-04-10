cmake_minimum_required(VERSION 3.29)

set(DS_EXE_NAME test_ds_logging)

project(${DS_EXE_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Quick Test)
qt_standard_project_setup(REQUIRES 6.9)

enable_testing()

qt_add_executable(${DS_EXE_NAME}
    tst_dsloggingtest.cpp
)

target_link_libraries(${DS_EXE_NAME} PRIVATE
    Dsqt::Core Qt6::Test
)

# Console output is required for test runners (ctest, CI)
set_target_properties(${DS_EXE_NAME} PROPERTIES
    WIN32_EXECUTABLE FALSE
)

# Register with ctest — output JUnit XML for CI reporting
add_test(NAME ${DS_EXE_NAME} COMMAND ${DS_EXE_NAME}
    -o "${CMAKE_CURRENT_BINARY_DIR}/${DS_EXE_NAME}.xml,junitxml"
    -o -,txt
)
set_tests_properties(${DS_EXE_NAME} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
