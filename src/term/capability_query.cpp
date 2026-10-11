// capability_query.cpp —— DA1 / DA2 序列与回复解析
#include "termchart/term/capability_query.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace termchart::term {
namespace {

constexpr int kFeatureSixel = 4;
constexpr int kFeatureWindowOps = 18;
constexpr int kFeatureAnsiColor = 22;
constexpr int kFeatureRectangularOps = 28;

bool has_param(const DeviceAttributes& attributes, int value) noexcept {
    for (const int param : attributes.params) {
        if (param == value) {
            return true;
        }
    }
    return attributes.device_class == value;
}

}  // namespace

std::string query_da1() {
    return "\x1b[c";
}

std::string query_da2() {
    return "\x1b[>c";
}

bool parse_device_attributes(std::string_view reply, DeviceAttributes& out) noexcept {
    std::size_t at = 0;
    if (at < reply.size() && reply[at] == '\x1b') {
        ++at;  // 容忍带上 ESC 的完整序列
    }
    if (at >= reply.size() || reply[at] != '[') {
        return false;
    }
    ++at;

    bool secondary = false;
    if (at < reply.size() && (reply[at] == '?' || reply[at] == '>')) {
        secondary = reply[at] == '>';
        ++at;
    }

    std::vector<int> values;
    int current = 0;
    bool has_digits = false;
    for (; at < reply.size(); ++at) {
        const char ch = reply[at];
        if (ch >= '0' && ch <= '9') {
            current = current * 10 + (ch - '0');
            has_digits = true;
            continue;
        }
        if (ch == ';') {
            if (!has_digits) {
                return false;  // 连续分隔符 / 空参数
            }
            values.push_back(current);
            current = 0;
            has_digits = false;
            continue;
        }
        if (ch == 'c' || ch == 'C') {
            if (!has_digits) {
                return false;
            }
            values.push_back(current);
            if (at + 1 != reply.size()) {
                return false;  // 终止符之后不该还有内容
            }
            DeviceAttributes parsed;
            parsed.valid = true;
            parsed.secondary = secondary;
            parsed.device_class = values.front();
            parsed.params.assign(values.begin() + 1, values.end());
            out = std::move(parsed);
            return true;
        }
        return false;  // 出现既不是数字也不是分隔符/终止符的字符
    }
    return false;  // 没等到终止符
}

Da1Features da1_features(const DeviceAttributes& attributes) noexcept {
    Da1Features features;
    if (!attributes.valid) {
        return features;
    }
    features.sixel = has_param(attributes, kFeatureSixel);
    features.window_ops = has_param(attributes, kFeatureWindowOps);
    features.ansi_color = has_param(attributes, kFeatureAnsiColor);
    features.rectangular_ops = has_param(attributes, kFeatureRectangularOps);
    return features;
}

}  // namespace termchart::term
