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

# ── 静态分析（可选，默认关；开启后构建期自动对每个编译单元跑） ──────────
#   工具需在 PATH（clang-tidy 随 LLVM 安装；cppcheck 独立安装）。
#   缺工具时显式报错（fail-loud），符合项目「可选扩展缺失即报错」取向。
option(TERMCHART_ENABLE_CLANG_TIDY "构建期用 clang-tidy 做静态分析（需 LLVM/clang-tidy）" OFF)
option(TERMCHART_ENABLE_CPPCHECK "构建期用 cppcheck 做静态分析（需 cppcheck）" OFF)

if(TERMCHART_ENABLE_CLANG_TIDY)
    # scoop 装的 LLVM 在 <scoop>/apps/llvm/current/bin，而 current 是 junction。
    # Windows 上 CMake 的 find_program 不跟随 junction，故先用 REAL_PATH 解析出
    # 真实版本目录作 HINTS——无需在 PATH 硬编码具体版本号，scoop 升级（current
    # 重指新版本）后依旧有效；若解析失败则退回普通 PATH 搜索（仍可用）。
    set(_clang_tidy_hints "")
    if(DEFINED ENV{USERPROFILE})
        set(_scoop_llvm "$ENV{USERPROFILE}/scoop/apps/llvm/current/bin")
        if(EXISTS "${_scoop_llvm}")
            file(REAL_PATH "${_scoop_llvm}" _scoop_llvm_real)
            list(APPEND _clang_tidy_hints "${_scoop_llvm_real}")
        endif()
    endif()
    find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy.exe HINTS ${_clang_tidy_hints})
    if(CLANG_TIDY_EXE)
        message(STATUS "termchart: 启用 clang-tidy -> ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(FATAL_ERROR "TERMCHART_ENABLE_CLANG_TIDY=ON 但找不到 clang-tidy，请先在 PATH 安装 LLVM")
    endif()
endif()

if(TERMCHART_ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES cppcheck cppcheck.exe)
    if(CPPCHECK_EXE)
        message(STATUS "termchart: 启用 cppcheck -> ${CPPCHECK_EXE}")
        set(CMAKE_CXX_CPPCHECK "${CPPCHECK_EXE}"
            --enable=warning,style,performance,portability
            --inline-suppr
            --suppress=missingIncludeSystem
            --suppress=unmatchedSuppression
            --error-exitcode=0)
    else()
        message(FATAL_ERROR "TERMCHART_ENABLE_CPPCHECK=ON 但找不到 cppcheck，请先在 PATH 安装 cppcheck")
    endif()
endif()

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
