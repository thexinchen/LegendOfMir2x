
file(GLOB_RECURSE CLIENT_SRCS src/*.hpp src/*.cpp)

add_executable(client ${CLIENT_SRCS})
target_link_libraries(client common)
target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS client DESTINATION client)
