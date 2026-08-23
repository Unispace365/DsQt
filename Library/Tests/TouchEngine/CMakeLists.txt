cmake_minimum_required(VERSION 3.29)

set(CMAKE_AUTOMOC ON)
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 6.9 REQUIRED COMPONENTS GuiPrivate)

function(dsqt_touchengine_copy_runtime target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:DsqtTouchEngineSDK>"
            "$<TARGET_FILE_DIR:${target}>"
        VERBATIM
    )
endfunction()

function(dsqt_touchengine_configure_test test_name target)
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endfunction()

qt_add_executable(test_ds_touchengine_types
    cpp/tst_dstouchenginetypes.cpp
)
target_link_libraries(test_ds_touchengine_types PRIVATE
    Dsqt::TouchEngine
    Qt6::Test
)
set_target_properties(test_ds_touchengine_types PROPERTIES WIN32_EXECUTABLE FALSE)
dsqt_touchengine_copy_runtime(test_ds_touchengine_types)
add_test(NAME test_ds_touchengine_types
    COMMAND test_ds_touchengine_types
        -o "$<TARGET_FILE_DIR:test_ds_touchengine_types>/test_ds_touchengine_types.xml,junitxml"
        -o -,txt
)
dsqt_touchengine_configure_test(test_ds_touchengine_types test_ds_touchengine_types)

qt_add_executable(test_ds_touchengine_session
    cpp/tst_dstouchenginesession.cpp
)
target_link_libraries(test_ds_touchengine_session PRIVATE
    Dsqt::TouchEngine
    Qt6::Test
)
set_target_properties(test_ds_touchengine_session PROPERTIES WIN32_EXECUTABLE FALSE)
dsqt_touchengine_copy_runtime(test_ds_touchengine_session)
add_test(NAME test_ds_touchengine_session
    COMMAND test_ds_touchengine_session
        -o "$<TARGET_FILE_DIR:test_ds_touchengine_session>/test_ds_touchengine_session.xml,junitxml"
        -o -,txt
)
dsqt_touchengine_configure_test(test_ds_touchengine_session test_ds_touchengine_session)
set_tests_properties(test_ds_touchengine_session PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)

qt_add_executable(test_ds_touchengine_qml
    qml/main.cpp
)
target_compile_definitions(test_ds_touchengine_qml PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
target_link_libraries(test_ds_touchengine_qml PRIVATE
    Dsqt::TouchEngine
    TouchEngineplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickTest
)
set_target_properties(test_ds_touchengine_qml PROPERTIES WIN32_EXECUTABLE FALSE)
dsqt_touchengine_copy_runtime(test_ds_touchengine_qml)
add_test(NAME test_ds_touchengine_qml
    COMMAND test_ds_touchengine_qml
        -o "$<TARGET_FILE_DIR:test_ds_touchengine_qml>/test_ds_touchengine_qml.xml,junitxml"
        -o -,txt
)
dsqt_touchengine_configure_test(test_ds_touchengine_qml test_ds_touchengine_qml)
set_tests_properties(test_ds_touchengine_qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
    RESOURCE_LOCK touchengine_gpu
    TIMEOUT 60
)

option(DSQT_TOUCHENGINE_REAL_TESTS
    "Run TouchEngine GPU integration tests against a real tox on all RHI backends"
    OFF
)

if(DSQT_TOUCHENGINE_REAL_TESTS)
    qt_add_executable(test_ds_touchengine_real
        real/tst_touchengine_real.cpp
    )
    target_link_libraries(test_ds_touchengine_real PRIVATE
        Dsqt::TouchEngine
        Qt6::Gui
        Qt6::Quick
    )
    set_target_properties(test_ds_touchengine_real PROPERTIES WIN32_EXECUTABLE FALSE)
    dsqt_touchengine_copy_runtime(test_ds_touchengine_real)

    set(_touchengine_fixture "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/test.tox")
    foreach(_backend IN ITEMS d3d11 d3d12 vulkan opengl)
        set(_test_name "test_ds_touchengine_real_${_backend}")
        set(_artifact_dir "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${_backend}")
        add_test(NAME ${_test_name}
            COMMAND test_ds_touchengine_real
                --tox "${_touchengine_fixture}"
                --artifact-dir "${_artifact_dir}"
        )
        dsqt_touchengine_configure_test(${_test_name} test_ds_touchengine_real)
        set_tests_properties(${_test_name} PROPERTIES
            ENVIRONMENT "QSG_RHI_BACKEND=${_backend};QT_QPA_PLATFORM=windows;QT_FORCE_STDERR_LOGGING=1;QT_LOGGING_RULES=dsqt.touchengine.core.debug=true"
            RESOURCE_LOCK touchengine_gpu
            TIMEOUT 360
        )
    endforeach()

    qt_add_executable(test_ds_touchengine_input_real
        real/tst_touchengine_input_real.cpp
    )
    set_source_files_properties(
        real/TextureInputPattern.qml
        PROPERTIES
            QT_RESOURCE_ALIAS TextureInputPattern.qml
    )
    qt_add_qml_module(test_ds_touchengine_input_real
        URI Dsqt.TouchEngine.InputTest
        RESOURCE_PREFIX /qt/qml
        QML_FILES
            real/TextureInputPattern.qml
    )
    target_link_libraries(test_ds_touchengine_input_real PRIVATE
        Dsqt::TouchEngine
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
    )
    set_target_properties(test_ds_touchengine_input_real PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
    dsqt_touchengine_copy_runtime(test_ds_touchengine_input_real)

    set(_touchengine_input_fixture
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/texture_input_roundtrip.tox"
    )
    foreach(_backend IN ITEMS d3d11 d3d12 vulkan opengl)
        set(_test_name "test_ds_touchengine_input_real_${_backend}")
        set(_artifact_dir
            "${CMAKE_CURRENT_BINARY_DIR}/artifacts/input/${_backend}"
        )
        add_test(NAME ${_test_name}
            COMMAND test_ds_touchengine_input_real
                --tox "${_touchengine_input_fixture}"
                --artifact-dir "${_artifact_dir}"
        )
        dsqt_touchengine_configure_test(
            ${_test_name}
            test_ds_touchengine_input_real
        )
        set_tests_properties(${_test_name} PROPERTIES
            ENVIRONMENT "QSG_RHI_BACKEND=${_backend};QT_QPA_PLATFORM=windows;QT_FORCE_STDERR_LOGGING=1;QT_LOGGING_RULES=dsqt.touchengine.core.debug=true"
            RESOURCE_LOCK touchengine_gpu
            TIMEOUT 480
        )
    endforeach()
endif()
