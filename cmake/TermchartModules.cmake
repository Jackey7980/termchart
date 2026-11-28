# TermchartModules.cmake —— 分层规则唯一源
#
# 规则（判定顺序，自上而下，先命中先定）：
#   1. FORBIDDEN_EDGES：模块级禁止，一票否决。
#   2. NARROW_EDGES：若某模块对出现在 NARROW_EDGES 中，
#      则该模块对的访问转为"文件级白名单"：
#        - 左模块中除白名单列出的文件外，其余文件不得 include 右模块任何文件；
#        - 右模块中除白名单列出的文件外，其余文件不得被左模块 include。
#   3. 层号：文件只能 include 层号 ≤ 自己模块的文件。
#
# 粒度说明：
#   - 模块 = include/termchart/ 下的一级目录名。
#   - 文件路径 = 相对 include/termchart/ 的去扩展名路径，例如 backend/snapshot/ansi_backend。
#   - 边格式："左文件>右文件"，两侧均为去扩展名的相对路径。
#
# 顶层文件（include/termchart/ 根下）：
#   termchart.hpp、termchart_full.hpp 为伞形头，豁免层号判定（见 check_layers.py）。
#   其余顶层文件（version/export/deprecate/fwd）视为 core（层号 0）。

set(TERMCHART_MODULE_LAYERS
    "core=0"
    "util=1"
    "math=1"
    "color=2"
    "io=2"
    "scale=3"
    "math3d=3"
    "math4d=3"
    "scene=4"
    "data=4"
    "media=5"
    "raster=6"
    "term=7"
    "network=7"
    "document=8"
    "protocol=9"
    "canvas=10"
    "layout=11"
    "chart=12"
    "chart3d=12"
    "chart4d=12"
    "graph=13"
    "render=14"
    "interaction=15"
    "backend=16"
    "dsl=17"
    "widget=18"
    "plugin=19"
    "app=20"
    "api=21"
    "autoviz=21"
    "gui=22"
)

# 收窄规则：把模块级允许收窄为文件级白名单。
# 只有当确需"某个模块对之间只允许极少数文件互相 include"时才登记，
# 一旦登记，该模块对的其余访问全部关闭。
set(TERMCHART_NARROW_EDGES
    "backend/snapshot/ansi_backend>render/sgr"   # 导出路径复用唯一 SGR 序列化原语；render 内其余文件对 backend 关闭
)

# 禁止边：层号管不住、必须显式拒绝的模块对。优先级最高。
set(TERMCHART_FORBIDDEN_EDGES
    "backend>chart"       # backend 走 Document，不碰 chart 类型（层号 16 ≥ 12 管不住）
    "chart>render"        # chart 不碰渲染管线，保住 to_document()（层号已覆盖，显式记录）
    "render>interaction"  # 故意反向：interaction 要能 include render（层号已覆盖，显式记录）
    "gui>canvas"          # GUI 平行栈不得碰终端呈现：字符格/CellBuffer 是终端专属（gui_stack.md）
    "gui>term"            # 同上：SGR/tty 是终端专属
    "gui>app"             # GUI 的事件循环自己写（native_loop），不借终端 loop
    # media DAG：encoder 是纯内核，业务层（image/video）可 include 它，反向一律禁止。
    # 含 "/" 的条目按路径前缀匹配（见 check_layers.py），模块级条目仍按一级目录。
    "media/encoder>media/image"
    "media/encoder>media/video"
)

# 路径级禁止边的类型依赖例外：encoder 接口需要 Bitmap / EncodeOptions 等共享值类型，
# 这些是 kernel 与业务层共同的数据契约，不是业务实现，故单列白名单。
set(TERMCHART_PATH_EXCEPTIONS
    "media/encoder>media/image/bitmap"
    "media/encoder>media/image/encode"
    "media/encoder>media/image/convert"
    "media/encoder>media/image/field"
)

# 设计意图（由上面两个集合机器执行，本段仅供人读）
#
#   layout > canvas            facet 只做几何划分，绘制靠 canvas（层号已允许）
#   interaction > chart        命中判定需要 chart 的 HitRegion 语义（层号已允许）
#   interaction > graph        同上（层号已允许）
#   backend > render           收窄为 ansi_backend>sgr 一对（NARROW_EDGES）
#   chart > render             禁止：chart 不碰渲染管线
#   backend > chart            禁止：backend 走 Document
#   render > interaction       禁止：维持单向，interaction 可 include render
