# ABI Policy / 二进制接口稳定承诺

状态：**草案**，随首个共享库发布（v0.1 之后）生效。

## 承诺范围

termchart 对**共享库（.so / .dll）构建**承诺：**PIMPL 门面稳定**。

- 每个跨库边界的公开类型都走 PIMPL（`Impl*` 持有全部数据与逻辑），
  例如 `render::Renderer`；
- 在同一 soname 内，公开类的**头文件布局不变**（只有指针成员），
  私有实现的增删改不破坏已编译的调用方；
- `include/termchart/**` 顶层与各模块公开头里的**函数签名**在同一 soname 内
  不做破坏性修改：不加纯虚函数、不改既有函数签名、不删除公开符号；
- C++ 标准库类型出现在签名中时，仅限 `std::string` / `std::string_view` /
  `std::span` / `std::vector` / `std::optional` / `std::expected` 语义子集
  （本库自带 `core/expected.hpp`），不引入自定义类型穿透 ABI。

## 版本与符号

- 符号可见性统一走 `include/termchart/export.hpp` 的 `TERMCHART_API`；
  标注了 `TERMCHART_ABI_STABLE` 的声明属于本承诺覆盖的面（该宏当前仅作注释性标注）；
- soname 与版本号由 `cmake/TermchartAbiVersion.cmake` 管理；
- 静态构建（`TERMCHART_STATIC`）**不受本文件约束**——静态链接下调用方
  与库一起重编，任何头文件改动都随重编自然生效。

## 明确不承诺

- **header-only 使用方式**不承诺 ABI 稳定：公开头属于源码接口，
  任何版本间都可以增删（变更记录见 `CHANGELOG.md`）；
- `include/termchart/detail/**`（生成的数据表）与 `src/` 下的一切内部结构
  随时可以变动，且无通知义务；
- 布局相关类（`core::Cell`、`core::CellBuffer` 等 POD/容器）的内存布局
  只在「同一份头文件编译」的前提下有意义，跨版本布局不承诺兼容；
- 虚函数表顺序、异常（本库不抛异常，`core::expected.hpp` 的 `value()`
  在契约破坏时抛 `std::logic_error` 属调用方 bug，见 `ERROR_POLICY.md`）。

## 破坏性变更的流程

1. 先在 `ROADMAP.md` 修订记录登记变更与影响面；
2. soname 随 `cmake/TermchartAbiVersion.cmake` 升级；
3. `CHANGELOG.md` 的 `Changed` / `Removed` 小节记录迁移说明。
