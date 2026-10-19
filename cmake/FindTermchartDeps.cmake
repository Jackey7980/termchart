# FindTermchartDeps.cmake —— 可选依赖查找（libpng / zlib / utf8proc / stb_image）
#
# 铁律：**零强制依赖**。任何 find_package 失败都不得让 configure 失败，
# 只是把对应 TERMCHART_HAVE_* 置为 OFF，由模块自行降级：
#   * 无 libpng → image/png.hpp 返回 Unsupported，PNG 相关功能不可用；
#   * 无 zlib  → image/png_encode.hpp 同上（zlib 只在这一个文件引入）；
#   * 无 utf8proc → 使用仓库内生成的表（detail/unicode_tables.hpp）即可；
#   * 无 stb_image（默认关）→ JPG/GIF/WebP 解码返回 Unsupported，交由调用方。
#
# stb_image 是 vendored（third_party/stb/），不走 find_package：开启开关即视为可用，
# 头文件由主 CMakeLists 挂 third_party/stb 包含目录 + 编译期定义 TERMCHART_HAVE_STB_IMAGE。

set(TERMCHART_HAVE_LIBPNG OFF)
set(TERMCHART_HAVE_ZLIB OFF)
set(TERMCHART_HAVE_UTF8PROC OFF)
set(TERMCHART_HAVE_STB_IMAGE OFF)

if(TERMCHART_ENABLE_PNG)
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        set(TERMCHART_HAVE_ZLIB ON)
    else()
        message(STATUS "termchart: zlib 未找到，PNG 编码降级为不可用")
    endif()

    find_package(PNG QUIET)
    if(PNG_FOUND)
        set(TERMCHART_HAVE_LIBPNG ON)
    else()
        message(STATUS "termchart: libpng 未找到，PNG 解码降级为不可用")
    endif()
endif()

if(TERMCHART_ENABLE_UTF8PROC)
    find_package(utf8proc QUIET)
    if(utf8proc_FOUND)
        set(TERMCHART_HAVE_UTF8PROC ON)
    else()
        message(STATUS "termchart: utf8proc 未找到，改用内置生成表")
    endif()
endif()

if(TERMCHART_ENABLE_STB_IMAGE)
    # vendored：开启即视为可用；缺失头文件会在编译期暴露（fail-loud，不静默）。
    set(TERMCHART_HAVE_STB_IMAGE ON)
    message(STATUS "termchart: stb_image 已启用（JPEG/GIF/WebP 解码）")
endif()
