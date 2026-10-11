// span.hpp —— 带样式的文本片段（Document IR 的叶）
//
// Span 是"语义文本 + 样式"，不含像素/位置信息：位置由后端或上层决定。
// header-only 值类型。
#ifndef TERMCHART_DOCUMENT_SPAN_HPP
#define TERMCHART_DOCUMENT_SPAN_HPP

#include <string>

#include "termchart/core/style.hpp"

namespace termchart::document {

struct Span {
    std::string text;
    Style style;

    Span() = default;
    explicit Span(std::string t, Style s = Style{}) : text(std::move(t)), style(s) {}
};

}  // namespace termchart::document

#endif  // TERMCHART_DOCUMENT_SPAN_HPP
