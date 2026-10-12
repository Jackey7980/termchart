# 内存模型与 arena 约束

## 所有权总则

- `core::CellBuffer` 是全库中枢，**持有堆资源**（连续 `vector<Cell>` 网格 +
  稀疏侧面表 `unordered_map<index, array<char32_t, N>>`）。它**禁止进
  `core/arena.hpp` 的帧分配器**。
- 对外暴露 `std::span<Cell>`：调用方不得保存跨越 `resize` 的 span。
- 对外接口普遍用 PIMPL（`render::Renderer` 持有 `Impl*`；`ABI_POLICY.md` 只承诺
  PIMPL 门面稳定）。

## arena（帧分配器）

`core/arena.hpp` 提供帧分配器：生命周期与"一帧"绑定的临时分配（布局计算、
路由探索等大量短命对象）。约束：

- 只放**无析构语义**的临时数据；
- 持有堆资源的对象（CellBuffer、Scene 的文本、Registry）不进去；
- 帧 arena 在帧末整体作废，不逐对象释放。

## 值语义优先

- `chart::Series`（name + color + points）是值类型 header-only；
- `chart::Dataset` 持有系列集合，可拷贝，`series()` 提供可变访问；
- `image::Bitmap` 纯值类型（列入 header_only）；
- `scene::Command` 是聚合体 variant，Scene 可整体拷贝/比较。

## 生成物的内存形态

`detail/*_tables.hpp`（生成物）是 constexpr 区间表：编译期固化、零运行时初始化。
区间「升序且互不重叠」是结构不变量（`tests/unit/test_layering.cpp` 守护），
二分查找的正确性依赖它。

## 已知约束与陷阱

- graph 的 `add_node` 返回 vector 内元素引用，后续插入可能使其失效——长期持有用
  `Node::id()` + `Graph::find()`；
- 后台线程与主循环的内存交接走投递队列（concurrency.md），不共享可变缓冲。
