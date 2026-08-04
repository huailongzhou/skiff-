cmake_minimum_required(VERSION 3.16)
project(skiff LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------- 核心库:后端无关,纯头文件 ----------
add_library(skiff INTERFACE)
target_include_directories(skiff INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---------- 测试(核心逻辑,纯头文件,无需后端) ----------
enable_testing()
add_subdirectory(tests)

# ---------- LVGL 8 后端(可选,-DSKIFF_BACKEND_LVGL=OFF 可关闭) ----------
option(SKIFF_BACKEND_LVGL "Build the LVGL 8 backend" ON)

if(SKIFF_BACKEND_LVGL)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl/CMakeLists.txt")
        message(FATAL_ERROR
            "未找到 third_party/lvgl,请先执行:\n"
            "  git clone --depth 1 --branch release/v8.4 https://github.com/lvgl/lvgl.git third_party/lvgl\n"
            "或者用 -DSKIFF_BACKEND_LVGL=OFF 只编译核心库。")
    endif()

    # FreeType(vendored):LVGL 的 TTF 字体渲染依赖它
    # freetype 2.13.2 声明的 cmake 最低版本过旧,CMake 4.x 需放宽策略下限
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI  TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2   TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG     TRUE CACHE BOOL "" FORCE)
    add_subdirectory(third_party/freetype-2.13.2)

    add_subdirectory(third_party/lvgl)
    # LVGL 8 通过 LV_CONF_PATH 宏指定 lv_conf.h(内部会字符串化,不要加引号)
    target_compile_definitions(lvgl PUBLIC LV_CONF_PATH=${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h)
    target_link_libraries(lvgl PUBLIC freetype)  # lv_freetype.c 需要其头文件和库

    add_library(skiff_lvgl STATIC backends/lvgl/skiff_lvgl.cpp)
    target_include_directories(skiff_lvgl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/backends/lvgl)
    target_link_libraries(skiff_lvgl PUBLIC skiff lvgl)

    add_executable(counter examples/counter.cpp)
    target_link_libraries(counter PRIVATE skiff_lvgl)

    add_executable(freetype_check examples/freetype_check.cpp)
    target_link_libraries(freetype_check PRIVATE skiff_lvgl)

    # ---------- SDL3 宿主(vendored,PC 预览;-DSKIFF_LVGL_SDL3=OFF 可关闭) ----------
    option(SKIFF_LVGL_SDL3 "Build the SDL3 PC preview host" ON)
    if(SKIFF_LVGL_SDL3)
        add_subdirectory(third_party/SDL3-3.2.14)

        add_library(skiff_lvgl_sdl3 STATIC backends/lvgl/skiff_lvgl_sdl3.cpp)
        target_link_libraries(skiff_lvgl_sdl3 PUBLIC skiff_lvgl SDL3::SDL3)

        add_executable(counter_sdl examples/counter_sdl.cpp)
        target_link_libraries(counter_sdl PRIVATE skiff_lvgl_sdl3)

        # pnd_sdl.cpp 现在是平台无关的 UI 定义,没有 main,
        # 平台入口在 platforms/<平台>/ 下。
        if(APPLE)
            add_executable(pnd_mac platforms/mac/mac_platform.cpp)
            target_include_directories(pnd_mac PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
            target_link_libraries(pnd_mac PRIVATE skiff_lvgl_sdl3)
            target_link_libraries(pnd_mac PRIVATE
                "/System/Library/PrivateFrameworks/DisplayServices.framework"
                "-framework CoreGraphics")
        endif()
        if(WIN32)
            add_executable(pnd_win platforms/win/win_platform.cpp)
            target_include_directories(pnd_win PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
            target_link_libraries(pnd_win PRIVATE skiff_lvgl_sdl3 winmm)
        endif()
        if(UNIX AND NOT APPLE)
            add_executable(pnd_linux platforms/linux/linux_platform.cpp)
            target_include_directories(pnd_linux PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
            target_link_libraries(pnd_linux PRIVATE skiff_lvgl_sdl3 mpg123)
        endif()
    endif()
endif()
