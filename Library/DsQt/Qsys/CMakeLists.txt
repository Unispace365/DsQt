find_package(Qt6 6.9 REQUIRED COMPONENTS Core Qml Network WebSockets)
qt_standard_project_setup(REQUIRES 6.9)

add_library(Qsys STATIC)
add_library(Dsqt::Qsys ALIAS Qsys)
qt_add_qml_module(Qsys
    URI Dsqt.Qsys
    VERSION 1.0
    NO_GENERATE_PLUGIN_SOURCE
    NO_PLUGIN_OPTIONAL
    SOURCES qsys/dsQsys.h qsys/dsQsys.cpp
)
target_sources(Qsysplugin PRIVATE qsys/dsQsysPlugin.cpp)
target_link_libraries(Qsys PUBLIC Dsqt::Core Qt6::Qml Qt6::Network Qt6::WebSockets Qsysplugin)
target_include_directories(Qsys PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qsys>
    $<INSTALL_INTERFACE:include/qsys>
)
include(GNUInstallDirs)
install(FILES qsys/dsQsys.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qsys)
