// file_sink.hpp —— 本地文件输出流。仅此文件包含具体文件系统逻辑。
#ifndef TERMCHART_SRC_MEDIA_IO_FILE_SINK_HPP
#define TERMCHART_SRC_MEDIA_IO_FILE_SINK_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include "termchart/media/io/i_byte_sink.hpp"

namespace termchart::media {

class FileSink final : public IByteSink {
public:
    explicit FileSink(std::string_view path) noexcept;
    ~FileSink() override;

    bool write(std::string_view bytes) override;
    [[nodiscard]] std::uint64_t tell() const noexcept override;
    bool seek(std::uint64_t offset) override;

    [[nodiscard]] bool opened() const noexcept { return fp_ != nullptr; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

    FileSink(const FileSink&) = delete;
    FileSink& operator=(const FileSink&) = delete;

private:
    std::string path_;
    std::string error_;
    void* fp_ = nullptr;
};

}  // namespace termchart::media

#endif  // TERMCHART_SRC_MEDIA_IO_FILE_SINK_HPP
