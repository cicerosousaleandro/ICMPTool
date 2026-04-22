cmake_minimum_required(VERSION 3.15)
project(ICMPTool VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(WIN32)
    add_compile_options(/W4 /GS)
    add_link_options(/DYNAMICBASE /NXCOMPAT /MANIFESTUAC:NO)
endif()

add_executable(ICMPTool WIN32
    src/main.c
    src/ping.c
    src/tracert.c
    src/scan.c
    resources/app.manifest
)

target_link_libraries(ICMPTool PRIVATE
    iphlpapi
    ws2_32
)

target_include_directories(ICMPTool PRIVATE include)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_link_options(ICMPTool PRIVATE /HIGHENTROPYVA)
endif()