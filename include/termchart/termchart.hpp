// termchart.hpp —— 伞形头（极简）
//
// 只包含"开箱即用"所需的最小集合：核心数据结构 + 终端 + 渲染 + 一行接口。
// 全量见 termchart_full.hpp（阶段 6）。
//
// 分层豁免：伞形头是聚合入口，天然依赖全模块，check_layers.py 对它不做层号判定。
#ifndef TERMCHART_TERMCHART_HPP
#define TERMCHART_TERMCHART_HPP

#include "termchart/deprecate.hpp"
#include "termchart/export.hpp"
#include "termchart/fwd.hpp"
#include "termchart/version.hpp"

// core
#include "termchart/core/arena.hpp"
#include "termchart/core/attr.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/error.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/core/grapheme.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/span_utils.hpp"
#include "termchart/core/style.hpp"

// util
#include "termchart/util/assert.hpp"
#include "termchart/util/bits.hpp"
#include "termchart/util/format.hpp"
#include "termchart/util/grapheme.hpp"
#include "termchart/util/logging.hpp"
#include "termchart/util/math.hpp"
#include "termchart/util/random.hpp"
#include "termchart/util/text.hpp"
#include "termchart/util/utf8.hpp"
#include "termchart/util/wcwidth.hpp"

// color
#include "termchart/color/colormap.hpp"
#include "termchart/color/gradient.hpp"
#include "termchart/color/named_colors.hpp"
#include "termchart/color/palette.hpp"
#include "termchart/color/quantize.hpp"

// term
#include "termchart/term/ansi.hpp"
#include "termchart/term/caps.hpp"
#include "termchart/term/daemon_safe.hpp"
#include "termchart/term/raw_mode.hpp"
#include "termchart/term/signal.hpp"
#include "termchart/term/size.hpp"
#include "termchart/term/terminal.hpp"
#include "termchart/term/waker.hpp"

// render
#include "termchart/render/damage.hpp"
#include "termchart/render/diff.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/render/sgr.hpp"

// api
#include "termchart/api/opts.hpp"
#include "termchart/api/print.hpp"
#include "termchart/api/quick.hpp"

// 便捷别名：绝大多数用户代码只写 tc::。
namespace tc = termchart;

// 版本查询（定义在 src/termchart.cpp）。
namespace termchart {
const char* version_string() noexcept;
}

#endif  // TERMCHART_TERMCHART_HPP
