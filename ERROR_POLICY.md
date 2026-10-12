# Error Policy / 错误处理策略

本文是 `include/termchart/core/error.hpp` 与 `core/expected.hpp` 顶部注释
所载约定的展开版；两者冲突时以头文件注释为准。

## 三类失败，三条通道

| 失败种类 | 通道 | 位置 |
| --- | --- | --- |
| 可恢复的、用户可能遇到的失败（解析失败、IO 失败、参数超界、能力不支持） | `Expected<T,E>` / `Result<T>` | `core/expected.hpp` + `core/error.hpp` |
| 契约破坏 / 内部不变量（调用方 bug） | `util/assert.hpp` 断言，Release 下 `abort()` | `util/assert.hpp` |
| 渲染热路径内的可钳位偏差 | 不报错，直接钳位/裁剪 | `canvas/`、`raster/` 各落格路径 |

**本库不向调用方抛异常**。渲染链任何函数都不会以异常作为正常错误通道。

## 错误码

`ErrorCode`（`core/error.hpp`）按类别分段编号：

| 段 | 类别（`ErrorCategory`） | 码值 |
| --- | --- | --- |
| 0 | none | `Ok = 0` |
| 100 段 | logic —— 调用方契约问题，改代码才能解决 | `InvalidArgument=100`、`OutOfRange=101`、`NotInitialized=104` |
| 100 段 | domain —— 数据/参数不合经营范围 | `Unsupported=102`、`NotImplemented=103` |
| 200 段 | encoding —— UTF-8 / 编码相关 | `MalformedUtf8=200`、`ParseError=201` |
| 300 段 | platform —— IO / 终端 / OS | `IoError=300`、`TerminalUnavailable=301`、`PlatformError=302` |

辅助约定：

- 只回报成败的函数返回 `Status`（= `ErrorCode`）；需要携带值的才用 `Result<T>`；
- `category_of()` 给日志与上层分流用，不与具体码一一对应；
- `to_string()` 是稳定英文标识，可用于断言与测试；`describe()` 是人类可读描述；
- `Expected::value()` 无值访问属调用方 bug，抛 `std::logic_error`（实现移出
  头文件，在 `src/core/expected.cpp`，避免头文件膨胀）——这不是错误通道。

## 调用方建议

1. 检查风格：`if (auto r = f(); !r) { /* r.error() */ }` 或显式 `is_ok()`；
2. 不要吞错误码：`Unsupported` 通常意味着当前终端/构建缺少能力，
   可参考 `protocol/` 与 `color` 的降级链自行回退；
3. 新增错误码必须挂进正确的类别段，并在 `category_of()` 补分支
   （漏分支会在测试中暴露）。
