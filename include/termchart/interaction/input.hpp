// input.hpp —— 原始输入字节缓冲（攒字节，解析交给 InputParser）
#ifndef TERMCHART_INTERACTION_INPUT_HPP
#define TERMCHART_INTERACTION_INPUT_HPP

#include <cstddef>
#include <string>
#include <string_view>

#include "termchart/export.hpp"

namespace termchart::interaction {

class Input {
public:
    TERMCHART_API void push(std::string_view bytes);
    TERMCHART_API void push(char byte);

    [[nodiscard]] TERMCHART_API std::string_view pending() const noexcept;
    [[nodiscard]] TERMCHART_API bool empty() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t size() const noexcept;
    TERMCHART_API void clear();

private:
    std::string buffer_;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_INPUT_HPP
