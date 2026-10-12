# 最小构建

## 开关

```sh
cmake -DTERMCHART_MINIMAL_BUILD=ON
```

只保留 **core / util / color / term / render / api** 六个模块
（开关定义在 `cmake/TermchartOptions.cmake`）。

## 得到什么

- 终端上色闭环：`CellBuffer` → `Renderer` → 字节流 → stdout；
- `api/quick.hpp` 的一行出图接口；
- 差分重绘、SGR 生成、能力探测（term/caps）。

## 失去什么

- chart/graph/layout/scene/canvas：不能画"图表"，只能画样式化文本与字符格；
- image/protocol：不能显示位图；
- backend/document/dsl：不能导出与解析。

## 可选依赖

libpng / zlib / utf8proc 默认 OFF；缺失只降级不失败——
例如无 libpng 时 PNG 解码不可用，但库构建不受影响。

## 为什么有这个模式

分层规则（design/layering.md）保证低层模块永远不知道高层存在，
所以裁剪高层不需要改任何低层代码——最小构建是分层的直接受益者，
也是分层的回归探针（裁剪版编译失败 = 有人引入了向下依赖）。

## 验证

- 最小配置下跑 `ctest --preset ninja`（套件随裁剪收窄）；
- `check_all` 照常可跑（分层 + 每头独立编译）。
