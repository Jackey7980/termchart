# ABI 策略

## 承诺范围

`ABI_POLICY.md`（仓库根）是权威文件；本篇是摘要。承诺对象是 **PIMPL 门面**：

- 对外类的内存布局只含 `Impl*`（如 `render::Renderer`），
  实现细节变化不破坏二进制兼容；
- 头文件里的 inline 简单访问器（如 `Renderer::depth()`）属于承诺面；
- 值类型（`core::Point/Rect/Style`、`chart::Series`、`image::Bitmap` 等 header-only
  类型）按其头文件当前布局承诺。

## 符号可见性

统一走 `export.hpp` 的 `TERMCHART_API`；静态构建时 `TERMCHART_STATIC` 是 PUBLIC 定义。
公开 API 一律打标，内部符号不导出。

## 兼容性实践

- PIMPL 类的新增功能加在 Impl 内或新增非虚成员函数，不改虚表；
- 基类（`Chart`、`Backend`、`Rasterizer`、`Scale`、`Protocol`）有同名 src cpp
  放虚表锚点——基类虚表位置稳定是跨 TU 继承的前提；
- 弃用走 `deprecate.hpp` 的标注宏，先标弃用、后移除，不移除前保持行为。

## 未承诺

- `detail/` 与 `src/` 内部的一切；
- 生成物内部布局（`detail/*_tables.hpp`）；
- 构造期注册表的具体容器类型。
