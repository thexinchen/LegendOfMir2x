cmake_minimum_required(VERSION 4.3.1)

project(mir2x LANGUAGES C CXX)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(${PROJECT_NAME}_MAJOR_VERSION 0)
set(${PROJECT_NAME}_MINOR_VERSION 1)
set(${PROJECT_NAME}_PATCH_VERSION 0)
set(${PROJECT_NAME}_VERSION ${${PROJECT_NAME}_MAJOR_VERSION}.${${PROJECT_NAME}_MINOR_VERSION}.${${PROJECT_NAME}_PATCH_VERSION})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/")
set(CMAKE_DEBUG_POSTFIX "d")

if(MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/permissive->")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/bigobj>")
    add_compile_options(/MP)  # multi-process compilation
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
    set(CMAKE_CXX_STANDARD 26)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_SKIP_RPATH TRUE)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

# ----------------------------------------------------------------------------
# Build signature
# ----------------------------------------------------------------------------
set(MIR2X_COMMON_SOURCE_DIR ${CMAKE_SOURCE_DIR}/common/src)

if(NOT DEFINED MIR2X_BUILD_SIGNATURE)
    string(TIMESTAMP NOW_TIMESTAMP "%Y%m%d%H%M%S")
    set(MIR2X_BUILD_SIGNATURE "VENGINEERING-${NOW_TIMESTAMP}")
endif()
message(STATUS "Build signature: ${MIR2X_BUILD_SIGNATURE}")

# ----------------------------------------------------------------------------
# Generate log file list for logProfiler()
# ----------------------------------------------------------------------------
if(DEFINED MIR2X_LOG_FILELIST)
    message(FATAL_ERROR "Don't define MIR2X_LOG_FILELIST, cmake automatically collect the file list")
else()
    file(GLOB_RECURSE MIR2X_LOG_SRC_FILELIST
        ${CMAKE_SOURCE_DIR}/common/src/*.[hc]pp
        ${CMAKE_SOURCE_DIR}/client/src/*.[hc]pp
        ${CMAKE_SOURCE_DIR}/server/src/*.[hc]pp
    )

    foreach(LOG_SRC ${MIR2X_LOG_SRC_FILELIST})
        string(REGEX REPLACE "^${CMAKE_SOURCE_DIR}/" "" LOG_SRC_BODY ${LOG_SRC})
        string(APPEND MIR2X_LOG_FILELIST "R\"DELIM(${LOG_SRC_BODY})DELIM\",\n")
    endforeach()
endif()

if(WIN32 AND MSVC)
    string(REPLACE "/" "\\" MIR2X_LOG_FILELIST ${MIR2X_LOG_FILELIST})
endif()

configure_file(${MIR2X_COMMON_SOURCE_DIR}/buildconfig.hpp.in ${CMAKE_BINARY_DIR}/config_file/buildconfig.hpp)
configure_file(${MIR2X_COMMON_SOURCE_DIR}/logprof.hpp.in     ${CMAKE_BINARY_DIR}/config_file/logprof.hpp)

include("${CMAKE_SOURCE_DIR}/cmake/Mir2xResource.cmake")

# ----------------------------------------------------------------------------
# Subdirectories
# ----------------------------------------------------------------------------
add_subdirectory(3rdparty)
add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)

# ----------------------------------------------------------------------------
# Install / CPack
# ----------------------------------------------------------------------------
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ./)
set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_GENERATOR "ZIP")
include(CPack)
