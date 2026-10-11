// golden_runner.cpp —— 属性级快照实现
#include "golden/golden_runner.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "termchart/core/color_depth.hpp"
#include "termchart/util/utf8.hpp"
#include "termchart/util/wcwidth.hpp"

namespace golden {
namespace {

struct StyleState {
    std::string fg = "none";
    std::string bg = "none";
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool reverse = false;
};

std::string attrs_of(const StyleState& state) {
    std::string out;
    auto append = [&out](std::string_view part) {
        if (!out.empty()) {
            out += "|";
        }
        out.append(part);
    };
    if (state.bold) {
        append("bold");
    }
    if (state.dim) {
        append("dim");
    }
    if (state.italic) {
        append("italic");
    }
    if (state.underline) {
        append("underline");
    }
    if (state.reverse) {
        append("reverse");
    }
    return out;
}

std::string hex2(int value) {
    std::ostringstream os;
    os << std::hex;
    os.width(2);
    os.fill('0');
    os << value;
    return os.str();
}

// 解析 SGR 参数流：参数按分号切分，逐个解释。
void apply_sgr(StyleState& state, std::string_view params) {
    std::vector<int> values;
    std::size_t pos = 0;
    while (pos <= params.size()) {
        const std::size_t sep = params.find(';', pos);
        const std::string_view token = params.substr(pos, sep == std::string_view::npos
                                                             ? std::string_view::npos
                                                             : sep - pos);
        int value = 0;
        for (const char ch : token) {
            if (ch >= '0' && ch <= '9') {
                value = value * 10 + (ch - '0');
            }
        }
        values.push_back(value);
        if (sep == std::string_view::npos) {
            break;
        }
        pos = sep + 1;
    }
    if (values.empty()) {
        values.push_back(0);
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        const int v = values[i];
        if (v == 0) {
            state = StyleState{};
            continue;
        }
        if (v == 1) {
            state.bold = true;
        } else if (v == 2) {
            state.dim = true;
        } else if (v == 3) {
            state.italic = true;
        } else if (v == 4) {
            state.underline = true;
        } else if (v == 7) {
            state.reverse = true;
        } else if (v == 22) {
            state.bold = false;
            state.dim = false;
        } else if (v == 23) {
            state.italic = false;
        } else if (v == 24) {
            state.underline = false;
        } else if (v == 27) {
            state.reverse = false;
        } else if (v == 39) {
            state.fg = "none";
        } else if (v == 49) {
            state.bg = "none";
        } else if (v >= 30 && v <= 37) {
            state.fg = "ansi16:" + std::to_string(v - 30);
        } else if (v >= 90 && v <= 97) {
            state.fg = "ansi16:" + std::to_string(v - 90 + 8);
        } else if (v >= 40 && v <= 47) {
            state.bg = "ansi16:" + std::to_string(v - 40);
        } else if (v >= 100 && v <= 107) {
            state.bg = "ansi16:" + std::to_string(v - 100 + 8);
        } else if ((v == 38 || v == 48) && i + 1 < values.size()) {
            const bool foreground = v == 38;
            const int mode = values[i + 1];
            if (mode == 5 && i + 2 < values.size()) {
                const std::string value = "indexed:" + std::to_string(values[i + 2]);
                if (foreground) {
                    state.fg = value;
                } else {
                    state.bg = value;
                }
                i += 2;
            } else if (mode == 2 && i + 4 < values.size()) {
                const std::string value = "#" + hex2(values[i + 2]) + hex2(values[i + 3]) +
                                          hex2(values[i + 4]);
                if (foreground) {
                    state.fg = value;
                } else {
                    state.bg = value;
                }
                i += 4;
            }
        }
    }
}

std::filesystem::path golden_root() {
    // 允许 TERMCHART_GOLDEN_DIR 覆盖（CI 把快照拷到别处时有用）。
    if (const char* env = std::getenv("TERMCHART_GOLDEN_DIR")) {
        return std::filesystem::path(env);
    }
    // 默认用编译期写死的源码树绝对路径：ctest 的工作目录是构建目录，
    // 相对路径 "tests/golden" 在那里根本不存在。
    return std::filesystem::path(TERMCHART_GOLDEN_DEFAULT_DIR);
}

}  // namespace

Props parse_ansi(std::string_view bytes) {
    Props props;
    StyleState state;
    int row = 0;
    int col = 0;
    std::size_t pos = 0;
    while (pos < bytes.size()) {
        const char ch = bytes[pos];
        if (ch == '\033') {
            // ESC[...X
            if (pos + 1 < bytes.size() && bytes[pos + 1] == '[') {
                std::size_t end = pos + 2;
                while (end < bytes.size() && bytes[end] != 'm' && bytes[end] != 'H') {
                    ++end;
                }
                const std::string_view params = bytes.substr(pos + 2, end - (pos + 2));
                if (end < bytes.size() && bytes[end] == 'm') {
                    apply_sgr(state, params);
                } else if (end < bytes.size() && bytes[end] == 'H') {
                    const std::size_t sep = params.find(';');
                    if (sep == std::string_view::npos) {
                        row = 0;
                        col = 0;
                    } else {
                        auto parse_int = [](std::string_view text) {
                            int value = 0;
                            for (const char c : text) {
                                if (c >= '0' && c <= '9') {
                                    value = value * 10 + (c - '0');
                                }
                            }
                            return value;
                        };
                        row = parse_int(params.substr(0, sep)) - 1;
                        col = parse_int(params.substr(sep + 1)) - 1;
                    }
                }
                pos = end + 1;
                continue;
            }
            ++pos;
            continue;
        }
        if (ch == '\r') {
            col = 0;
            ++pos;
            continue;
        }
        if (ch == '\n') {
            ++row;
            ++pos;
            continue;
        }
        const termchart::util::Utf8Decode decoded = termchart::util::decode(bytes, pos);
        if (decoded.consumed == 0) {
            ++pos;
            continue;
        }
        // 组合记号（宽度 0）不单独成格：挂到当前格上，这里直接跳过。
        if (termchart::util::codepoint_width(decoded.cp) > 0) {
            CellProps cell;
            cell.row = row;
            cell.col = col;
            cell.codepoint = static_cast<std::uint32_t>(decoded.cp);
            cell.fg = state.fg;
            cell.bg = state.bg;
            cell.attrs = attrs_of(state);
            props.cells.push_back(cell);
            col += termchart::util::codepoint_width(decoded.cp);
            props.width = std::max(props.width, col);
            props.height = std::max(props.height, row + 1);
        }
        pos += static_cast<std::size_t>(decoded.consumed);
    }
    return props;
}

std::string dump(const Props& props) {
    std::ostringstream os;
    os << "size: " << props.width << "x" << props.height << "\n";
    os << "cells: " << props.cells.size() << "\n";
    for (const CellProps& cell : props.cells) {
        os << "cell "
           << cell.row << " " << cell.col << " "
           << "U+" << std::hex << std::uppercase << cell.codepoint << std::dec << " "
           << "fg=" << (cell.fg.empty() ? std::string("none") : cell.fg) << " "
           << "bg=" << (cell.bg.empty() ? std::string("none") : cell.bg) << " "
           << "attrs=" << (cell.attrs.empty() ? std::string("-") : cell.attrs) << "\n";
    }
    return os.str();
}

void check_or_update(const std::string& subdir, const std::string& name,
                     const std::string& actual) {
    const std::filesystem::path path = golden_root() / subdir / (name + ".txt");
    const bool update = std::getenv("TERMCHART_UPDATE_GOLDEN") != nullptr;
    if (update) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << actual;
        return;
    }
    // 正常模式：文件必须存在（快照是契约，不是可选产物）。
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::ostringstream os;
        os << "golden 快照缺失: " << path.string()
           << " —— 确认改动确有必要时，运行 `python tools/gen_golden.py` 重新生成\n"
           << "---- 实际内容 ----\n" << actual;
        throw std::runtime_error(os.str());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (buffer.str() != actual) {
        std::ostringstream os;
        os << "golden 快照不一致: " << path.string() << "\n"
           << "---- 期望 ----\n" << buffer.str() << "\n"
           << "---- 实际 ----\n" << actual << "\n";
        throw std::runtime_error(os.str());
    }
}

}  // namespace golden
