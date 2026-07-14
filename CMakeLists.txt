cmake_minimum_required(VERSION 3.21)

# 让 MSVC_RUNTIME_LIBRARY 属性生效
cmake_policy(SET CMP0091 NEW)

project(BlackLock
    VERSION 1.1.0
    DESCRIPTION "无界面黑屏锁屏工具 - Windows 10/11 x64"
    LANGUAGES C
)

# ---- 版本号: 默认取 project() 版本; CI 可用 -DBL_VERSION_OVERRIDE=1.2.3 (来自 git tag) 覆盖 ----
set(BL_VERSION_OVERRIDE "" CACHE STRING "覆盖版本号 (通常由 CI 从 git tag 传入, 不含 v 前缀)")
if(BL_VERSION_OVERRIDE)
    set(BL_VER "${BL_VERSION_OVERRIDE}")
else()
    set(BL_VER "${PROJECT_VERSION}")
endif()
string(REPLACE "." ";" _bl_vparts "${BL_VER}")
list(LENGTH _bl_vparts _bl_vn)
list(GET _bl_vparts 0 BL_VER_MAJOR)
if(_bl_vn GREATER 1)
    list(GET _bl_vparts 1 BL_VER_MINOR)
else()
    set(BL_VER_MINOR 0)
endif()
if(_bl_vn GREATER 2)
    list(GET _bl_vparts 2 BL_VER_PATCH)
else()
    set(BL_VER_PATCH 0)
endif()

# ---- 语言标准: C17 (见开发文档 §7.1) ----
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---- 仅 Windows ----
if(NOT WIN32)
    message(FATAL_ERROR "BlackLock 仅支持 Windows 平台")
endif()

# ---- 仅 MSVC: 本项目只使用 MSVC 编译, 不支持 GCC/Clang/clang-cl ----
if(NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR
        "BlackLock 仅使用 MSVC (cl.exe) 编译, 当前编译器为 '${CMAKE_C_COMPILER_ID}'。\n"
        "请用 MSVC 配置, 例如:  cmake --preset msvc")
endif()

# ---- 默认构建类型: 单配置生成器下未指定时用 MinSizeRel (VS 多配置生成器忽略此项) ----
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "MinSizeRel" CACHE STRING "" FORCE)
endif()

# ============================================================
#  可选: 第三方依赖 (默认零依赖, 见开发文档 §7.3)
#  如需引入 INI 解析库 inih, 取消下面注释:
#
#  include(FetchContent)
#  FetchContent_Declare(inih
#      GIT_REPOSITORY https://github.com/benhoyt/inih.git
#      GIT_TAG        r58)
#  FetchContent_MakeAvailable(inih)
# ============================================================

# ---- 目标: 无控制台窗口的 GUI 子系统程序 ----
add_executable(BlackLock WIN32
    src/main.c
    src/config.c
    src/hotkey.c
    src/lockwin.c
    src/keyhook.c
    src/autostart.c
    src/tray.c
    src/util.c
    src/app.rc
)

target_include_directories(BlackLock PRIVATE src)

set_target_properties(BlackLock PROPERTIES
    OUTPUT_NAME "BlackLock"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/dist")

# ---- 动态 UCRT (Win10+ 自带, 不额外分发文件) ----
set_property(TARGET BlackLock PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# ---- 通用宏 + 版本号 (C 代码与 .rc 资源共用) ----
target_compile_definitions(BlackLock PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00
    BL_VERSION="${BL_VER}"
    BL_VER_MAJOR=${BL_VER_MAJOR}
    BL_VER_MINOR=${BL_VER_MINOR}
    BL_VER_PATCH=${BL_VER_PATCH})

# ---- 编译选项 (体积优化, 见 §7.6) ----
target_compile_options(BlackLock PRIVATE
    /W4 /utf-8 /std:c17
    $<$<CONFIG:Release,MinSizeRel>:/O1>
    $<$<CONFIG:Release,MinSizeRel>:/Os>
    $<$<CONFIG:Release,MinSizeRel>:/Gy>
    $<$<CONFIG:Release,MinSizeRel>:/GS->)

# ---- 链接选项 (去调试信息 + 合并/裁剪) ----
target_link_options(BlackLock PRIVATE
    $<$<CONFIG:Release,MinSizeRel>:/DEBUG:NONE>
    $<$<CONFIG:Release,MinSizeRel>:/OPT:REF>
    $<$<CONFIG:Release,MinSizeRel>:/OPT:ICF>
    $<$<CONFIG:Release,MinSizeRel>:/INCREMENTAL:NO>)

# ---- 仅依赖 Windows 系统库 (无第三方运行时) ----
target_link_libraries(BlackLock PRIVATE
    user32 gdi32 bcrypt shell32 advapi32)

# ---- 编译期在输出目录生成默认配置文件 (便携用; 已存在则不覆盖) ----
# 单一事实来源: src/config.default.ini (同时内嵌为 exe 资源供运行时写 %APPDATA%)
add_custom_command(TARGET BlackLock POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_SOURCE_DIR}/src/config.default.ini
        -DDST=$<TARGET_FILE_DIR:BlackLock>/config.ini
        -P ${CMAKE_SOURCE_DIR}/cmake/copy_if_absent.cmake
    VERBATIM
    COMMENT "生成默认配置文件 dist/config.ini (若不存在)")

# ---- clang-format 便捷目标 (仅格式化, 不参与编译) ----
file(GLOB BL_SOURCES ${CMAKE_SOURCE_DIR}/src/*.c ${CMAKE_SOURCE_DIR}/src/*.h)
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${BL_SOURCES}
        COMMENT "格式化源码" VERBATIM)
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${BL_SOURCES}
        COMMENT "检查格式" VERBATIM)
endif()

install(TARGETS BlackLock RUNTIME DESTINATION .)

message(STATUS "BlackLock ${BL_VER}")
message(STATUS "  Compiler : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Output   : dist/BlackLock.exe")
