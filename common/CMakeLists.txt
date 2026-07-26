
file(GLOB COMMON_SRC
    src/*.hpp
    src/*.cpp
    src/*.inc
)

add_library(common ${COMMON_SRC})

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(common PUBLIC ${CMAKE_BINARY_DIR}/config_file)

target_link_libraries(common PUBLIC
    argh
    asio
    cereal
    g3log
    libpopcnt
    lz4
    parallel_hashmap
    png
    sol2
    tiny_aes
    tinyxml2
    utfcpp
    zstd
)

mir2x_generate_resource(common luamodule_lua "${CMAKE_CURRENT_SOURCE_DIR}/src/luamodule.lua")
