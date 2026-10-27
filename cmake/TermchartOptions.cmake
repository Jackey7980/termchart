# TermchartOptions.cmake —— 模块编译开关与可选依赖链
#
# 两条设计约束（docs/design/modularity.md）：
#   * 零强制依赖：每个外部依赖都有对应开关，**缺失时模块降级而不编译失败**；
#   * 最小构建：TERMCHART_MINIMAL_BUILD 只编 core/util/color/term/render，
#     用于验证"没有上层模块也能跑通链路"。

option(TERMCHART_BUILD_TESTS "构建测试（doctest）" ON)
option(TERMCHART_BUILD_EXAMPLES "构建 examples/" ON)
option(TERMCHART_BUILD_BENCH "构建 bench/（性能基线，零外部依赖）" ON)
option(TERMCHART_BUILD_PYTHON "构建 Python 绑定（pybind11，可选）" OFF)
option(TERMCHART_BUILD_SHARED "构建共享库（默认静态）" OFF)
option(TERMCHART_ENABLE_COVERAGE "构建覆盖率插桩" OFF)
option(TERMCHART_WARNINGS_AS_ERRORS "把警告当错误（CI 用）" OFF)
option(TERMCHART_MINIMAL_BUILD "只构建最小核心" OFF)

# ── 可选依赖链（bền 只经 find_package，不 vendored 进构建图） ────────────
option(TERMCHART_ENABLE_PNG "尝试引入 libpng/zlib（PNG 解码 / 编码）" OFF)
option(TERMCHART_ENABLE_UTF8PROC "尝试引入 utf8proc（完整 Unicode 属性）" OFF)
option(TERMCHART_ENABLE_STB_IMAGE "引入 vendored stb_image（JPEG/GIF/WebP 解码）；默认关，缺失即 Unsupported" OFF)
option(TERMCHART_ENABLE_FUZZING "构建 fuzz 目标（需 clang 或 libFuzzer）" OFF)

# 原生 GUI 后端插件（可选，默认关）：Win32 真实窗口后端需编译型平台代码 + 链
# user32/gdi32，违背核心"零强制依赖、header-only"，故独立插件、仅 Windows、默认不编。
option(TERMCHART_BUILD_GUI_WIN32 "构建原生 Win32 GUI 后端插件（可选，仅 Windows；默认关）" OFF)

# 原生 Qt GUI 后端插件（可选，全平台；默认关）。显式开启但找不到 Qt5/Qt6
# Widgets 时**配置期报错**（fail-loud，不静默跳过）。
option(TERMCHART_BUILD_GUI_QT "构建原生 Qt GUI 后端插件（可选，需 Qt5/Qt6 Widgets；默认关）" OFF)

# 原生 Tk GUI 后端插件（可选，全平台；默认关）。显式开启但找不到 Tcl/Tk 开发库
# 时**配置期报错**（fail-loud，不静默跳过）。
option(TERMCHART_BUILD_GUI_TK "构建原生 Tk GUI 后端插件（可选，需 Tcl/Tk 开发库；默认关）" OFF)

# Emscripten WASM 目标（可选，需 emsdk；默认关、不进 CI）。开启后，配合 Emscripten
# 工具链会编出一个把 image::Bitmap 推到网页 <canvas> 的模块（src/wasm/termchart_wasm.cpp）。
# 普通桌面/CI 构建里 EMSCRIPTEN 为假，此开关无效、不产生任何目标。
option(TERMCHART_BUILD_WASM "构建 Emscripten WASM 目标（需 emsdk；默认关）" OFF)

# ── 模块开关：关掉即裁掉对应 src 目录 ─────────────────────────────────────
#   上层模块的缺失不影响下层编译（分层规则保证单向依赖）。
set(TERMCHART_MODULES
    core util color term render api
    scale scene document canvas layout
    chart graph backend interaction
    dsl widget app plugin autoviz
    CACHE STRING "参与构建的模块（include/termchart 下的一级目录名）")

if(TERMCHART_MINIMAL_BUILD)
    set(TERMCHART_MODULES core util color term render api CACHE STRING "" FORCE)
endif()

message(STATUS "termchart 模块集合: ${TERMCHART_MODULES}")

# 供 src 侧按模块裁剪使用（当前阶段所有模块目录都存在；后续阶段按需过滤）。
function(termchart_module_enabled name out_var)
    if("${name}" IN_LIST TERMCHART_MODULES)
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()
