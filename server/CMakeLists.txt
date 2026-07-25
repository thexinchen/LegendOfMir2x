
file(GLOB_RECURSE SERVER_SRC src/*.hpp src/*.cpp)

add_executable(server ${SERVER_SRC})
target_link_libraries(server PRIVATE common sqlitecpp imgui)
target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty)

mir2x_generate_resource(server charobject_lua               "${CMAKE_CURRENT_SOURCE_DIR}/src/charobject.lua")
mir2x_generate_resource(server npchar_lua                   "${CMAKE_CURRENT_SOURCE_DIR}/src/npchar.lua")
mir2x_generate_resource(server player_lua                   "${CMAKE_CURRENT_SOURCE_DIR}/src/player.lua")
mir2x_generate_resource(server quest_lua                    "${CMAKE_CURRENT_SOURCE_DIR}/src/quest.lua")
mir2x_generate_resource(server server_lua                   "${CMAKE_CURRENT_SOURCE_DIR}/src/server.lua")
mir2x_generate_resource(server serverluacoroutinerunner_lua "${CMAKE_CURRENT_SOURCE_DIR}/src/serverluacoroutinerunner.lua")
mir2x_generate_resource(server serverluamodule_lua          "${CMAKE_CURRENT_SOURCE_DIR}/src/serverluamodule.lua")
mir2x_generate_resource(server servermap_lua                "${CMAKE_CURRENT_SOURCE_DIR}/src/servermap.lua")
mir2x_generate_resource(server serverobject_lua             "${CMAKE_CURRENT_SOURCE_DIR}/src/serverobject.lua")

install(TARGETS server DESTINATION server)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/server/script DESTINATION server)
