aux_source_directory(. PKGVIEWER_SRC)
add_executable(pkgviewer ${PKGVIEWER_SRC})
if(WIN32)
    set_target_properties(pkgviewer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(pkgviewer PRIVATE ${MIR2X_COMMON_SOURCE_DIR})
target_include_directories(pkgviewer PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(pkgviewer PRIVATE common imgui_backends stb::stb)

install(TARGETS pkgviewer DESTINATION tools/pkgviewer)
