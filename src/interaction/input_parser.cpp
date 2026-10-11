// input_parser.cpp —— 输入字节流状态机
#include "termchart/interaction/input_parser.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/interaction/key.hpp"
#include "termchart/interaction/mouse.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::interaction {

namespace {

constexpr char kEsc = '\x1b';
constexpr char kBel = '\x07';

bool is_csi_final(char c) noexcept { return c >= 0x40 && c <= 0x7E; }

bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

// 从 body 的 i 处读一个十进制数（无数字返回 0，i 不前进）。
int read_int(std::string_view body, std::size_t& i) noexcept {
    int value = 0;
    bool any = false;
    while (i < body.size() && is_digit(body[i])) {
        value = value * 10 + (body[i] - '0');
        ++i;
        any = true;
    }
    (void)any;
    return value;
}

MouseButton button_from(int code) noexcept {
    switch (code) {
        case 0:
            return MouseButton::Left;
        case 1:
            return MouseButton::Middle;
        case 2:
            return MouseButton::Right;
        default:
            return MouseButton::None;
    }
}

// CSI 主体 + 终止字节 → Event。
bool csi_event(std::string_view body, char final_ch, Event& out) noexcept {
    switch (final_ch) {
        case 'A':
            out = KeyEvent{Key::Up};
            return true;
        case 'B':
            out = KeyEvent{Key::Down};
            return true;
        case 'C':
            out = KeyEvent{Key::Right};
            return true;
        case 'D':
            out = KeyEvent{Key::Left};
            return true;
        case 'H':
            out = KeyEvent{Key::Home};
            return true;
        case 'F':
            out = KeyEvent{Key::End};
            return true;
        case 'Z':
            out = KeyEvent{Key::Tab, 0, mod::kShift};
            return true;
        case '~': {
            std::size_t i = 0;
            const int n = read_int(body, i);
            Key key = Key::Unknown;
            switch (n) {
                case 1:
                    key = Key::Home;
                    break;
                case 2:
                    key = Key::Insert;
                    break;
                case 3:
                    key = Key::Delete;
                    break;
                case 4:
                    key = Key::End;
                    break;
                case 5:
                    key = Key::PageUp;
                    break;
                case 6:
                    key = Key::PageDown;
                    break;
                case 11:
                    key = Key::F1;
                    break;
                case 12:
                    key = Key::F2;
                    break;
                case 13:
                    key = Key::F3;
                    break;
                case 14:
                    key = Key::F4;
                    break;
                case 15:
                    key = Key::F5;
                    break;
                case 17:
                    key = Key::F6;
                    break;
                case 18:
                    key = Key::F7;
                    break;
                case 19:
                    key = Key::F8;
                    break;
                case 20:
                    key = Key::F9;
                    break;
                case 21:
                    key = Key::F10;
                    break;
                case 23:
                    key = Key::F11;
                    break;
                case 24:
                    key = Key::F12;
                    break;
                default:
                    return false;
            }
            out = KeyEvent{key};
            return true;
        }
        case 'M':
        case 'm': {
            if (body.empty() || body[0] != '<') {
                return false;
            }
            std::size_t i = 1;
            const int b = read_int(body, i);
            if (i >= body.size() || body[i] != ';') {
                return false;
            }
            ++i;
            const int x = read_int(body, i);
            if (i >= body.size() || body[i] != ';') {
                return false;
            }
            ++i;
            const int y = read_int(body, i);

            MouseEvent me;
            me.x = x;
            me.y = y;
            if ((b & 4) != 0) {
                me.mods |= mod::kShift;
            }
            if ((b & 8) != 0) {
                me.mods |= mod::kAlt;
            }
            if ((b & 16) != 0) {
                me.mods |= mod::kCtrl;
            }
            if ((b & 64) != 0) {
                me.action = ((b & 1) != 0) ? MouseAction::ScrollDown : MouseAction::ScrollUp;
                me.button = MouseButton::None;
            } else if ((b & 32) != 0) {
                me.action = MouseAction::Move;
                me.button = button_from(b & 3);
            } else if (final_ch == 'M') {
                me.action = MouseAction::Press;
                me.button = button_from(b & 3);
            } else {
                me.action = MouseAction::Release;
                me.button = button_from(b & 3);
            }
            out = me;
            return true;
        }
        default:
            return false;  // 未知 CSI：消费但不出事件
    }
}

// SS3（ESC O x）：F1–F4 与方向键的另一套编码。
bool ss3_event(char c, Event& out) noexcept {
    switch (c) {
        case 'P':
            out = KeyEvent{Key::F1};
            return true;
        case 'Q':
            out = KeyEvent{Key::F2};
            return true;
        case 'R':
            out = KeyEvent{Key::F3};
            return true;
        case 'S':
            out = KeyEvent{Key::F4};
            return true;
        case 'A':
            out = KeyEvent{Key::Up};
            return true;
        case 'B':
            out = KeyEvent{Key::Down};
            return true;
        case 'C':
            out = KeyEvent{Key::Right};
            return true;
        case 'D':
            out = KeyEvent{Key::Left};
            return true;
        case 'H':
            out = KeyEvent{Key::Home};
            return true;
        case 'F':
            out = KeyEvent{Key::End};
            return true;
        default:
            return false;
    }
}

}  // namespace

std::vector<Event> InputParser::feed(std::string_view bytes) {
    buffer_.append(bytes);
    std::vector<Event> events;
    std::size_t pos = 0;

    while (pos < buffer_.size()) {
        const auto b = static_cast<unsigned char>(buffer_[pos]);
        Event ev{};
        std::size_t consumed = 0;
        bool produced = false;

        if (b == kEsc) {
            if (pos + 1 >= buffer_.size()) {
                break;  // 不完整：等 flush() 或更多字节
            }
            const char n1 = buffer_[pos + 1];
            if (n1 == '[') {
                std::size_t end = pos + 2;
                while (end < buffer_.size() && !is_csi_final(buffer_[end])) {
                    ++end;
                }
                if (end >= buffer_.size()) {
                    break;  // 不完整
                }
                const std::string_view body(buffer_.data() + pos + 2, end - (pos + 2));
                produced = csi_event(body, buffer_[end], ev);
                consumed = end - pos + 1;
            } else if (n1 == 'O') {
                if (pos + 2 >= buffer_.size()) {
                    break;  // 不完整
                }
                produced = ss3_event(buffer_[pos + 2], ev);
                consumed = 3;
            } else if (n1 == ']') {
                // OSC：吃到 BEL 或 ESC \ 为止；内容不入事件流。
                std::size_t end = pos + 2;
                bool closed = false;
                while (end < buffer_.size()) {
                    if (buffer_[end] == kBel) {
                        ++end;
                        closed = true;
                        break;
                    }
                    if (buffer_[end] == kEsc && end + 1 < buffer_.size() &&
                        buffer_[end + 1] == '\\') {
                        end += 2;
                        closed = true;
                        break;
                    }
                    ++end;
                }
                if (!closed) {
                    break;  // 不完整
                }
                consumed = end - pos;
            } else {
                // Alt + <字符>（按 UTF-8 解码）
                const util::Utf8Decode dec = util::decode(buffer_, pos + 1);
                if (dec.consumed == 0 || dec.incomplete) {
                    break;
                }
                if (dec.ok) {
                    KeyEvent ke;
                    ke.key = Key::Char;
                    ke.ch = dec.cp;
                    ke.mods = mod::kAlt;
                    ev = ke;
                    produced = true;
                }
                consumed = 1 + static_cast<std::size_t>(dec.consumed);
            }
        } else if (b == 0x0D || b == 0x0A) {
            ev = KeyEvent{Key::Enter};
            produced = true;
            consumed = 1;
        } else if (b == 0x09) {
            ev = KeyEvent{Key::Tab};
            produced = true;
            consumed = 1;
        } else if (b == 0x7F) {
            ev = KeyEvent{Key::Backspace};
            produced = true;
            consumed = 1;
        } else if (b == 0x00) {
            consumed = 1;  // NUL：忽略
        } else if (b >= 0x1C && b < 0x20) {
            consumed = 1;  // FS/GS/RS/US 等其余控制字符：忽略，不成事件
        } else if (b >= 0x01 && b <= 0x1A) {
            KeyEvent ke;  // Ctrl + 字母（0x09/0x0D/0x1B 已在上方处理）
            ke.key = Key::Char;
            ke.ch = static_cast<char32_t>(U'a' + (b - 1));
            ke.mods = mod::kCtrl;
            ev = ke;
            produced = true;
            consumed = 1;
        } else {
            const util::Utf8Decode dec = util::decode(buffer_, pos);
            if (dec.consumed == 0 || dec.incomplete) {
                break;  // 等更多字节
            }
            if (dec.ok) {
                KeyEvent ke;
                ke.key = Key::Char;
                ke.ch = dec.cp;
                ev = ke;
                produced = true;
            }
            consumed = static_cast<std::size_t>(dec.consumed);
        }

        if (consumed == 0) {
            break;
        }
        pos += consumed;
        if (produced) {
            events.push_back(ev);
        }
    }

    buffer_.erase(0, pos);
    return events;
}

std::vector<Event> InputParser::flush() {
    std::vector<Event> events;
    if (buffer_.size() == 1 && buffer_[0] == kEsc) {
        events.push_back(KeyEvent{Key::Escape});
        buffer_.clear();
    }
    return events;
}

std::size_t InputParser::pending() const noexcept { return buffer_.size(); }

void InputParser::reset() { buffer_.clear(); }

}  // namespace termchart::interaction
