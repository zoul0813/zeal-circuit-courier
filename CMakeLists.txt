cmake_minimum_required(VERSION 3.16)

##
# Project specific
##
set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets" CACHE PATH "Directory containing assets")
option(REPLAY "Build with scripted input from src/replay.h" OFF)
if(REPLAY)
    set(target_name courier_replay)
else()
    set(target_name courier)
endif()

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BIN_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "Where the .bin goes")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)

zos_use(zvb)
include($ENV{ZGDK_PATH}/cmake/zgdk_init.cmake)

project(circuit_courier C ASM)

# Define your output name and sources directly here.
add_executable(${target_name}
    src/controls.c
    src/main.c
    src/room.c
    src/assets.c
    src/game.c
    src/hud.c
    src/audio.c
)
# keeps code small: code + data + the music patterns must fit the 48 KB program space
target_compile_options(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:C>:--opt-code-size>)
if(REPLAY)
    target_compile_definitions(${target_name} PRIVATE REPLAY=1)
endif()

##
# Feature toggles (template.mk equivalents)
##
# Add compile definitions as needed, for example:
# target_compile_definitions(${target_name} PRIVATE EMULATOR=1)
# target_compile_definitions(${target_name} PRIVATE FRAMELOCK=1)
# target_compile_definitions(${target_name} PRIVATE DEBUG=1)

##
# Dependencies
##
find_package(ZVB_SDK REQUIRED CONFIG PATHS "$ENV{ZVB_SDK_PATH}/cmake" NO_DEFAULT_PATH)
find_package(ZGDK REQUIRED CONFIG PATHS "$ENV{ZGDK_PATH}/cmake" NO_DEFAULT_PATH)

target_link_libraries(${target_name} PUBLIC zgdk zvb_gfx zvb_sound)
# Add optional libraries as needed, for example:
# target_link_libraries(${target_name} PUBLIC zvb_sound)
# target_link_libraries(${target_name} PUBLIC zvb_crc)

# Native map bytes and spawn metadata are generated from assets/**/*.tmx.
# Tileset GIF and music remain tracked assets; map exports stay in the build tree.
include(cmake/maps.cmake)
include(cmake/assets.cmake)

# Generate <bin>.bin output in the build directory.
zos_add_outputs(${target_name})

set_property(TARGET ${target_name} APPEND PROPERTY LINK_DEPENDS "$ENV{ZGDK_PATH}/lib/zgdk.lib")
