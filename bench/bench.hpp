// bench.hpp —— 极简计时骨架（零外部依赖）
//
// 为什么不用 Google Benchmark：本仓库的约束是"每个外部依赖都必须可缺失"，bench
// 也必须能在裸环境编译运行。这里只做三件事：预热、跑固定轮数取**中位数**（比均值
// 抗抖动）、统一格式打印。
//
// 约定：
//   * 每个 bench 可执行文件接受 `--quick`：只跑最小规模的一两档，供 ctest 冒烟
//     （CI 不该被基线测量拖慢，但又要保证 bench 本身不会烂掉）。
//   * 被测对象若会被就地修改（如布局写 Node::set_box），调用方必须每次用干净副本，
//     否则第二轮测的是"已经布局好的图"。
#ifndef TERMCHART_BENCH_BENCH_HPP
#define TERMCHART_BENCH_BENCH_HPP

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace termchart::bench {

// 命令行里是否带 --quick。
inline bool is_quick(int argc, char** argv) noexcept {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--quick") {
            return true;
        }
    }
    return false;
}

// 跑 rounds 轮，返回每轮耗时的**中位数**（纳秒）。rounds 至少按 1 轮算。
inline double median_ns(const std::function<void()>& body, int rounds) {
    const auto count = static_cast<std::size_t>(std::max(1, rounds));
    std::vector<double> samples;
    samples.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto start = std::chrono::steady_clock::now();
        body();
        const auto end = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double, std::nano>(end - start).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

inline void header(const char* bench_name, const char* what) {
    std::printf("== %s == %s\n", bench_name, what);
    std::printf("%-22s %-30s %12s\n", "case", "size", "median");
}

inline void report(const std::string& name, const std::string& size, double ns) {
    std::printf("%-22s %-30s %10.1f us\n", name.c_str(), size.c_str(), ns / 1000.0);
}

}  // namespace termchart::bench

#endif  // TERMCHART_BENCH_BENCH_HPP
