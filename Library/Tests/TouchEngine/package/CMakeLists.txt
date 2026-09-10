cmake_minimum_required(VERSION 3.29)

project(DsqtTouchEnginePackageSmoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DSQT_PACKAGE_SMOKE_CORE_ONLY
    "Only verify that the Core component can be discovered"
    OFF
)

if(DSQT_PACKAGE_SMOKE_CORE_ONLY)
    find_package(Qt6 6.9 REQUIRED COMPONENTS Core)
    find_package(Dsqt REQUIRED COMPONENTS Core)

    if(NOT TARGET Dsqt::Core)
        message(FATAL_ERROR "The installed Dsqt package did not define Dsqt::Core")
    endif()
    if(TARGET Vulkan::Vulkan)
        message(FATAL_ERROR "A Core-only Dsqt lookup unexpectedly resolved Vulkan")
    endif()
    return()
endif()

find_package(Qt6 6.9 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
)
qt_standard_project_setup(REQUIRES 6.9)

find_package(Dsqt REQUIRED COMPONENTS TouchEngine)

qt_add_executable(dsqt_touchengine_package_smoke
    main.cpp
)

qt_add_qml_module(dsqt_touchengine_package_smoke
    URI Dsqt.PackageSmoke
    QML_FILES
        Main.qml
)

target_link_libraries(dsqt_touchengine_package_smoke PRIVATE
    Dsqt::TouchEngine
    Dsqt::TouchEngineplugin
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

dsqt_deploy_runtime_dlls(dsqt_touchengine_package_smoke)

set_target_properties(dsqt_touchengine_package_smoke PROPERTIES
    WIN32_EXECUTABLE FALSE
)
