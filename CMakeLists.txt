cmake_minimum_required(VERSION 3.16)

project(stremio VERSION "5.0.20")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(DEBUG_LOG "Allow debug logs" ON)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "VCPKG toolchain file")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MPV_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/libmpv/x86_64/include")
    set(MPV_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/deps/libmpv/x86_64/mpv.lib")
    set(MPV_DLL "${CMAKE_CURRENT_SOURCE_DIR}/deps/libmpv/x86_64/libmpv-2.dll")
else()
    # 32-bit architecture
endif()

include_directories(${MPV_INCLUDE_DIR})

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-webview2 CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED) 

add_subdirectory(deps/discord-rpc-src)


set(SOURCES
        src/main.cpp
        stremio.rc
        src/core/globals.h
        src/core/globals.cpp
        src/utils/helpers.h
        src/utils/helpers.cpp
        src/utils/config.h
        src/utils/config.cpp
        src/utils/crashlog.h
        src/utils/crashlog.cpp
        src/mpv/player.h
        src/mpv/player.cpp
        src/node/server.cpp
        src/node/server.h
        src/tray/tray.h
        src/tray/tray.cpp
        src/ui/splash.h
        src/ui/splash.cpp
        src/updater/updater.cpp
        src/updater/updater.h
        src/webview/webview.h
        src/webview/webview.cpp
        src/ui/mainwindow.h
        src/ui/mainwindow.cpp
        src/resource.h
        src/utils/extensions.cpp
        src/utils/extensions.h
        src/utils/discord.cpp
        src/utils/discord.h
        src/logger/logger.cpp
        src/logger/logger.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} PRIVATE
        gdiplus.lib
        dwmapi.lib
        shcore.lib
        Msimg32.lib
        winhttp.lib
        shlwapi.lib
        ws2_32.lib 
        nlohmann_json::nlohmann_json
        unofficial::webview2::webview2
        OpenSSL::SSL
        OpenSSL::Crypto
        CURL::libcurl
        WIL::WIL
        ${MPV_LIBRARY}
        discord-rpc-static 
)

if(DEBUG_LOG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_LOG)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MPV_DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")

set(NODE_RUNTIME_SRC "${CMAKE_CURRENT_SOURCE_DIR}/deps/stremio-runtime.exe")
set(NODE_SCRIPT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/deps/server.js")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NODE_RUNTIME_SRC}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NODE_SCRIPT_SRC}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copying Node.js server files to output directory...")