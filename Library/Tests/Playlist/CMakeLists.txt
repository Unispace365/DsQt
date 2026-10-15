find_package(Qt6 6.12 REQUIRED COMPONENTS Test Quick Qml Multimedia)
qt_add_executable(test_ds_playlist quicktest.cpp)
target_link_libraries(test_ds_playlist PRIVATE
    Qt6::Test Qt6::Quick Qt6::Qml Qt6::Multimedia Dsqt::Waffles Dsqt::Core)
target_compile_definitions(test_ds_playlist PRIVATE
    DSQT_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../Examples/AssetViewer/data")
set_target_properties(test_ds_playlist PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME test_ds_playlist COMMAND test_ds_playlist
    -o "${CMAKE_CURRENT_BINARY_DIR}/playlist-results.xml,junitxml" -o -,txt)
set_tests_properties(test_ds_playlist PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_IM_MODULE=qtvirtualkeyboard"
    TIMEOUT 300)
