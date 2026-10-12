# 写自定义光栅器

## 契约

继承 `raster::Rasterizer`（raster/rasterizer.hpp）：

```cpp
class MyRasterizer final : public termchart::raster::Rasterizer {
public:
    [[nodiscard]] termchart::CellBuffer rasterize(
        const termchart::image::FloatField& field,
        const termchart::raster::RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override;
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override;
    [[nodiscard]] termchart::raster::RasterKind kind() const noexcept override;
    [[nodiscard]] const char* name() const noexcept override;
};
```

## 不变量（基类注释钉死）

- `rasterize` 无副作用、不抛异常、不写 stdout；
- 输出 `CellBuffer` 尺寸 = `field / (subpixel_w, subpixel_h)`；
- 单色语义：点亮处 `opts.fg`、熄灭处 `opts.bg`；`opts.invert` 交换二者并反相；
  `opts.threshold`（默认 0.5）决定二进制子像素的开/关。

## 现有实现（照着抄风格）

| 类 | 子像素 | 特点 |
| --- | --- | --- |
| `BrailleRasterizer` | 2×4 | 盲文点阵（空白是 U+2800 不是空格！） |
| `HalfBlockRasterizer` | 1×2 | 上下双色（▀），唯一能表达彩色的 |
| `QuadrantRasterizer` | 2×2 | 四分块字符 |
| `AsciiRasterizer` | 1×1 | 密度字符 |
| `BlockRasterizer` | 1×1 | 八级竖条 |

`kind()` 返回 `RasterKind` 枚举；若你的光栅器不属于现有五类，
需要先给 `RasterKind` 加枚举值（枚举在基类头里，与 kind 一一对应）。

## 注册与自动选择

- `raster::RasterRegistry`：注册实例（构造期，见 design/registry.md）；
- `raster/auto_select.hpp`：`RasterCaps` + 自动选择逻辑——按能力挑最优光栅器；
- 一次性入口：`raster::rasterize(field, kind, opts)`。

## 输入：FloatField

`image/field.hpp` 的 `FloatField` 是标量强度场（宽/高 + 值域归一化约定）。
不是图像（RGB）——彩色位图走 `protocol/`，不走光栅器。

## 验证

- 新实现加单元测试：给定已知 field，断言输出格的字符与颜色；
- 三组可扩展性契约之一就是 raster：`tests/integration/replace_raster.cpp`
  证明新增光栅器核心零改动；
- 新文件照常登记（src 同名 cpp、CMake、测试），跑 `check_all`。
