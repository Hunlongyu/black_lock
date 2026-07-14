# copy_if_absent.cmake —— 仅当目标文件不存在时复制 (不覆盖用户已有编辑)
# 用法: cmake -DSRC=<源> -DDST=<目标> -P copy_if_absent.cmake
if(NOT EXISTS "${DST}")
    configure_file("${SRC}" "${DST}" COPYONLY)
    message(STATUS "已生成默认配置: ${DST}")
else()
    message(STATUS "保留已存在配置 (未覆盖): ${DST}")
endif()
