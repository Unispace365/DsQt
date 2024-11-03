#function to define imported targets
function(define_imported_target library libraryplugin headers)
    qt_add_library(DsqtBase STATIC IMPORTED)
    set_target_properties(DsqtBase
        PROPERTIES
        IMPORTED_LOCATION ${library}
        #INTERFACE_INCLUDE_DIRECTORIES ${headers}
    )

    qt_add_library(Dsqtplugin STATIC IMPORTED)
    set_target_properties(Dsqtplugin
        PROPERTIES
        IMPORTED_LOCATION ${libraryplugin}
        INTERFACE_INCLUDE_DIRECTORIES ${headers}
    )

    qt_add_library(Dsqt INTERFACE IMPORTED)
    set_property(TARGET Dsqt PROPERTY
        INTERFACE_LINK_LIBRARIES DsqtBase Dsqtplugin
    )

    set(Dsqt_FOUND 1 CACHE INTERNAL "Dsqt Found" FORCE)
    set(Dsqt_LIBRARIES ${library} CACHE STRING "Path to Dsqt library" FORCE)
    set(Dsqt_PLUGINLIBRARIES ${libraryplugin} CACHE STRING "Path to Dsqt PLUGIN library" FORCE)
    set(Dsqt_INCLUDES ${headers} CACHE STRING "Path to Dsqt headers" FORCE)
    mark_as_advanced(FORCE Dsqt_LIBRARIES)
    mark_as_advanced(FORCE Dsqt_PLUGINLIBRARIES)
    mark_as_advanced(FORCE Dsqt_INCLUDES)
endfunction()

#Have they already been found?
if(Dsqt_LIBRARIES AND Dsqt_PLUGINLIBRARIES AND Dsqt_INCLUDES)
    define_imported_target(${Dsqt_LIBRARIES} ${Dsqt_PLUGINLIBRARIES} "${Dsqt_INCLUDES}")
    return()
endif()

file(TO_CMAKE_PATH "$ENV{DS_QT_PLATFORM_100}" _Dsqt_DIR)
find_library(Dsqt_LIBRARY_PATH
    NAMES
    Dsqt

    PATHS
    @CMAKE_BINARY_DIR@
    ${_Dsqt_DIR}/modules/dsqtx/build/debug
)

find_library(Dsqt_PLUGINLIBRARY_PATH
    NAMES
    Dsqtplugin

    PATHS
    @CMAKE_BINARY_DIR@/Dsqt
    ${_Dsqt_DIR}/modules/dsqtx/build/debug/Dsqt
)

find_path(Dsqt_HEADER_PATH
    NAMES
    core/dsqmlapplicationengine.h

    PATHS
    @CMAKE_SOURCE_DIR@
     ${_Dsqt_DIR}/modules/dsqtx/
 )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Dsqt DEFAULT_MSG Dsqt_LIBRARY_PATH Dsqt_PLUGINLIBRARY_PATH Dsqt_HEADER_PATH)
if(Dsqt_FOUND)
    define_imported_target(
        "${Dsqt_LIBRARY_PATH}"
        "${Dsqt_PLUGINLIBRARY_PATH}"
        "${Dsqt_HEADER_PATH};${Dsqt_HEADER_PATH}/core;${Dsqt_HEADER_PATH}/model;${Dsqt_HEADER_PATH}/network;${Dsqt_HEADER_PATH}/qml;${Dsqt_HEADER_PATH}/settings;${Dsqt_HEADER_PATH}/ui;${Dsqt_HEADER_PATH}/utility;${Dsqt_HEADER_PATH}/bridge"
    )
elseif(Dsqt_FIND_REQUIRED)
    message(FATAL_ERROR "Required Dsqt library not found")
endif()


