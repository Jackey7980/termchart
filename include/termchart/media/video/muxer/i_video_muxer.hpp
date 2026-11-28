// i_video_muxer.hpp —— 容器封装器接口与注册表。
//
// 与 EncoderRegistry 同样的模式，但这里管的是「容器」：png/jpeg/gif 单图，
// mp4/ivf 多帧。C ABI 的字符串路由只需查这里，不再手写 if-else。
#ifndef TERMCHART_SRC_MEDIA_VIDEO_MUXER_I_VIDEO_MUXER_HPP
#define TERMCHART_SRC_MEDIA_VIDEO_MUXER_I_VIDEO_MUXER_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"

namespace termchart::media {

enum class MuxKind : std::uint8_t {
    SingleFrame,  // png / jpeg / webp
    MultiFrame,   // gif / mp4 / ivf
};

class IVideoMuxer {
public:
    virtual ~IVideoMuxer() = default;

    [[nodiscard]] virtual MuxKind kind() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::string_view extension() const noexcept = 0;

    // 把已编码的码流封装成最终字节流；空串表示失败。
    virtual std::string mux(std::span<const std::string> streams,
                            const image::EncodeOptions& options) = 0;
};

class MuxerRegistry {
public:
    using Creator = std::function<std::unique_ptr<IVideoMuxer>()>;

    static MuxerRegistry& instance() {
        static MuxerRegistry registry;
        return registry;
    }

    void register_muxer(std::string name, Creator creator) {
        std::lock_guard<std::mutex> lock(mutex_);
        creators_[std::move(name)] = std::move(creator);
    }

    [[nodiscard]] std::unique_ptr<IVideoMuxer> create(std::string_view name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = creators_.find(std::string(name));
        if (it == creators_.end()) return nullptr;
        return it->second ? it->second() : nullptr;
    }

    [[nodiscard]] bool has(std::string_view name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return creators_.find(std::string(name)) != creators_.end();
    }

    [[nodiscard]] std::vector<std::string> names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> out;
        out.reserve(creators_.size());
        for (const auto& kv : creators_) out.push_back(kv.first);
        return out;
    }

    MuxerRegistry(const MuxerRegistry&) = delete;
    MuxerRegistry& operator=(const MuxerRegistry&) = delete;

private:
    MuxerRegistry() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Creator> creators_;
};

}  // namespace termchart::media

#endif  // TERMCHART_SRC_MEDIA_VIDEO_MUXER_I_VIDEO_MUXER_HPP
