set(glfw_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# glfw srcs
set(glfw_SOURCES internal.h platform.h mappings.h
    context.c init.c input.c monitor.c platform.c vulkan.c window.c 
    egl_context.c osmesa_context.c null_platform.h null_joystick.h
    null_init.c null_monitor.c null_window.c null_joystick.c
)

if (APPLE)
    add_compile_definitions(_GLFW_COCOA)
    list(APPEND glfw_SOURCES cocoa_time.h cocoa_time.c posix_thread.h posix_module.c posix_thread.c  
                cocoa_platform.h cocoa_joystick.h cocoa_init.m cocoa_joystick.m cocoa_monitor.m cocoa_window.m nsgl_context.m)
elseif (WIN32)
    add_compile_definitions(_GLFW_WIN32)
    list(APPEND glfw_SOURCES win32_time.h win32_thread.h win32_module.c win32_time.c win32_thread.c
                win32_platform.h win32_joystick.h win32_init.c win32_joystick.c win32_monitor.c win32_window.c wgl_context.c)
else()
    add_compile_definitions(_GLFW_X11)
    list(APPEND glfw_SOURCES posix_time.h posix_thread.h posix_module.c posix_time.c posix_thread.c
                x11_platform.h x11_init.c x11_monitor.c x11_window.c xkb_unicode.h xkb_unicode.c glx_context.c
                linux_joystick.h linux_joystick.c posix_poll.h posix_poll.c
    )
endif()

set(GLFW_SOURCES )
foreach(_source ${glfw_SOURCES})
    list(APPEND GLFW_SOURCES ${glfw_PATH}/src/${_source})
endforeach()

list(APPEND GLFW_SOURCES 
    ${glfw_PATH}/include/glad/gl.h
    ${glfw_PATH}/include/GLFW/glfw3.h
    ${glfw_PATH}/include/GLFW/glfw3native.h
)

source_group(glfw FILES ${GLFW_SOURCES})

add_library(glfw3 STATIC ${GLFW_SOURCES})
target_include_directories(glfw3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
