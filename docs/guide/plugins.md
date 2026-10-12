# 插件开发

## 现状

`plugin/` 模块目录在 `include/termchart/` 下**尚不存在**（结构.md 规划中，
未落地）。本文记录的是"插件点已经存在的位置"——注册表机制已落地，
插件系统是它之上的薄层。

## 现有扩展点（都已可用）

| 扩展点 | 注册表 | 头文件 |
| --- | --- | --- |
| 自定义图表 | `chart::ChartRegistry` | chart/registry.hpp |
| 自定义光栅器 | `raster::RasterRegistry` | raster/registry.hpp |
| 自定义比例尺 | `scale::ScaleRegistry` | scale/registry.hpp |
| 自定义后端 | 直接实现 Backend 子接口 | backend/*.hpp |
| DSL 方言 | `dsl_registry.hpp` | dsl/dsl_registry.hpp |

各注册表同一契约：`instance()` function-local static、构造期冻结快照、
注册工厂而非原型（见 design/registry.md）。

## 写一个"插件"（现状做法）

1. 实现扩展点接口（如继承 `chart::Chart`）；
2. **在 App/主程序构造前**注册：
   ```cpp
   termchart::chart::ChartRegistry::instance().register_chart(
       "my_chart", [] { return std::make_unique<MyChart>(); });
   ```
3. 之后 `create("my_chart")` 即可拿到实例。

## 硬约束

- **运行期注册无效**：`App` 构造时冻结浅拷贝快照，之后注册不生效；
- 注册表构造后只读，无注销 API；
- 可扩展性契约测试（tests/integration/replace_*.cpp）保证：新增实现 + 注册，
  核心零改动。

## 未做

- 动态库加载、生命周期管理、插件元数据/依赖声明等"插件系统"语义；
- `App` 运行期插件 API（等 app/ 模块落地）。
