// attributes.hpp —— 节点/边的键值元数据（header-only 值类型）
//
// 图元数据是开放的（label、color、shape、rank、weight 等由各层约定），因此
// 这里用字符串键值袋而不是固定字段：新增元数据不需要改图的数据结构。
// 用透明比较器，查找可直接接受 std::string_view，避免临时构造 std::string。
#ifndef TERMCHART_GRAPH_ATTRIBUTES_HPP
#define TERMCHART_GRAPH_ATTRIBUTES_HPP

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace termchart::graph {

class Attributes {
public:
    void set(std::string key, std::string value) { values_[std::move(key)] = std::move(value); }

    [[nodiscard]] bool has(std::string_view key) const {
        return values_.find(key) != values_.end();
    }

    // 缺失时返回 fallback（默认空串），不抛异常。
    [[nodiscard]] std::string_view get(std::string_view key,
                                       std::string_view fallback = {}) const {
        const auto it = values_.find(key);
        return it == values_.end() ? fallback : std::string_view(it->second);
    }

    bool erase(std::string_view key) { return values_.erase(std::string(key)) != 0; }
    void clear() { values_.clear(); }

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
    [[nodiscard]] bool empty() const noexcept { return values_.empty(); }

private:
    std::map<std::string, std::string, std::less<>> values_;
};

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_ATTRIBUTES_HPP
