#include "termchart/media/io/file_sink.hpp"

#include <cstdio>

#include "termchart/util/file.hpp"

namespace termchart::media {

FileSink::FileSink(std::string_view path) noexcept : path_(path) {
    fp_ = util::fopen_utf8(path_, "wb");
    if (fp_ == nullptr) error_ = "cannot open " + path_;
}

FileSink::~FileSink() {
    if (fp_ != nullptr) std::fclose(static_cast<std::FILE*>(fp_));
}

bool FileSink::write(std::string_view bytes) {
    if (fp_ == nullptr) return false;
    const std::size_t written = std::fwrite(bytes.data(), 1, bytes.size(),
                                            static_cast<std::FILE*>(fp_));
    if (written != bytes.size()) {
        error_ = "short write to " + path_;
        return false;
    }
    return true;
}

std::uint64_t FileSink::tell() const noexcept {
    if (fp_ == nullptr) return 0;
    return static_cast<std::uint64_t>(std::ftell(static_cast<std::FILE*>(fp_)));
}

bool FileSink::seek(std::uint64_t offset) {
    if (fp_ == nullptr) return false;
    return std::fseek(static_cast<std::FILE*>(fp_), static_cast<long>(offset), SEEK_SET) == 0;
}

}  // namespace termchart::media
