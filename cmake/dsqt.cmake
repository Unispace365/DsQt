get_filename_component(DIR_OF_DSQT_CMAKE_PARENT ".." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
message("DIR_OF_DSQT_CMAKE_PARENT: ${DIR_OF_DSQT_CMAKE_PARENT} :")
function(downstream_modules)
    set(options)
    set(oneValueArgs TARGET)
    set(multiValueArgs MODULES)
    cmake_parse_arguments(DOWNSTREAM_MOD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})



    set(DSQT_PROJECT_PATH "${DIR_OF_DSQT_CMAKE_PARENT}")
    message("DSQT_PROJECT_PATH: ${DSQT_PROJECT_PATH} :")
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        set(DSQT_BUILD_PATH "${DSQT_PROJECT_PATH}/build-debug/")
    else()
        set(DSQT_BUILD_PATH "${DSQT_PROJECT_PATH}/build-release/")
    endif()


    set(DSQT_LIBS dsqt dsqml)
    list(APPEND DS_IMPORT_PATH ${QML2_IMPORT_PATH} "${DSQT_BUILD_PATH}")
    list(REMOVE_DUPLICATES DS_IMPORT_PATH)
    set(QML2_IMPORT_PATH "${DS_IMPORT_PATH}" CACHE STRING "Qt Creator 4.1 extra qml import paths" FORCE)

    foreach(MOD in LISTS DOWNSTREAM_MOD_MODULES)
        add_subdirectory("${DSQT_PROJECT_PATH}/${MOD}" "${DSQT_BUILD_PATH}/${MOD}")
        target_link_libraries(${DOWNSTREAM_MOD_TARGET}
            PRIVATE ${MOD} "${MOD}plugin")
        target_include_directories(${DOWNSTREAM_MOD_TARGET} PRIVATE ${MOD})
    endforeach() #mod in LISTS DOWNSTREAM_MOD_MODULES
endfunction()
