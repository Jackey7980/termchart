// manifest.hpp —— 插件元数据
//
// 纯值类型：可拷贝、可比较。注册表快照（App 构造期冻结）就是一次深拷贝，
// 因此 Manifest 不持有任何句柄或引用。
#ifndef TERMCHART_PLUGIN_MANIFEST_HPP
#define TERMCHART_PLUGIN_MANIFEST_HPP

#include <string>

#include "termchart/export.hpp"

namespace termchart::plugin {

// name 是注册键（非空，比较不区分大小写）；version / author / description
// 是给人看的（日志、--list-plugins），允许为空。
struct Manifest {
    std::string name;
    std::string version = "0.0.0";
    std::string author;
    std::string description;
};

// 最小约束：name 非空即合法。version 等字段不做格式校验——宽进严出，
// 格式问题是展示层的事，不该在加载路径上拦人。
[[nodiscard]] TERMCHART_API bool manifest_valid(const Manifest& manifest) noexcept;

// 单行人读描述："name@version — description"。
[[nodiscard]] TERMCHART_API std::string describe(const Manifest& manifest);

}  // namespace termchart::plugin

#endif  // TERMCHART_PLUGIN_MANIFEST_HPP
