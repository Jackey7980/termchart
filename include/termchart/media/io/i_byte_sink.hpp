// i_byte_sink.hpp —— 抽象输出流：编码器只认这个接口，不认具体文件系统。
//
// C ABI 的两段式缓冲用 memory_sink；落盘用 file_sink。接口刻意保持极小，
// 与 std::ostream 解耦，避免把 iostream 的 locale/异常语义带进媒体层。
#ifndef TERMCHART_SRC_MEDIA_IO_I_BYTE_SINK_HPP
#define TERMCHART_SRC_MEDIA_IO_I_BYTE_SINK_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace termchart::media {

class IByteSink {
public:
    virtual ~IByteSink() = default;

    virtual bool write(std::string_view bytes) = 0;
    [[nodiscard]] virtual std::uint64_t tell() const noexcept = 0;
    virtual bool seek(std::uint64_t offset) = 0;

    [[nodiscard]] bool empty() const noexcept { return tell() == 0; }
};

}  // namespace termchart::media

#endif  // TERMCHART_SRC_MEDIA_IO_I_BYTE_SINK_HPP
