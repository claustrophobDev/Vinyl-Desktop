cmake_minimum_required(VERSION 3.20)
project(Vinyl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# просить права админа при запуске. для tun интерфейса они нужны обязательно
# на время отладки можно выключить, чтобы не ловить uac на каждый запуск
option(VINYL_REQUIRE_ADMIN "запрашивать права администратора" ON)

# сторонние заголовки лежат отдельно от проекта, чтобы не таскать их в гит
set(VINYL_DEPS_DIR "E:/dev/deps" CACHE PATH "папка со сторонними библиотеками")

if(VINYL_REQUIRE_ADMIN)
    set(VINYL_EXEC_LEVEL "requireAdministrator")
else()
    set(VINYL_EXEC_LEVEL "asInvoker")
endif()

# статический рантайм, чтобы рядом с exe не таскать msvc длл и папка была самодостаточной
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# вся логика без окон, чтобы ее можно было прогнать отдельной консольной утилитой
add_library(vinyl_core STATIC
    src/app/AppModel.cpp
    src/app/Autostart.cpp
    src/app/Install.cpp
    src/app/Log.cpp
    src/app/Paths.cpp
    src/app/Processes.cpp
    src/net/Http.cpp
    src/net/Ping.cpp
    src/vpn/Stats.cpp
    src/core/Text.cpp
    src/core/Flags.cpp
    src/core/ProxyUri.cpp
    src/core/LinkParser.cpp
    src/core/SingBoxConfig.cpp
    src/data/Models.cpp
    src/data/Storage.cpp
    src/ui/Format.cpp
    src/vpn/Core.cpp
)

target_include_directories(vinyl_core PUBLIC
    src
    res
    ${VINYL_DEPS_DIR}/nlohmann-json
)

target_compile_definitions(vinyl_core PUBLIC
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_options(vinyl_core PUBLIC /utf-8 /Zc:__cplusplus /W4 /permissive-)

target_link_libraries(vinyl_core PUBLIC bcrypt shlwapi shell32 ole32 advapi32 winhttp ws2_32)

configure_file(res/app.manifest.in ${CMAKE_CURRENT_BINARY_DIR}/app.manifest @ONLY)

add_executable(vinyl WIN32
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/Painter.cpp
    src/ui/Screens.cpp
    src/ui/Widgets.cpp
    res/app.rc
)

target_link_options(vinyl PRIVATE
    "/MANIFEST:EMBED"
    "/MANIFESTINPUT:${CMAKE_CURRENT_BINARY_DIR}/app.manifest"
)

target_link_libraries(vinyl PRIVATE
    vinyl_core
    d2d1
    dwrite
    windowscodecs
    dwmapi
    shell32
    user32
    gdi32
    ole32
    comctl32
)

# лежит рядом с vinyl.exe и убирает программу из системы
add_executable(vinyl_uninstall WIN32 tools/Uninstall.cpp)
set_target_properties(vinyl_uninstall PROPERTIES OUTPUT_NAME "uninstall")
target_link_libraries(vinyl_uninstall PRIVATE vinyl_core)

# проверялка конфигов, рядом с релизом не нужна
add_executable(vinyl_check tests/CheckConfig.cpp)
target_link_libraries(vinyl_check PRIVATE vinyl_core)
