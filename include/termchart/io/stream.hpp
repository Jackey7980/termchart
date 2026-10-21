// io/stream.hpp —— 分块行读取（L2）
//
// 用途：GB 级 CSV/日志不整文件入内存。内部 64KB 缓冲，逐行产出 string_view
// （视图在下一次 next()/close() 前有效，要跨行保留请自行拷贝）。
// 关键不变量：
//   * 任意长的单行都不 OOM：超长行走"compact + 续读"路径，缓冲最坏增长到
//     该行自身长度（行必须放得下，没有第二条路）；
//   * 行尾口径：LF / CRLF / CR 均切行，行内容不含行尾符；
//   * 不抛异常：打开失败走 Result；读完 next() 返回 false。
#ifndef TERMCHART_IO_STREAM_HPP
#define TERMCHART_IO_STREAM_HPP

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"

namespace termchart::io {

class LineReader {
public:
    LineReader() noexcept = default;
    ~LineReader() noexcept;
    LineReader(LineReader&& other) noexcept;
    LineReader& operator=(LineReader&& other) noexcept;
    LineReader(const LineReader&) = delete;
    LineReader& operator=(const LineReader&) = delete;

    [[nodiscard]] bool valid() const noexcept { return file_ != nullptr || owns_string_; }

    // 打开文件（二进制读）。失败返回错误码（NotFound 等）。
    [[nodiscard]] static Result<LineReader> open(const std::string& path) noexcept;

    // 内存源（测试 / 小文件 / 已下载的文本）。零 I/O，永不为空失败。
    [[nodiscard]] static LineReader from_string(std::string text) noexcept;

    // 取下一行（不含行尾）。返回 false = 流结束；line 的有效期见类注释。
    [[nodiscard]] bool next(std::string_view& line) noexcept;

    void close() noexcept;

private:
    // 续读一块到 buf_；返回读到的字节数（0 = EOF）。
    std::size_t fill() noexcept;

    std::FILE* file_ = nullptr;
    std::string owned_;              // from_string 源
    bool owns_string_ = false;       // from_string 构造过（空串也是合法源）
    std::vector<char> buf_;          // 分块缓冲
    std::size_t buf_len_ = 0;        // 有效字节
    std::size_t buf_pos_ = 0;        // 消费游标
    std::size_t owned_pos_ = 0;      // 内存源游标
};

}  // namespace termchart::io

#endif  // TERMCHART_IO_STREAM_HPP
