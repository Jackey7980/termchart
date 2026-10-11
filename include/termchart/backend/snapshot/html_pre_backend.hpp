// backend/snapshot/html_pre_backend.hpp —— CellBuffer → <pre> 包装的 HTML
//
// 只做最小转义（& < >），颜色/属性留白——需要彩色 HTML 用 backend/structured。
#ifndef TERMCHART_BACKEND_SNAPSHOT_HTML_PRE_BACKEND_HPP
#define TERMCHART_BACKEND_SNAPSHOT_HTML_PRE_BACKEND_HPP

#include "termchart/backend/snapshot/backend.hpp"

namespace termchart::backend::snapshot {

class HtmlPreBackend final : public SnapshotBackend {
public:
    void render(const CellBuffer& buffer) override;
};

}  // namespace termchart::backend::snapshot

#endif  // TERMCHART_BACKEND_SNAPSHOT_HTML_PRE_BACKEND_HPP
