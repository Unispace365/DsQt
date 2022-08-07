add_library(dsqt STATIC)

set(DS_LIB_PATH "lib/$<IF:$<CONFIG:Debug>,debug,release>")
set(DSQML_LIB_PATH "${DS_LIB_PATH}/dsqml")

set (DSQT_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/dsqt.h

)

target_sources(dsqt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/dsqt.cpp
    ${DSQT_INCLUDES}
)

set(DSQML_QML_LIB_FILES)


target_link_libraries(dsqt PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(dsqt PRIVATE Qt6::Qml tomlplusplus::tomlplusplus)
target_include_directories(dsqt PRIVATE tomlplusplus::tomlplusplus)
target_compile_definitions(dsqt PRIVATE DSQT_LIBRARY)

#install(TARGETS dsqt dsqt_resources_1 EXPORT DsQtConfig)
#install(TARGETS dsqt dsqt_resources_1
#    CONFIGURATIONS Debug ARCHIVE DESTINATION lib/debug)
#install(TARGETS dsqt dsqt_resources_1
#    CONFIGURATIONS Release ARCHIVE DESTINATION lib/release)


#install(TARGETS dsqt ARCHIVE DESTINATION "${DS_LIB_PATH}")
#install(TARGETS dsqml ARCHIVE DESTINATION "${DSQML_LIB_PATH}")
#install(FILES ${DSQT_INCLUDES} TYPE INCLUDE)
#install(FILES ${DSQML_INCLUDES} TYPE INCLUDE)
#install(FILES ${DSQML_QML_LIB_FILES} DESTINATION "${DSQML_LIB_PATH}")

target_include_directories(dsqt PRIVATE  ${DSQT_INCLUDES} ${DSQML_INCLUDES})



