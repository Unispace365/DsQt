include(FetchContent)
# first we can indicate the documentation build as an option and set it to ON by default
option(BUILD_DOC "Build documentation" OFF)

set(CMAKE_DIR_LOCATION "${CMAKE_CURRENT_LIST_DIR}")

if(BUILD_DOC)

    find_package(
            Python3
            REQUIRED
            COMPONENTS Interpreter
    )

    find_package(
            Doxygen
            REQUIRED
    )

    FetchContent_Declare(
        mcss
        GIT_REPOSITORY https://github.com/mosra/m.css.git
        GIT_TAG ea904db
        #PREFIX ${CMAKE_DIR_LOCATION}/_deps
    )
    FetchContent_populate(mcss)

    #is doxyqml installed
    execute_process(
        COMMAND pip show doxyqml
        RESULT_VARIABLE DOXYQML_EXIT_CODE
        OUTPUT_QUIET
    )

    if (NOT ${DOXYQML_EXIT_CODE} EQUAL 0)
        message(
            FATAL_ERROR
            "The \"doxyqml\" Python3 package is not installed. Please install it using the following command: \"pip3 install doxyqml\"."
        )
    endif()

    #is poxy installed
    #execute_process(
    #        COMMAND pip show poxy
    #        RESULT_VARIABLE EXIT_CODE
    #        OUTPUT_QUIET
    #)

    #if (NOT ${EXIT_CODE} EQUAL 0)
    #    message(
    #            FATAL_ERROR
    #            "The \"poxy\" Python3 package is not installed. Please install it using the following command: \"pip3 install poxy\"."
    #    )
    #endif()
    function(downstream_docs)
        set(options)
        set(oneValueArgs IN_DIR OUT_DIR DOCNAME DESC)
        set(multiValueArgs)
        cmake_parse_arguments(DOWNSTREAM_DOCS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})


        # set input and output files
        set(DOXYGEN_IN ${CMAKE_DIR_LOCATION}/Doxyfile.in)
        set(DOXYGEN_OUT ${DOWNSTREAM_DOCS_IN_DIR}/Doxyfile)
        set(DOXYGEN_MCSS_IN ${CMAKE_DIR_LOCATION}/Doxyfile-mcss.in)
        set(DOXYGEN_MCSS_OUT ${DOWNSTREAM_DOCS_IN_DIR}/Doxyfile-mcss)

        set(DOXYGEN_OUT_DIR ${DOWNSTREAM_DOCS_OUT_DIR} )
        set(DOXYGEN_INPUT_DIRS "${CMAKE_CURRENT_SOURCE_DIR} ${DOWNSTREAM_DOCS_IN_DIR}/pages")
        set(DOXYGEN_NAME ${DOWNSTREAM_DOCS_DOCNAME})
        set(DOXYGEN_DESCRIPTION ${DOWNSTREAM_DOCS_DESC})

        message("in = ${DOXYGEN_IN}")
        message("out = ${DOXYGEN_OUT}")
        # request to configure the file
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        configure_file(${DOXYGEN_MCSS_IN} ${DOXYGEN_MCSS_OUT} @ONLY)

        file(GLOB_RECURSE DS_DOCS LIST_DIRECTORIES true ${DOWNSTREAM_DOCS_IN_DIR})
        add_custom_target(ds-docs  ALL
            COMMAND doxygen ${DOWNSTREAM_DOCS_IN_DIR}/Doxyfile-mcss
            WORKING_DIRECTORY "${mcss_SOURCE_DIR}/documentation"
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
            SOURCES ${DS_DOCS})

    endfunction()
else()
    function(downstream_docs)
    endfunction()
endif() #BUILD_DOC
