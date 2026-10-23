// options.cpp —— 管道枚举的名字（诊断与 --help 用）
#include "termchart/autoviz/options.hpp"

namespace termchart::autoviz {

const char* pipeline_name(PipelineKind kind) noexcept {
    switch (kind) {
    case PipelineKind::Time:            return "time";
    case PipelineKind::Geo:             return "geo";
    case PipelineKind::Event:           return "event";
    case PipelineKind::CategoryNumeric: return "category_numeric";
    case PipelineKind::Text:            return "text";
    case PipelineKind::Numeric:         return "numeric";
    case PipelineKind::Preview:         return "preview";
    }
    return "?";
}

}  // namespace termchart::autoviz
