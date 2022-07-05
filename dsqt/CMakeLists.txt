cmake_minimum_required(VERSION 3.14)

project(dsqt LANGUAGES CXX)

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/../install/ CACHE PATH "Install Prefix" FORCE)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_DEBUG_POSTFIX d)


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Qml)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Qml)
find_package(tomlplusplus)


add_subdirectory(src)






#add_custom_command(TARGET dsqt POST_BUILD
#  COMMAND "${CMAKE_COMMAND}" -E copy
#     "$<TARGET_FILE:dsqt>"
#     "../${CMAKE_BUILD_TYPE}/lib/$<TARGET_FILE_NAME:dsqt>"
#  COMMENT "Copying to output directory")


