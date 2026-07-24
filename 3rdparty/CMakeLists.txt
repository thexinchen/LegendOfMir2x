
set(CMAKE_FOLDER "3rdparty")

# ============================================================================
# Header-only / INTERFACE libraries
# ============================================================================

# asio
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/asio-1.38.2)
if(WIN32)
    target_compile_definitions(asio INTERFACE _WIN32_WINNT=0x0A00)
    target_link_libraries(asio INTERFACE ws2_32 wsock32)
endif()

# cereal (include <cereal/...> -> 3rdparty/cereal/)
add_library(cereal INTERFACE)
target_include_directories(cereal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# sol2 (include <sol/sol.hpp> -> 3rdparty/sol/)
add_library(sol2 INTERFACE)
target_include_directories(sol2 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sol2 INTERFACE lua)

# argh
add_library(argh INTERFACE)
target_include_directories(argh INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/argh)

# libpopcnt
add_library(libpopcnt INTERFACE)
target_include_directories(libpopcnt INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/libpopcnt)

# parallel_hashmap (include "parallel_hashmap/phmap.h")
add_library(parallel_hashmap INTERFACE)
target_include_directories(parallel_hashmap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/parallel_hashmap)

# utfcpp (include <utf8.h>)
add_library(utfcpp INTERFACE)
target_include_directories(utfcpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/utfcpp-4.1.1)

# stb (include <stb_image.h>)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# ============================================================================
# Compiled libraries
# ============================================================================

# ----------------------------------------------------------------------------
# g3log (needs generated_definitions.hpp)
# ----------------------------------------------------------------------------
set(G3LOG_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/g3log_gen)
file(MAKE_DIRECTORY ${G3LOG_GEN_DIR}/g3log)
file(WRITE ${G3LOG_GEN_DIR}/g3log/generated_definitions.hpp
"// AUTO GENERATED MACRO DEFINITIONS FOR G3LOG\n\
#pragma once\n\
#define G3LOG_DEBUG DEBUG\n")

file(GLOB G3LOG_SRCS g3log/src/*.cpp)
if(WIN32)
    list(REMOVE_ITEM G3LOG_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/g3log/src/crashhandler_unix.cpp)
endif()
add_library(g3log STATIC ${G3LOG_SRCS})
target_include_directories(g3log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${G3LOG_GEN_DIR})
if(WIN32)
    target_link_libraries(g3log PUBLIC dbghelp)
endif()

# ----------------------------------------------------------------------------
# tinyxml2
# ----------------------------------------------------------------------------
add_library(tinyxml2 STATIC tinyxml2.cpp)
target_include_directories(tinyxml2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ----------------------------------------------------------------------------
# tiny_aes
# ----------------------------------------------------------------------------
add_library(tiny_aes STATIC tiny_aes/aes.c)
target_include_directories(tiny_aes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tiny_aes)

# ----------------------------------------------------------------------------
# miniaudio
# ----------------------------------------------------------------------------
add_library(miniaudio STATIC miniaudio/miniaudio.c)
target_include_directories(miniaudio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio)
if(WIN32)
    target_link_libraries(miniaudio PUBLIC ole32 uuid)
endif()

# ----------------------------------------------------------------------------
# sqlite3
# ----------------------------------------------------------------------------
add_library(sqlite3 STATIC sqlite3/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3)

# ----------------------------------------------------------------------------
# SQLiteCpp (include <SQLiteCpp/SQLiteCpp.h> -> 3rdparty/SQLiteCpp/)
# ----------------------------------------------------------------------------
file(GLOB SQLITECPP_SRCS SQLiteCpp/src/*.cpp)
add_library(sqlitecpp STATIC ${SQLITECPP_SRCS})
target_link_libraries(sqlitecpp PUBLIC sqlite3)
target_include_directories(sqlitecpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ----------------------------------------------------------------------------
# lua (exclude lua.c and luac.c which contain main())
# ----------------------------------------------------------------------------
file(GLOB LUA_SRCS lua-5.5.0/*.c)
list(REMOVE_ITEM LUA_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/lua-5.5.0/lua.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lua-5.5.0/luac.c
)
add_library(lua STATIC ${LUA_SRCS})
target_include_directories(lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lua-5.5.0)

# ----------------------------------------------------------------------------
# zlib
# ----------------------------------------------------------------------------
file(GLOB ZLIB_SRCS zlib/*.c)
add_library(zlib STATIC ${ZLIB_SRCS})
target_include_directories(zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/zlib)

# ----------------------------------------------------------------------------
# libpng (needs pnglibconf.h generated from prebuilt)
# ----------------------------------------------------------------------------
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libpng_gen)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/libpng/scripts/pnglibconf.h.prebuilt
    ${CMAKE_CURRENT_BINARY_DIR}/libpng_gen/pnglibconf.h
    COPYONLY
)
file(GLOB PNG_SRCS libpng/png*.c)
add_library(png STATIC ${PNG_SRCS})
target_link_libraries(png PUBLIC zlib)
target_include_directories(png PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libpng
    ${CMAKE_CURRENT_BINARY_DIR}/libpng_gen
)

# ----------------------------------------------------------------------------
# lz4
# ----------------------------------------------------------------------------
add_library(lz4 STATIC
    lz4/lz4.c
    lz4/lz4hc.c
    lz4/lz4frame.c
    lz4/xxhash.c
)
target_include_directories(lz4 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lz4)

# ----------------------------------------------------------------------------
# zstd
# ----------------------------------------------------------------------------
file(GLOB ZSTD_COMMON_SRCS    zstd/common/*.c)
file(GLOB ZSTD_COMPRESS_SRCS  zstd/compress/*.c)
file(GLOB ZSTD_DECOMPRESS_SRCS zstd/decompress/*.c)
file(GLOB ZSTD_DICT_SRCS      zstd/dictBuilder/*.c)
add_library(zstd STATIC
    ${ZSTD_COMMON_SRCS}
    ${ZSTD_COMPRESS_SRCS}
    ${ZSTD_DECOMPRESS_SRCS}
    ${ZSTD_DICT_SRCS}
)
target_include_directories(zstd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/zstd)
target_include_directories(zstd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common)
if(MSVC)
    target_compile_definitions(zstd PRIVATE ZSTD_DISABLE_ASM)
endif()

# ----------------------------------------------------------------------------
# pinyin stub (libpinyin replacement for Windows - Chinese IME disabled)
# ----------------------------------------------------------------------------
add_library(pinyin_stub STATIC pinyin_stub/pinyin_stub.cpp)
target_include_directories(pinyin_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pinyin_stub)

# ----------------------------------------------------------------------------
# glfw
# ----------------------------------------------------------------------------
add_subdirectory(glfw-3.4)
if(WIN32)
    target_link_libraries(glfw3 PUBLIC gdi32 opengl32 user32 shell32 kernel32)
endif()

# ----------------------------------------------------------------------------
# imgui (needs backends include path + link glfw)
# ----------------------------------------------------------------------------
add_subdirectory(imgui-1.92.8)
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui-1.92.8/backends)
target_link_libraries(imgui PUBLIC glfw3)
