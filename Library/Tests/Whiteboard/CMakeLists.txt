find_package(Qt6 6.12 REQUIRED COMPONENTS QuickTest Qml)
qt_add_executable(test_ds_whiteboard quicktest.cpp)
target_link_libraries(test_ds_whiteboard PRIVATE
    Qt6::QuickTest Qt6::Qml Dsqt::Waffles Dsqt::Core)
target_compile_definitions(test_ds_whiteboard PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
set_target_properties(test_ds_whiteboard PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME test_ds_whiteboard COMMAND test_ds_whiteboard
    -o "${CMAKE_CURRENT_BINARY_DIR}/whiteboard-results.xml,junitxml" -o -,txt)
set_tests_properties(test_ds_whiteboard PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" TIMEOUT 120)
