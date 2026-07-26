add_executable(mapeditor
    animationdb.cpp
    editormap.cpp
    imguihelper.cpp
    main.cpp
    mainwindow.cpp
)
if(WIN32)
    set_target_properties(mapeditor PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(mapeditor PRIVATE "/ENTRY:mainCRTStartup")
    endif()
endif()

target_include_directories(mapeditor PRIVATE ${MIR2X_COMMON_SOURCE_DIR})
target_include_directories(mapeditor PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(mapeditor PRIVATE common imgui stb)

install(TARGETS mapeditor DESTINATION tools/mapeditor)
