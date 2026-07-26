
set(IMGUI_SOURCES
    backends/imgui_impl_opengl3_loader.h
    backends/imgui_impl_opengl3.h
    backends/imgui_impl_opengl3.cpp
    backends/imgui_impl_glfw.h
    backends/imgui_impl_glfw.cpp
    imconfig.h
    imgui.h
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_internal.h
    imgui_tables.cpp
    imgui_widgets.cpp
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
)

source_group(imgui FILES ${IMGUI_SOURCES})

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
