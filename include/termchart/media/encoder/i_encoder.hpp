// i_encoder.hpp —— 编码器统一接口：只负责「裸帧 → 码流」，不碰容器封装。
//
// 与 docs/design/registry.md 的统一模式一致：Creator + function-local static
// instance()。gpa/ 下的纯数学核保持静态调用，不进注册表。
#ifndef TERMCHART_SRC_MEDIA_ENCODER_I_ENCODER_HPP
#define TERMCHART_SRC_MEDIA_ENCODER_I_ENCODER_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"

namespace termchart::media {

class IEncoder {
public:
    virtual ~IEncoder() = default;

    // 返回空串表示不支持 / 编码失败（与既有 encode_* 的契约一致）。
    virtual std::string encode(std::span<const image::Bitmap> frames,
                               const image::EncodeOptions& options) = 0;

    // 稳定注册名，与 C ABI 的字符串入参一一对应。
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

class EncoderRegistry {
public:
    using Creator = std::function<std::unique_ptr<IEncoder>()>;

    static EncoderRegistry& instance() {
        static EncoderRegistry registry;
        return registry;
    }

    // 重复注册同一名字以最后一次为准（fail-loud 的替代：显式覆盖比静默忽略安全）。
    void register_encoder(std::string name, Creator creator) {
        std::lock_guard<std::mutex> lock(mutex_);
        creators_[std::move(name)] = std::move(creator);
    }

    [[nodiscard]] std::unique_ptr<IEncoder> create(std::string_view name) const {
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

    EncoderRegistry(const EncoderRegistry&) = delete;
    EncoderRegistry& operator=(const EncoderRegistry&) = delete;

private:
    EncoderRegistry() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Creator> creators_;
};

}  // namespace termchart::media

#endif  // TERMCHART_SRC_MEDIA_ENCODER_I_ENCODER_HPP
