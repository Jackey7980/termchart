// input.cpp —— 输入字节缓冲
#include "termchart/interaction/input.hpp"

namespace termchart::interaction {

void Input::push(std::string_view bytes) { buffer_.append(bytes); }

void Input::push(char byte) { buffer_.push_back(byte); }

std::string_view Input::pending() const noexcept { return buffer_; }

bool Input::empty() const noexcept { return buffer_.empty(); }

std::size_t Input::size() const noexcept { return buffer_.size(); }

void Input::clear() { buffer_.clear(); }

}  // namespace termchart::interaction
