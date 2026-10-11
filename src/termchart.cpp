// termchart.cpp —— 伞形头对应的实现单元
//
// 伞形头本身不含实现，这里提供唯一一个"版本查询"符号：
// 它让链接期能验证"库和头文件版本是否同源"。
#include "termchart/termchart.hpp"

namespace termchart {

const char* version_string() noexcept { return TERMCHART_VERSION_STRING; }

}  // namespace termchart
