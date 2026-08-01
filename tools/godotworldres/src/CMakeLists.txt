add_executable(godotworldres main.cpp)
target_link_libraries(godotworldres PRIVATE common imgui::imgui)
target_include_directories(godotworldres PRIVATE ${CMAKE_SOURCE_DIR}/client/src)
set(CMAKE_FOLDER "tools")
