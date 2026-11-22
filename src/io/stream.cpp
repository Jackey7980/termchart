#include "termchart/io/stream.hpp"

#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/util/file.hpp"

namespace termchart::io {
namespace {

constexpr std::size_t kChunk = 64u * 1024u;  // 64KB 块

}  // namespace

LineReader::~LineReader() noexcept { close(); }

LineReader::LineReader(LineReader&& other) noexcept
    : file_(other.file_),
      owned_(std::move(other.owned_)),
      owns_string_(other.owns_string_),
      buf_(std::move(other.buf_)),
      buf_len_(other.buf_len_),
      buf_pos_(other.buf_pos_),
      owned_pos_(other.owned_pos_) {
    other.file_ = nullptr;
    other.owns_string_ = false;
    other.buf_len_ = 0;
    other.buf_pos_ = 0;
    other.owned_pos_ = 0;
}

LineReader& LineReader::operator=(LineReader&& other) noexcept {
    if (this != &other) {
        close();
        file_ = other.file_;
        owned_ = std::move(other.owned_);
        owns_string_ = other.owns_string_;
        buf_ = std::move(other.buf_);
        buf_len_ = other.buf_len_;
        buf_pos_ = other.buf_pos_;
        owned_pos_ = other.owned_pos_;
        other.file_ = nullptr;
        other.owns_string_ = false;
        other.buf_len_ = 0;
        other.buf_pos_ = 0;
        other.owned_pos_ = 0;
    }
    return *this;
}

Result<LineReader> LineReader::open(const std::string& path) noexcept {
    std::FILE* f = util::fopen_utf8(path, "rb");
    if (f == nullptr) {
        return Unexpected<ErrorCode>(ErrorCode::IoError);
    }
    LineReader reader;
    reader.file_ = f;
    reader.buf_.resize(kChunk);
    return Result<LineReader>(std::move(reader));
}

LineReader LineReader::from_string(std::string text) noexcept {
    LineReader reader;
    reader.owned_ = std::move(text);
    reader.owns_string_ = true;
    return reader;
}

void LineReader::close() noexcept {
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
    owned_.clear();
    owns_string_ = false;  // close 之后 next() 一律 false
    buf_len_ = 0;
    buf_pos_ = 0;
    owned_pos_ = 0;
}

std::size_t LineReader::fill() noexcept {
    // 把未消费的残余 compact 到缓冲头，再续读一块。
    if (buf_pos_ > 0 && buf_pos_ < buf_len_) {
        std::move(buf_.begin() + static_cast<std::ptrdiff_t>(buf_pos_),
                  buf_.begin() + static_cast<std::ptrdiff_t>(buf_len_), buf_.begin());
        buf_len_ -= buf_pos_;
        buf_pos_ = 0;
    } else if (buf_pos_ >= buf_len_) {
        buf_len_ = 0;
        buf_pos_ = 0;
    }
    if (buf_.empty()) buf_.resize(kChunk);
    const std::size_t got =
        std::fread(buf_.data() + buf_len_, 1, buf_.size() - buf_len_, file_);
    buf_len_ += got;
    return got;
}

bool LineReader::next(std::string_view& line) noexcept {
    if (!owned_.empty()) {
        // 内存源：直接切 owned_。
        if (owned_pos_ >= owned_.size()) return false;
        std::size_t end = owned_.find('\n', owned_pos_);
        if (end == std::string::npos) end = owned_.size();
        std::size_t stop = end;
        if (stop > owned_pos_ && owned_[stop - 1] == '\r') --stop;
        line = std::string_view(owned_).substr(owned_pos_, stop - owned_pos_);
        owned_pos_ = (end < owned_.size()) ? end + 1 : owned_.size();
        return true;
    }
    if (file_ == nullptr) return false;
    // 文件源：在缓冲内找行尾；找不到就续读（行可跨块，最坏占满整个缓冲后增长）。
    while (true) {
        for (std::size_t i = buf_pos_; i < buf_len_; ++i) {
            if (buf_[i] != '\n') continue;
            std::size_t stop = i;
            if (stop > buf_pos_ && buf_[stop - 1] == '\r') --stop;
            line = std::string_view(buf_.data() + buf_pos_, stop - buf_pos_);
            buf_pos_ = i + 1;
            return true;
        }
        // 缓冲内无行尾：若已 EOF 且缓冲非空 → 末行（无换行结尾）。
        const std::size_t got = fill();
        if (got == 0) {
            if (buf_pos_ < buf_len_) {
                std::size_t stop = buf_len_;
                if (stop > buf_pos_ && buf_[stop - 1] == '\r') --stop;
                line = std::string_view(buf_.data() + buf_pos_, stop - buf_pos_);
                buf_pos_ = buf_len_;
                return true;
            }
            return false;
        }
        // 续读后仍无行尾且缓冲已满：行比块大，缓冲翻倍续读（上限 = 行自身长度）。
        bool has_newline = false;
        for (std::size_t i = buf_pos_; i < buf_len_; ++i) {
            if (buf_[i] == '\n') {
                has_newline = true;
                break;
            }
        }
        if (!has_newline && buf_len_ == buf_.size()) {
            buf_.resize(buf_.size() * 2);
        }
    }
}

}  // namespace termchart::io
