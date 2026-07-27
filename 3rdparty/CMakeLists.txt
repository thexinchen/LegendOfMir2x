find_package(argh CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)
find_package(g3log CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(PNG CONFIG REQUIRED)
find_package(libpopcnt CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(miniaudio CONFIG REQUIRED)
find_package(phmap CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)
find_package(tiny-aes-c CONFIG REQUIRED)
find_package(tinyxml2 CONFIG REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

# Conan's ImGui package ships the official backend sources as resources.
foreach(_config RELEASE DEBUG RELWITHDEBINFO MINSIZEREL)
    if(DEFINED imgui_PACKAGE_FOLDER_${_config})
        set(IMGUI_BACKEND_DIR "${imgui_PACKAGE_FOLDER_${_config}}/res/bindings")
        break()
    endif()
endforeach()
if(NOT IMGUI_BACKEND_DIR)
    message(FATAL_ERROR "Can't locate the ImGui backend sources in the Conan package")
endif()

set_property(TARGET imgui::imgui APPEND PROPERTY INTERFACE_SOURCES
    "${IMGUI_BACKEND_DIR}/imgui_impl_glfw.cpp"
    "${IMGUI_BACKEND_DIR}/imgui_impl_opengl3.cpp"
)
set_property(TARGET imgui::imgui APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${IMGUI_BACKEND_DIR}"
)
set_property(TARGET imgui::imgui APPEND PROPERTY INTERFACE_LINK_LIBRARIES glfw)
