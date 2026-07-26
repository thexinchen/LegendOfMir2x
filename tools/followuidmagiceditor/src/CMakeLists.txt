aux_source_directory(. FOLLOWUIDMAGICEDITOR_SRC)
add_executable(followuidmagiceditor ${FOLLOWUIDMAGICEDITOR_SRC})
if(WIN32)
    set_target_properties(followuidmagiceditor PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(followuidmagiceditor PRIVATE "/ENTRY:wmainCRTStartup")
    endif()
endif()

target_include_directories(followuidmagiceditor PRIVATE ${MIR2X_COMMON_SOURCE_DIR})
target_include_directories(followuidmagiceditor PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(followuidmagiceditor PRIVATE common imgui stb)

install(TARGETS followuidmagiceditor DESTINATION tools/followuidmagiceditor)
