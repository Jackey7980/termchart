// backend.hpp —— 所有后端的抽象基类（begin/end 生命周期）
//
// 后端的铁律（见 ROADMAP 阶段 3 约束）：**不做跨渲染模型转换**。
// 输入是 CellBuffer、Scene 还是 Document，由子接口在类型上钉死：
//   * backend/snapshot/*    ← CellBuffer
//   * backend/vector/*      ← Scene
//   * backend/structured/*  ← Document
// 因此 backend 不需要认识 chart 类型（backend > chart 是禁止边）。
#ifndef TERMCHART_BACKEND_BACKEND_HPP
#define TERMCHART_BACKEND_BACKEND_HPP

#include <string_view>

#include "termchart/export.hpp"

namespace termchart::backend {

class Backend {
public:
    virtual ~Backend();

    // 生命周期：begin 清空内部缓冲，end 收口（补尾部字节）。二者可重入。
    virtual void begin() = 0;
    virtual void end() = 0;

    // 结果视图；调用方不得保存跨越下一次 begin/render 的视图。
    [[nodiscard]] virtual std::string_view bytes() const = 0;
};

}  // namespace termchart::backend

#endif  // TERMCHART_BACKEND_BACKEND_HPP
