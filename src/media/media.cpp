#include "termchart/media/media.hpp"

#include <array>
#include <memory>
#include <mutex>
#include <utility>

#include "termchart/media/encoder/i_encoder.hpp"
#include "termchart/media/video/muxer/i_video_muxer.hpp"

#include "termchart/media/image/gif_encode.hpp"
#include "termchart/media/image/jpeg_encode.hpp"
#include "termchart/media/image/mp4.hpp"
#include "termchart/media/image/png_encode.hpp"
#include "termchart/media/image/webp_encode.hpp"

#include "image/av1_encode.hpp"

namespace termchart::media {
namespace {

using image::Bitmap;
using image::EncodeOptions;

class PngEncoder final : public IEncoder {
public:
    [[nodiscard]] std::string encode(std::span<const Bitmap> frames,
                                     const EncodeOptions&) override {
        return frames.empty() ? std::string{} : image::encode_png(frames[0]);
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "png"; }
};

class JpegEncoder final : public IEncoder {
public:
    [[nodiscard]] std::string encode(std::span<const Bitmap> frames,
                                     const EncodeOptions& opts) override {
        return frames.empty() ? std::string{} : image::encode_jpeg(frames[0], opts.quality);
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "jpeg"; }
};

class GifEncoder final : public IEncoder {
public:
    [[nodiscard]] std::string encode(std::span<const Bitmap> frames,
                                     const EncodeOptions& opts) override {
        return image::encode_gif(frames, opts);
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "gif"; }
};

class WebpEncoder final : public IEncoder {
public:
    [[nodiscard]] std::string encode(std::span<const Bitmap> frames,
                                     const EncodeOptions& opts) override {
        return image::encode_webp(frames, opts);
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "webp"; }
};

class Mp4Encoder final : public IEncoder {
public:
    [[nodiscard]] std::string encode(std::span<const Bitmap> frames,
                                     const EncodeOptions& opts) override {
        return image::encode_mp4(frames, opts);
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "mp4"; }
};

class Av1Encoder final : public IEncoder {
public:
    [[nodiscard]] std::string encode(std::span<const Bitmap> frames,
                                     const EncodeOptions&) override {
        return image::encode_av1_ivf(frames);
    }
    [[nodiscard]] std::string_view name() const noexcept override { return "av1"; }
};

// 现有 encode_* 已经产出完整文件字节；封装器先做直通，保留容器拆分点。
class PassthroughMuxer final : public IVideoMuxer {
public:
    PassthroughMuxer(std::string name, std::string extension, MuxKind kind) noexcept
        : name_(std::move(name)), extension_(std::move(extension)), kind_(kind) {}

    [[nodiscard]] MuxKind kind() const noexcept override { return kind_; }
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }
    [[nodiscard]] std::string_view extension() const noexcept override { return extension_; }

    [[nodiscard]] std::string mux(std::span<const std::string> streams,
                                  const EncodeOptions&) override {
        return streams.empty() ? std::string{} : streams.front();
    }

private:
    std::string name_;
    std::string extension_;
    MuxKind kind_;
};

std::once_flag g_once;

void do_register() {
    auto& encoders = EncoderRegistry::instance();
    encoders.register_encoder("png", [] { return std::make_unique<PngEncoder>(); });
    encoders.register_encoder("jpeg", [] { return std::make_unique<JpegEncoder>(); });
    encoders.register_encoder("jpg", [] { return std::make_unique<JpegEncoder>(); });
    encoders.register_encoder("gif", [] { return std::make_unique<GifEncoder>(); });
    encoders.register_encoder("webp", [] { return std::make_unique<WebpEncoder>(); });
    encoders.register_encoder("mp4", [] { return std::make_unique<Mp4Encoder>(); });
    encoders.register_encoder("av1", [] { return std::make_unique<Av1Encoder>(); });
    encoders.register_encoder("ivf", [] { return std::make_unique<Av1Encoder>(); });
    encoders.register_encoder("avif", [] { return std::make_unique<Av1Encoder>(); });

    auto& muxers = MuxerRegistry::instance();
    const auto single = MuxKind::SingleFrame;
    const auto multi = MuxKind::MultiFrame;
    muxers.register_muxer("png", [single] {
        return std::make_unique<PassthroughMuxer>("png", "png", single);
    });
    muxers.register_muxer("jpeg", [single] {
        return std::make_unique<PassthroughMuxer>("jpeg", "jpg", single);
    });
    muxers.register_muxer("jpg", [single] {
        return std::make_unique<PassthroughMuxer>("jpg", "jpg", single);
    });
    muxers.register_muxer("webp", [single] {
        return std::make_unique<PassthroughMuxer>("webp", "webp", single);
    });
    muxers.register_muxer("gif", [multi] {
        return std::make_unique<PassthroughMuxer>("gif", "gif", multi);
    });
    muxers.register_muxer("mp4", [multi] {
        return std::make_unique<PassthroughMuxer>("mp4", "mp4", multi);
    });
    muxers.register_muxer("av1", [multi] {
        return std::make_unique<PassthroughMuxer>("av1", "ivf", multi);
    });
    muxers.register_muxer("ivf", [multi] {
        return std::make_unique<PassthroughMuxer>("ivf", "ivf", multi);
    });
    muxers.register_muxer("avif", [multi] {
        return std::make_unique<PassthroughMuxer>("avif", "ivf", multi);
    });
}

}  // namespace

void register_all_builtin_codecs() {
    std::call_once(g_once, do_register);
}

std::string encode(std::span<const Bitmap> frames, std::string_view format,
                   const EncodeOptions& opts) {
    register_all_builtin_codecs();
    auto encoder = EncoderRegistry::instance().create(format);
    if (!encoder) return {};
    std::string stream = encoder->encode(frames, opts);
    if (stream.empty()) return {};
    auto muxer = MuxerRegistry::instance().create(format);
    if (!muxer) return stream;
    std::array<std::string, 1> streams{std::move(stream)};
    return muxer->mux(streams, opts);
}

bool has_encoder(std::string_view format) {
    register_all_builtin_codecs();
    return EncoderRegistry::instance().has(format);
}

std::vector<std::string> encoder_names() {
    register_all_builtin_codecs();
    return EncoderRegistry::instance().names();
}

bool is_multiframe(std::string_view format) {
    register_all_builtin_codecs();
    auto muxer = MuxerRegistry::instance().create(format);
    return muxer != nullptr && muxer->kind() == MuxKind::MultiFrame;
}

std::string extension_for(std::string_view format) {
    register_all_builtin_codecs();
    auto muxer = MuxerRegistry::instance().create(format);
    return muxer ? std::string(muxer->extension()) : std::string{};
}

}  // namespace termchart::media
