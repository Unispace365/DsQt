include(FetchContent)
get_filename_component(DIR_OF_DSQT_CMAKE_PARENT ".." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
message("DIR_OF_DSQT_CMAKE_PARENT: ${DIR_OF_DSQT_CMAKE_PARENT} :")
set(CMAKE_DIR_LOCATION "${CMAKE_CURRENT_LIST_DIR}")

##Feth Tomlplusplus
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.2.0
    #PREFIX ${CMAKE_DIR_LOCATION}/_deps
)
FetchContent_MakeAvailable(tomlplusplus)


##Fetch GLM from the g-truc glm repo.
##https://glm.g-truc.net/0.9.9/index.html
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    #PREFIX ${CMAKE_DIR_LOCATION}/_deps
)

FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
  FetchContent_Populate(glm)
endif()

function(downstream_modules)
    set(options)
    set(oneValueArgs TARGET)
    set(multiValueArgs MODULES)
    cmake_parse_arguments(DOWNSTREAM_MOD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})



    set(DSQT_PROJECT_PATH "${DIR_OF_DSQT_CMAKE_PARENT}")
    message("DSQT_PROJECT_PATH: ${DSQT_PROJECT_PATH} :")
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        set(DSQT_BUILD_PATH "${DSQT_PROJECT_PATH}/build/build-debug/")
    else()
        set(DSQT_BUILD_PATH "${DSQT_PROJECT_PATH}/build/build-release/")
    endif()

    configure_file("${CMAKE_DIR_LOCATION}/dsqmlimportpath.h.in" "${CMAKE_DIR_LOCATION}/dsqmlimportpath.h")
    target_include_directories(${DOWNSTREAM_MOD_TARGET} PRIVATE "${CMAKE_DIR_LOCATION}")
    set(DSQT_LIBS dsqt dsqml)
    list(APPEND DS_IMPORT_PATH ${QML2_IMPORT_PATH} "${DSQT_BUILD_PATH}")
    list(REMOVE_DUPLICATES DS_IMPORT_PATH)
    set(QML2_IMPORT_PATH "${DS_IMPORT_PATH}" CACHE STRING "Qt Creator 4.1 extra qml import paths" FORCE)

    foreach(MOD IN LISTS DOWNSTREAM_MOD_MODULES)
        #execute_process(COMMAND ${conan_program} install .
        #                    WORKING_DIRECTORY "${DSQT_PROJECT_PATH}/modules/${MOD}"
        #                    RESULT_VARIABLE CONAN_MOD_RESULT)
        #                message(${CONAN_MOD_RESULT})
        if(NOT TARGET ${MOD})
            add_subdirectory("${DSQT_PROJECT_PATH}/modules/${MOD}" "${DSQT_BUILD_PATH}/${MOD}")
        endif()
        target_link_libraries(${DOWNSTREAM_MOD_TARGET}
            PUBLIC ${MOD} "${MOD}plugin")
        target_include_directories(${DOWNSTREAM_MOD_TARGET} PUBLIC ${MOD})
    endforeach() #mod in LISTS DOWNSTREAM_MOD_MODULES

    ##connect GLM to the project
    add_library(glm INTERFACE)
    message("GLM: ${glm_SOURCE_DIR} ;")
    target_include_directories(glm INTERFACE ${glm_SOURCE_DIR})
    target_include_directories(${DOWNSTREAM_MOD_TARGET} PUBLIC ${glm_SOURCE_DIR})
    target_link_libraries(${DOWNSTREAM_MOD_TARGET} PUBLIC glm)

    ##connect toml++
    target_include_directories(${DOWNSTREAM_MOD_TARGET} PUBLIC tomlplusplus::tomlplusplus)
endfunction()
