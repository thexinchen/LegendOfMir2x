
file(GLOB COMMON_SRC
    src/*.hpp
    src/*.cpp
    src/*.inc
)

add_library(common ${COMMON_SRC})

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(common PUBLIC ${CMAKE_BINARY_DIR}/config_file)
if(WIN32)
    target_compile_definitions(common PUBLIC _WIN32_WINNT=0x0A00)
endif()

target_link_libraries(common PUBLIC
    argh
    asio::asio
    cereal::cereal
    g3log
    libpopcnt::libpopcnt
    LZ4::lz4_static
    phmap
    PNG::PNG
    sol2::sol2
    tiny-aes-c::tiny-aes-c
    tinyxml2::tinyxml2
    utf8cpp::utf8cpp
    zstd::libzstd_static
)

if(WIN32)
    target_link_libraries(common PUBLIC absl::inlined_vector)
endif()

mir2x_generate_resource(common luamodule_lua "${CMAKE_CURRENT_SOURCE_DIR}/src/luamodule.lua")
