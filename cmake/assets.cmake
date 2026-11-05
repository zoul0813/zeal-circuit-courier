# Game-owned archive manifest and loading-buffer limit.
find_package(ZAR REQUIRED CONFIG PATHS "$ENV{ZAR_PATH}/cmake" NO_DEFAULT_PATH)
target_link_libraries(${target_name} PRIVATE zar)
set(tile_chunk_max 4096)
target_compile_definitions(${target_name} PRIVATE PACK_FILE="${target_name}.zar")
set_source_files_properties(src/assets.c PROPERTIES
    COMPILE_DEFINITIONS "TILE_CHUNK_MAX=${tile_chunk_max}")

set(asset_files
    ${map_assets}
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/bgm.zmt"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/win.zmt"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/anim.bin"
)
if(NOT Python3_EXECUTABLE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()
set(tiles_dir ${CMAKE_CURRENT_BINARY_DIR}/${target_name}_tiles)
set(tch ${tiles_dir}/tiles.tch)
set(ztp ${tiles_dir}/tiles.ztp)
add_custom_target(${target_name}_tileset)
tileset_chunks(${target_name}_tileset GIF assets/tileset.gif FORMAT lz MAX ${tile_chunk_max}
    BUILD_DIR ${tiles_dir} TILESET ${tch} PALETTE ${ztp})

set(archive ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_name}.zar)
set(pack_script ${CMAKE_CURRENT_SOURCE_DIR}/tools/zar_pack.py)
set(zar_py $ENV{ZAR_PATH}/zar.py)
add_custom_command(
    OUTPUT ${archive}
    COMMAND ${Python3_EXECUTABLE} ${pack_script} --out ${archive} --zar ${zar_py}
        ${tch} ${ztp} ${asset_files}
    DEPENDS ${pack_script} ${zar_py} ${tch} ${ztp} ${asset_files}
    COMMENT "Packing ${target_name}.zar"
    VERBATIM
)
add_custom_target(${target_name}_zar ALL DEPENDS ${archive})
add_dependencies(${target_name}_zar ${target_name}_tileset ${target_name}_maps)
add_dependencies(${target_name} ${target_name}_zar)
