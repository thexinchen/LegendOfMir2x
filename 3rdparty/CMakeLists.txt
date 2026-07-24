
set(CMAKE_FOLDER "3rdparty")

# g3log
file(GLOB G3LOG_SRCS g3log/src/*.cpp)
add_library(g3log STATIC ${G3LOG_SRCS})
target_include_directories(g3log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# tinyxml2
add_library(tinyxml2 STATIC tinyxml2.cpp)

# tiny_aes
add_library(tiny_aes STATIC tiny_aes/aes.c)
target_include_directories(tiny_aes PUBLIC tiny_aes)

#miniaudio
add_library(miniaudio STATIC miniaudio/miniaudio.c)
target_include_directories(miniaudio PUBLIC miniaudio)

# sqlite3
add_library(sqlite3 STATIC sqlite3/sqlite3.c)
target_include_directories(sqlite3 PUBLIC sqlite3)

# sqlitecpp
file(GLOB SQLITECPP_SRCS SQLiteCpp/*.h SQLiteCpp/src/*.cpp)
add_library(sqlitecpp STATIC ${SQLITECPP_SRCS})
target_link_libraries(sqlitecpp PUBLIC sqlite3)
target_include_directories(sqlitecpp PUBLIC SQLiteCpp)

# lua
file(GLOB LUA_SRCS lua-5.5.0/*.c)
add_library(lua STATIC ${LUA_SRCS})
target_include_directories(lua PUBLIC lua-5.5.0)

# glfw
add_subdirectory(glfw-3.4)
# imgui
add_subdirectory(imgui-1.92.8)