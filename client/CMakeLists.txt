
file(GLOB_RECURSE CLIENT_SRCS src/*.hpp src/*.cpp)

add_executable(client ${CLIENT_SRCS})
target_link_libraries(client PRIVATE
    common
    imgui
    miniaudio
    stb
    pinyin_stub
)
target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

mir2x_generate_resource(client ivboard_png  "${CMAKE_CURRENT_SOURCE_DIR}/src/ivboard.png")
mir2x_generate_resource(client ivbutton_png "${CMAKE_CURRENT_SOURCE_DIR}/src/ivbutton.png")
mir2x_generate_resource(client monaco_ttf   "${CMAKE_CURRENT_SOURCE_DIR}/src/monaco.ttf")

install(TARGETS client DESTINATION client)
install(CODE [
    execute_process(
        COMMENT "install client resources ..."
        COMMAND "${Python3_EXECUTABLE}" "${MIR2X_INSTALL_RES_SCRIPT}"
            --install-client 
            --ignore-zsdbmaker-error
            "--zsdbmaker=$<TARGET_FILE:zsdbmaker>"
            "--input-dir=${MIR2X_RES_REPO_PATH}"
            "--output-dir=$<TARGET_FILE_DIR:client>/res"
    )
])
