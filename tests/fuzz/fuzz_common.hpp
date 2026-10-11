// fuzz_common.hpp —— fuzz 目标的公共骨架（tests 私有，不进库）
//
// 设计口径：**fuzz 目标本身必须能在没有 libFuzzer 的环境里跑**。三档运行方式：
//   * `--self-test`   内置畸形语料（空串、单字节、截断的 UTF-8/ESC、全 0xFF、
//                     长重复、混入 NUL…），逐条喂给 target。确定性、零文件依赖，
//                     因此可以直接进 ctest —— 没有这一档，fuzz 目标迟早会烂掉；
//   * `--iterations N [--seed S]`：用确定性 PRNG（xorshift64*）造 N 组随机输入。
//                     同一 seed 必然复现同一串输入，崩溃可精确重现；
//   * `<file>...`     把文件内容当输入，用于复现实际崩溃样本。
//
// "不崩"是唯一断言：目标函数内部不得吞掉异常（除了各 API 自己声明的错误码路径），
// 一旦有越界/UB，sanitizer 会直接终止进程 —— 这正是我们要它做的事。
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace fuzz {

// xorshift64*：小而确定，足以造出"看起来随机"的字节流。
class Rng {
public:
    explicit Rng(std::uint64_t seed) noexcept
        : state_(seed != 0 ? seed : 0x9E3779B97F4A7C15ull) {}

    [[nodiscard]] std::uint64_t next() noexcept {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 7;
        state_ ^= state_ << 17;
        return state_ * 0x2545F4914F6CDD1Dull;
    }

    [[nodiscard]] std::uint8_t byte() noexcept {
        return static_cast<std::uint8_t>(next() >> 56);
    }

    [[nodiscard]] std::uint32_t below(std::uint32_t bound) noexcept {
        return bound == 0 ? 0 : static_cast<std::uint32_t>(next() % bound);
    }

private:
    std::uint64_t state_;
};

// 一小组最容易踩边界的输入：每条都对应一类真实 bug（截断序列、非法续接字节、
// 超长序列、NUL 混入、以及"看起来合法但长度不对"的躯干）。
//
// 每条种子都在注释里写明它想触发**哪条分支**——否则语料会退化成"随便凑几段字节"，
// 覆盖率上不去而没人发现。
[[nodiscard]] inline std::vector<std::string> seed_corpus() {
    std::vector<std::string> seeds;
    seeds.emplace_back();                                   // 空
    seeds.emplace_back("\0", 1);                            // 单个 NUL
    seeds.emplace_back("\x1b");                             // 孤立的 ESC
    seeds.emplace_back("\x1b[");
    seeds.emplace_back("\x1b[<0;0;0M");                     // SGR 鼠标按下
    seeds.emplace_back("\x1b[<35;999;999;8m");              // 鼠标 + 修饰键
    seeds.emplace_back("\x1b]1337;File=inline=1:");         // 被截断的 OSC（无终止符）
    seeds.emplace_back("\x1bPq#0;2;100;0;0#");              // 被截断的 sixel DCS
    seeds.emplace_back("\xe4\xb8");                         // 截断的 UTF-8（3 字节只剩 2）
    seeds.emplace_back("\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa9");  // ZWJ 序列
    seeds.emplace_back("\xf0\x9f\x87\xa6\xf0\x9f\x87\xba");  // 区域指示符对（GB12/13）
    seeds.emplace_back("\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7\xe2\x80\x8d\xf0\x9f\x91\xa6");  // 长 ZWJ 链（家庭 emoji）
    seeds.emplace_back("\xed\xa0\x80");                     // 代理区码点（非法 UTF-8）
    seeds.emplace_back("\xc0\x80");                         // 2 字节过长编码
    seeds.emplace_back("\xf0\x80\x80\x80");                 // 4 字节过长编码
    seeds.emplace_back("\xf8\x80\x80\x80\x80");             // 5 字节前导（早已废弃）
    seeds.emplace_back("\xfc\x80\x80\x80\x80\x80");         // 6 字节前导（早已废弃）
    seeds.emplace_back(std::string(300, '\xff'));           // 全 0xFF 长串
    seeds.emplace_back(std::string(64, 'a') + "\x1b[38;2;999999999;0;0m");  // 越界参数
    seeds.emplace_back("\x1b[" + std::string(512, ';'));    // CSI 参数洪水：无终止字节（走"不完整"分支）
    seeds.emplace_back("\x1b]" + std::string(1024, 'x'));   // OSC 无终止符长内容（走"不完整"分支）
    // 下面两条才是**真正解析超长数字**的种子：上面的 CSI 洪水没有终止字节，解析器停在
    // "不完整"分支、根本不会调用 read_int；这两条分别走功能键与 SGR 鼠标的数字解析路径，
    // 用来压 read_int 的饱和处理（那是有符号溢出的回归防线）。
    seeds.emplace_back("\x1b[999999999999999999~");          // 功能键编号 18 位
    seeds.emplace_back("\x1b[<999999999999999999;0;0M");     // SGR 鼠标按钮码 18 位
    seeds.emplace_back(std::string("\x89PNG\r\n\x1a\n", 8));  // 只有签名，没有块

    // 注：这里**刻意没有**"sixel 调色板洪水"种子。fuzz_sixel 的输入是"字节 → Bitmap"，
    // 不是 sixel 文本；而编码器把像素映射进固定 256 项调色板（`for i in 0..255`），
    // "颜色数 > 256"这条分支结构上不存在 —— 写这种种子只会让注释在骗人。
    return seeds;
}

using Target = void (*)(const std::uint8_t* data, std::size_t size);

inline void run_one(const Target& target, const std::string& input) {
    target(reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
}

// 单条随机输入的长度上限。**刻意不大**：utf8/grapheme 目标是"逐位置"扫描（每字节
// 调一次 decode / next_boundary），input_parser 的分块喂入也是近 O(n²)；上限给到
// 4096 会让 2 万组随机输入跑到 70 秒以上。512 已足够覆盖各类边界，又能让压测在
// 一分钟内跑完（真实缺陷在几十字节的输入上就会现形）。
inline constexpr std::size_t kMaxRandomSize = 512;

inline std::string make_random_input(Rng& rng, std::size_t max_size) {
    const std::size_t size = rng.below(static_cast<std::uint32_t>(max_size) + 1);
    std::string input;
    input.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        // 一半纯随机字节，一半偏向"结构性"字节（ESC/CSI/UTF-8 前导/0x00），
        // 后者更容易落进解析器的分支里。
        const std::uint32_t pick = rng.below(2);
        if (pick == 0) {
            input.push_back(static_cast<char>(rng.byte()));
        } else {
            static constexpr char kStructural[] = {'\x1b', '[', ';', 'm', 'M', 'q', '#',
                                                   '0',    '9', '\x7f', '\xc3', '\xe2',
                                                   '\xf0', '\x00', ':', '/', ';'};
            input.push_back(kStructural[rng.below(sizeof(kStructural))]);
        }
    }
    return input;
}

// 读整个文件；失败返回 false（调用方报错退出）。
inline bool read_file(const char* path, std::string& out) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    out.clear();
    char buffer[4096];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        out.append(buffer, got);
    }
    std::fclose(file);
    return true;
}

inline int run(int argc, char** argv, const Target& target,
               const std::vector<std::string>& extra_seeds = {}) {
    bool self_test = false;
    std::uint32_t iterations = 0;
    std::uint64_t seed = 0x5EED1234u;
    std::vector<const char*> files;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--self-test") {
            self_test = true;
        } else if (arg == "--iterations" && i + 1 < argc) {
            iterations = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<std::uint64_t>(std::strtoull(argv[++i], nullptr, 10));
        } else {
            files.push_back(argv[i]);
        }
    }

    if (self_test || (iterations == 0 && files.empty())) {
        std::size_t index = 0;
        for (const std::string& seed_input : seed_corpus()) {
            run_one(target, seed_input);
            ++index;
        }
        // 目标专属语料：有些目标需要"结构完整"的输入才能进入深层分支
        // （例如 PNG 的往返路径要求输入是合法 PNG），共享语料做不到。
        for (const std::string& seed_input : extra_seeds) {
            run_one(target, seed_input);
            ++index;
        }
        std::printf("self-test: %zu 条语料全部通过（无崩溃）\n", index);
        return 0;
    }

    for (const char* path : files) {
        std::string content;
        if (!read_file(path, content)) {
            std::fprintf(stderr, "无法读取输入文件：%s\n", path);
            return 2;
        }
        run_one(target, content);
    }

    if (iterations > 0) {
        Rng rng(seed);
        for (std::uint32_t i = 0; i < iterations; ++i) {
            run_one(target, make_random_input(rng, kMaxRandomSize));
        }
        std::printf("随机输入: %u 组（seed=%llu，单条上限 %zu 字节）全部通过（无崩溃）\n",
                    iterations, static_cast<unsigned long long>(seed), kMaxRandomSize);
    }
    return 0;
}

}  // namespace fuzz

// 每个 fuzz 目标只需定义好 target 函数，再用这个宏补 main。
#define TERMCHART_FUZZ_MAIN(fn)              \
    int main(int argc, char** argv) {        \
        return ::fuzz::run(argc, argv, fn);  \
    }

// 需要"目标专属语料"时用这个：seed_fn 返回 `std::vector<std::string>`。
#define TERMCHART_FUZZ_MAIN_WITH_SEEDS(fn, seed_fn)                            \
    int main(int argc, char** argv) {                                          \
        return ::fuzz::run(argc, argv, fn, seed_fn());                         \
    }
