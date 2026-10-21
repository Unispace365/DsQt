qt_add_executable(test_ds_qsys tst_dsqsys.cpp)
target_link_libraries(test_ds_qsys PRIVATE Dsqt::Qsys Qt6::Test)
set_target_properties(test_ds_qsys PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME test_ds_qsys COMMAND test_ds_qsys
    -o "${CMAKE_CURRENT_BINARY_DIR}/test_ds_qsys.xml,junitxml" -o -,txt)
