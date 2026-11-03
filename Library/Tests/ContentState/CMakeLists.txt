find_package(Qt6 6.12 REQUIRED COMPONENTS Test Quick Qml Multimedia)
qt_add_executable(test_ds_contentstate quicktest.cpp)
target_link_libraries(test_ds_contentstate PRIVATE
    Qt6::Test Qt6::Quick Qt6::Qml Qt6::Multimedia Dsqt::Waffles Dsqt::Core)
if(DSQT_WAFFLES_HAS_WebEngineQuick)
    find_package(Qt6WebEngineQuick 6.9 REQUIRED HINTS "${Qt6_DIR}/../Qt6WebEngineQuick")
    target_link_libraries(test_ds_contentstate PRIVATE Qt6::WebEngineQuick)
    target_compile_definitions(test_ds_contentstate PRIVATE DSQT_TEST_WEBENGINE)
endif()
set_target_properties(test_ds_contentstate PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME test_ds_contentstate COMMAND test_ds_contentstate
    -o "${CMAKE_CURRENT_BINARY_DIR}/contentstate-results.xml,junitxml" -o -,txt)
set_tests_properties(test_ds_contentstate PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion"
    TIMEOUT 120)
