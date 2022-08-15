cmake_minimum_required(VERSION 3.14)

project(dsqt LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#Set Install location as a relative location to the source dir -AOF
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/../install/ CACHE PATH "Install Prefix" FORCE)

find_package(Qt6 6.2 COMPONENTS Quick REQUIRED)
add_subdirectory(../modules/dsqml ${CMAKE_CURRENT_BINARY_DIR}/modules/dsqml)
add_subdirectory(../modules/DsSqlQuery ${CMAKE_CURRENT_BINARY_DIR}/modules/DsSqlQuery)
add_custom_target(dsqt-cmake-tools
    SOURCES ../cmake/dsqt.cmake ../cmake/dsqmlimportpath.h.in)


#add_custom_command(TARGET dsqt POST_BUILD
#  COMMAND "${CMAKE_COMMAND}" -E copy
#     "$<TARGET_FILE:dsqt>"
#     "../${CMAKE_BUILD_TYPE}/lib/$<TARGET_FILE_NAME:dsqt>"
#  COMMENT "Copying to output directory")


