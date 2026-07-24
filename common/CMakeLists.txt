
file(GLOB COMMON_SRC
	src/*.hpp
	src/*.cpp
	src/*.inc
)

add_library(common ${COMMON_SRC})

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(common PUBLIC ${CMAKE_BINARY_DIR}/config_file)
