# TermchartSingleHeader.cmake —— 单头文件生成
#
# 规则（见 tools/gen_single_header.py 顶部）：去重、按层拓扑排序、匿名命名空间隔离。
# 阶段 7 才真正启用（ROADMAP R8：文件数未收敛就生成会导致产物不稳定）；
# 这里先把构建接线做好，脚本缺失时目标不存在而不报错。

find_package(Python3 COMPONENTS Interpreter QUIET)

set(_single_header_script "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_single_header.py")

if(Python3_Interpreter_FOUND AND EXISTS "${_single_header_script}")
    add_custom_target(single_header
        COMMAND ${Python3_EXECUTABLE} "${_single_header_script}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "生成 single_include/termchart.hpp"
        VERBATIM)
    message(STATUS "termchart: 单头生成目标已启用")
else()
    message(STATUS "termchart: 单头生成脚本尚未落地（阶段 7），跳过 single_header 目标")
endif()
