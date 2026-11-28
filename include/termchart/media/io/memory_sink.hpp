// memory_sink.hpp —— 内存输出流：给 C ABI 两段式缓冲与单测用。
#ifndef TERMCHART_SRC_MEDIA_IO_MEMORY_SINK_HPP
#define TERMCHART_SRC_MEDIA_IO_MEMORY_SINK_HPP

#include <string>
#include <string_view>

#include "termchart/media/io/i_byte_sink.hpp"

namespace termchart::media {

class MemorySink final : public IByteSink {
public:
    bool write(std::string_view bytes) override {
        data_.append(bytes.data(), bytes.size());
        return true;
    }

    [[nodiscard]] std::uint64_t tell() const noexcept override {
        return static_cast<std::uint64_t>(data_.size());
    }

    bool seek(std::uint64_t offset) override {
        if (offset > data_.size()) return false;
        data_.resize(static_cast<std::size_t>(offset));
        return true;
    }

    [[nodiscard]] const std::string& data() const noexcept { return data_; }
    void clear() noexcept { data_.clear(); }

private:
    std::string data_;
};

}  // namespace termchart::media

#endif  // TERMCHART_SRC_MEDIA_IO_MEMORY_SINK_HPP
