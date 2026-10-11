#include "termchart/core/arena.hpp"

#include <algorithm>
#include <cstdlib>

namespace termchart {
namespace {

std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
    const std::size_t remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
}

}  // namespace

Arena::Arena(std::size_t initial_capacity) {
    if (initial_capacity == 0) {
        return;
    }
    Block block;
    block.data = std::make_unique<std::byte[]>(initial_capacity);
    block.size = initial_capacity;
    blocks_.push_back(std::move(block));
}

Arena::~Arena() = default;

void* Arena::allocate(std::size_t bytes, std::size_t alignment) {
    if (bytes == 0) {
        return nullptr;
    }
    if (alignment == 0) {
        alignment = alignof(std::max_align_t);
    }
    for (Block& b : blocks_) {
        const std::size_t cursor = align_up(b.used, alignment);
        if (cursor + bytes <= b.size) {
            b.used = cursor + bytes;
            return b.data.get() + cursor;
        }
    }
    // 现有块都不够：开新块（几何扩张，避免逐帧抖动）。
    std::size_t new_size = std::max<std::size_t>(bytes + alignment, 64u * 1024u);
    if (!blocks_.empty()) {
        new_size = std::max<std::size_t>(new_size, blocks_.back().size * 2);
    }
    Block block;
    block.data = std::make_unique<std::byte[]>(new_size);
    block.size = new_size;
    const std::size_t cursor = align_up(0, alignment);
    // 主张新块必然足够：几何扩张已保证。
    block.used = cursor + bytes;
    std::byte* ptr = block.data.get() + cursor;
    blocks_.push_back(std::move(block));
    return ptr;
}

void Arena::reset() noexcept {
    for (Block& b : blocks_) {
        b.used = 0;
    }
}

std::size_t Arena::used_bytes() const noexcept {
    std::size_t total = 0;
    for (const Block& b : blocks_) {
        total += b.used;
    }
    return total;
}

std::size_t Arena::capacity_bytes() const noexcept {
    std::size_t total = 0;
    for (const Block& b : blocks_) {
        total += b.size;
    }
    return total;
}

std::size_t Arena::block_count() const noexcept { return blocks_.size(); }

}  // namespace termchart
