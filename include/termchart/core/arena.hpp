// arena.hpp —— 帧分配器
//
// 用途：每帧大量的短命小对象（anvas 路径片段、布局候选矩形）。
// 语义：**只能 reset()，不能单独释放**——一帧结束整块回收，无析构开销。
//
// 硬约束（docs/design/memory_model.md）：
//   * 持有堆资源的类型**禁止**进 arena：create<T>() 用 static_assert 卡住
//     （非 trivially destructible 直接编译失败）。
//   * Arena 自身非拷贝非移动：它是"帧"的同义词。
#ifndef TERMCHART_CORE_ARENA_HPP
#define TERMCHART_CORE_ARENA_HPP

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "termchart/export.hpp"

namespace termchart {

class Arena {
public:
    explicit Arena(std::size_t initial_capacity = 64 * 1024);
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) = delete;
    Arena& operator=(Arena&&) = delete;

    // 取原始内存：不做构造。对齐必须是 2 的幂。
    TERMCHART_API void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t));

    // 原地构造。静态断言防止"持堆类型"混入（违反内存模型）。
    template <class T, class... Args>
    T* create(Args&&... args) {
        static_assert(std::is_trivially_destructible_v<T>,
                      "Arena 只接纳 trivially destructible 类型：持有堆资源的对象请改用普通容器");
        void* raw = allocate(sizeof(T), alignof(T));
        return ::new (raw) T(std::forward<Args>(args)...);
    }

    // 一帧结束：复用全部内存（不调用析构，因 T 必为 trivially destructible）。
    TERMCHART_API void reset() noexcept;

    [[nodiscard]] TERMCHART_API std::size_t used_bytes() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t capacity_bytes() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t block_count() const noexcept;

private:
    struct Block {
        std::unique_ptr<std::byte[]> data;
        std::size_t size = 0;
        std::size_t used = 0;
    };

    std::vector<Block> blocks_;
};

}  // namespace termchart

#endif  // TERMCHART_CORE_ARENA_HPP
