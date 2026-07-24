
file(GLOB_RECURSE SERVER_SRC src/*.hpp src/*.cpp)

add_executable(server ${SERVER_SRC})
target_link_libraries(server common sqlitecpp)
target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS server DESTINATION server)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/server/script DESTINATION server)
