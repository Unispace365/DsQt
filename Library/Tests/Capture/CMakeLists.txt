find_package(Qt6 6.12 REQUIRED COMPONENTS Test Multimedia Qml)
qt_add_executable(test_ds_capture
    tst_dscapture.cpp
    ../../DsQt/Waffles/capture/dsCaptureSession.h
    ../../DsQt/Waffles/capture/dsCaptureSession.cpp
    ../../DsQt/Waffles/capture/dsCaptureSource.h
    ../../DsQt/Waffles/capture/dsCaptureSource.cpp
)
target_include_directories(test_ds_capture PRIVATE ../../DsQt/Waffles/capture)
target_link_libraries(test_ds_capture PRIVATE Qt6::Test Qt6::Multimedia Qt6::Qml)
add_test(NAME ds_capture COMMAND test_ds_capture -o "${CMAKE_CURRENT_BINARY_DIR}/capture-results.txt,txt")
set_tests_properties(ds_capture PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
