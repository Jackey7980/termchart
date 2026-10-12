# 错误处理

## 决策

**不抛异常**。C++23 的 `std::expected` 不可用，因此 `core/expected.hpp` 自带
`Expected<T,E>`（基于 variant，语义对齐 `std::expected`），并派生：

```cpp
template <class T> using Result = Expected<T, ErrorCode>;
using Status = ErrorCode;   // 只回报成败
```

## 选择规则

| 场景 | 返回类型 | 例子 |
| --- | --- | --- |
| 只回报成败 | `Status`（即 `ErrorCode`） | `print_stdout`、`Terminal::enter` |
| 成功携带值、失败带错误码 | `Result<T>` | `render_line → Result<std::string>`、`parse_mermaid → Result<AstGraph>` |
| 无值访问 `value()` | 抛异常 | 这是**调用方 bug**，实现放在 `src/core/expected.cpp`（避免头文件膨胀） |
| 越界读 | 返回零值/钳制，不报错 | `Bitmap::at` 越界返回 0；`Canvas` 负坐标裁剪 |
| 缺配置 | 用默认/保守值 | `palette_by_name` 未知名字返回 Default |

## 设计理由

- 终端库常用于信号处理器邻域与 nothrow 语境（`Terminal::restore()` 是 noexcept 版的
  leave，给析构/信号处理用），异常穿过这些路径不可接受；
- `[[nodiscard]]` + `Status`/`Result` 让忽略错误在编译期可见；
- "数据是脏的"（用户 CSV 含 NaN、DSL 是用户写的文本）：解析类 API 把非法输入作为
  正常错误返回（如 `dsl::parse_mermaid` 带 `ParseDiagnostic` 出参），
  绝不假装解析成功——mermaid 序列图不支持时就返回 `Unsupported` 并说明原因。

## 反例

- 用异常表达"用户给了坏数据"；
- 返回 bool 让调用方无法区分错误类别；
- 在 header 里写抛异常的 `value()` 实现（膨胀每个包含者）。
