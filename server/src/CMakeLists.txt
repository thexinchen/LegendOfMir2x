# Stage 1a of the FLTK -> ImGui migration: the fluid-generated FLTK windows
# are out of the build (legacy_gui/ keeps their support sources on disk);
# the server monitor GUI now lives in imguiui/.
aux_source_directory(. SERVER_SRC)
aux_source_directory(imguiui SERVER_IMGUIUI_SRC)
add_executable(server ${SERVER_SRC} ${SERVER_IMGUIUI_SRC})
add_dependencies(server zsdbmaker mir2x_resources)

target_include_directories(server PRIVATE ${MIR2X_COMMON_SOURCE_DIR})
target_include_directories(server PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(server common mir2x::sqlitecpp)

install(TARGETS server DESTINATION server)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/server/script DESTINATION server)
install(CODE "execute_process(
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/installres/src/install_res.py
            --install-server
            --ignore-zsdbmaker-error
            --zsdbmaker=$<TARGET_FILE:zsdbmaker>
            --input-dir=${MIR2X_RES_REPO_PATH}
            --output-dir=${CMAKE_INSTALL_PREFIX}/server
    COMMAND_ERROR_IS_FATAL ANY
)")
