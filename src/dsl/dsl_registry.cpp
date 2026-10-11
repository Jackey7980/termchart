#include "termchart/dsl/dsl_registry.hpp"

#include <algorithm>
#include <mutex>

#include "termchart/dsl/mermaid.hpp"
#include "termchart/dsl/mermaid_flow.hpp"

namespace termchart::dsl {
namespace {

std::once_flag g_builtin_flag;

char lower(char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

bool same_name(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lower(lhs[i]) != lower(rhs[i])) {
            return false;
        }
    }
    return true;
}

// mermaid 描述符的适配器：把注册表的 DslOptions 翻译成方言自己的选项。
Result<AstGraph> parse_mermaid_adapter(std::string_view source, const DslOptions& options) {
    MermaidOptions mermaid_options;
    mermaid_options.max_nodes = options.max_nodes;
    return parse_mermaid(source, mermaid_options);
}

Result<AstGraph> parse_flowchart_adapter(std::string_view source, const DslOptions& options) {
    FlowchartParseOptions flowchart_options;
    flowchart_options.max_nodes = options.max_nodes;
    return parse_flowchart(source, flowchart_options);
}

// 静态初始化期登记（与 plugin/loader.hpp 同一时机）；因为 instance() 是
// function-local static，此处首次触碰它是安全的。
struct BuiltinBootstrap {
    BuiltinBootstrap() { (void)register_builtin_dsls(); }
};

const BuiltinBootstrap g_bootstrap;

}  // namespace

DslRegistry& DslRegistry::instance() noexcept {
    static DslRegistry registry;
    return registry;
}

bool DslRegistry::add(const DslDescriptor& descriptor) noexcept {
    if (descriptor.parse == nullptr || descriptor.name.empty()) {
        return false;
    }
    for (const DslDescriptor& existing : entries_) {
        if (same_name(existing.name, descriptor.name)) {
            return false;
        }
    }
    entries_.push_back(descriptor);
    return true;
}

const DslDescriptor* DslRegistry::find(std::string_view name) const noexcept {
    for (const DslDescriptor& entry : entries_) {
        if (same_name(entry.name, name)) {
            return &entry;
        }
    }
    return nullptr;
}

Result<AstGraph> DslRegistry::parse(std::string_view name, std::string_view source,
                                    const DslOptions& options) const {
    const DslDescriptor* descriptor = find(name);
    if (descriptor == nullptr) {
        (void)register_builtin_dsls();  // 兜底：静态初始化期可能尚未跑
        descriptor = find(name);
    }
    if (descriptor == nullptr || descriptor->parse == nullptr) {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);
    }
    return descriptor->parse(source, options);
}

std::vector<std::string_view> DslRegistry::names() const {
    std::vector<std::string_view> out;
    out.reserve(entries_.size());
    for (const DslDescriptor& entry : entries_) {
        out.push_back(entry.name);
    }
    return out;
}

std::vector<DslDescriptor> DslRegistry::snapshot() const { return entries_; }

std::size_t DslRegistry::size() const noexcept { return entries_.size(); }

void DslRegistry::clear_for_test() noexcept { entries_.clear(); }

std::vector<DslDescriptor> builtin_descriptors() {
    std::vector<DslDescriptor> out;
    out.push_back(DslDescriptor{
        .name = "mermaid",
        .description = "Mermaid 文本（自动识别 flowchart / sequenceDiagram / classDiagram / stateDiagram）",
        .directive = "flowchart",
        .parse = &parse_mermaid_adapter,
    });
    out.push_back(DslDescriptor{
        .name = "flowchart",
        .description = "Mermaid flowchart 子集（graph/flowchart + 节点形状 + 边标签 + subgraph）",
        .directive = "flowchart TD",
        .parse = &parse_flowchart_adapter,
    });
    return out;
}

bool register_builtin_dsls() noexcept {
    bool ok = true;
    std::call_once(g_builtin_flag, [&ok] {
        for (const DslDescriptor& descriptor : builtin_descriptors()) {
            ok = DslRegistry::instance().add(descriptor) && ok;
        }
    });
    return ok;
}

}  // namespace termchart::dsl
