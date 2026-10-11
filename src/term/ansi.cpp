#include "termchart/term/ansi.hpp"

#include <string>

#include "termchart/detail/ansi_tables.hpp"
#include "termchart/util/format.hpp"

namespace termchart::term::ansi {
namespace {

void append_csi(std::string& out) noexcept { out.append(detail::kCsiPrefix); }

void append_number(std::string& out, int value) noexcept {
    out.append(util::format_int(static_cast<std::int64_t>(value)));
}

// CSI 参数：n <= 1 时省略（"\x1b[H" 比 "\x1b[1;1H" 短，差分渲染在乎这几个字节）。
void append_count(std::string& out, int n) noexcept {
    if (n > 1) {
        append_number(out, n);
    }
}

}  // namespace

void append_cursor_to(std::string& out, int row, int col) noexcept {
    append_csi(out);
    append_number(out, row);
    out.push_back(';');
    append_number(out, col);
    out.push_back('H');
}

void append_cursor_up(std::string& out, int n) noexcept {
    append_csi(out);
    append_count(out, n);
    out.push_back('A');
}

void append_cursor_down(std::string& out, int n) noexcept {
    append_csi(out);
    append_count(out, n);
    out.push_back('B');
}

void append_cursor_forward(std::string& out, int n) noexcept {
    append_csi(out);
    append_count(out, n);
    out.push_back('C');
}

void append_cursor_back(std::string& out, int n) noexcept {
    append_csi(out);
    append_count(out, n);
    out.push_back('D');
}

void append_cursor_home(std::string& out) noexcept {
    append_csi(out);
    out.push_back('H');
}

std::string cursor_to(int row, int col) noexcept {
    std::string out;
    append_cursor_to(out, row, col);
    return out;
}

std::string cursor_home() noexcept {
    std::string out;
    append_cursor_home(out);
    return out;
}

void append_erase_to_end_of_line(std::string& out) noexcept {
    append_csi(out);
    out.push_back('K');
}

void append_erase_line(std::string& out) noexcept {
    append_csi(out);
    out.append("2K");
}

void append_erase_display(std::string& out) noexcept {
    append_csi(out);
    out.append("2J");
}

std::string erase_line() noexcept {
    std::string out;
    append_erase_line(out);
    return out;
}

std::string hide_cursor() noexcept { return std::string(detail::kCsiPrefix) + "?25l"; }
std::string show_cursor() noexcept { return std::string(detail::kCsiPrefix) + "?25h"; }
std::string enter_alt_screen() noexcept { return std::string(detail::kCsiPrefix) + "?1049h"; }
std::string exit_alt_screen() noexcept { return std::string(detail::kCsiPrefix) + "?1049l"; }

void append_move(std::string& out, int from_row, int from_col, int to_row, int to_col) noexcept {
    if (to_row == from_row && to_col == from_col) {
        return;  // 已在目标处：不产生任何字节
    }
    if (to_row == from_row && to_col > from_col && to_col - from_col <= 8) {
        append_cursor_forward(out, to_col - from_col);
        return;
    }
    if (to_row == from_row && to_col < from_col && from_col - to_col <= 8) {
        append_cursor_back(out, from_col - to_col);
        return;
    }
    if (to_col == 1 && to_row > from_row && to_row - from_row <= 8) {
        append_cursor_down(out, to_row - from_row);
        out.push_back('\r');
        return;
    }
    if (to_col == 1 && to_row < from_row && from_row - to_row <= 8) {
        append_cursor_up(out, from_row - to_row);
        out.push_back('\r');
        return;
    }
    append_cursor_to(out, to_row, to_col);
}

}  // namespace termchart::term::ansi
