#include "termchart/api/print.hpp"

#include <string>

#include "termchart/util/format.hpp"

namespace termchart::api {

Status write_to(std::FILE* stream, std::string_view bytes) noexcept {
    if (stream == nullptr) {
        return ErrorCode::InvalidArgument;
    }
    if (bytes.empty()) {
        return ErrorCode::Ok;
    }
    const std::size_t written = std::fwrite(bytes.data(), 1, bytes.size(), stream);
    return written == bytes.size() ? ErrorCode::Ok : ErrorCode::IoError;
}

Status write_stdout(std::string_view bytes) noexcept { return write_to(stdout, bytes); }

Status write_stderr(std::string_view bytes) noexcept { return write_to(stderr, bytes); }

Status flush(std::FILE* stream) noexcept {
    if (stream == nullptr) {
        return ErrorCode::InvalidArgument;
    }
    return std::fflush(stream) == 0 ? ErrorCode::Ok : ErrorCode::IoError;
}

Status flush_stdout() noexcept { return flush(stdout); }

std::string escape_for_debug(std::string_view bytes) {
    std::string out;
    out.reserve(bytes.size() * 2);
    // 显式转 unsigned char：direct-list-init 到 unsigned char 会触发
    // -Wsign-conversion（char 在 MSVC 下有符号），而这里按字节比较才是本意。
    for (const char raw : bytes) {
        const auto ch = static_cast<unsigned char>(raw);
        if (ch == '\033') {
            out += "<ESC>";
        } else if (ch == '\n') {
            out += "<LF>";
        } else if (ch == '\r') {
            out += "<CR>";
        } else if (ch < 0x20 || ch == 0x7F) {
            out += "^";
            out += static_cast<char>(ch + 64);
        } else if (ch >= 0x80) {
            out += "\\x";
            out += util::format_int(static_cast<std::int64_t>(ch));
        } else {
            out += static_cast<char>(ch);
        }
    }
    return out;
}

}  // namespace termchart::api
