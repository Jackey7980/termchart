# 注册表模型

## 契约

六张注册表共用同一套契约（`docs/design/registry.md` 的口径即此）：

| 注册表 | 头文件 | 注册内容 |
| --- | --- | --- |
| `chart::ChartRegistry` | chart/registry.hpp | 图表工厂 `std::function<std::unique_ptr<Chart>()>` |
| `raster::RasterRegistry` | raster/registry.hpp | 光栅器（按 `RasterKind`） |
| `scale::ScaleRegistry` | scale/registry.hpp | 比例尺 |
| `plugin::PluginRegistry` | plugin/plugin.hpp | 插件（Manifest + 工厂） |
| `dsl::DslRegistry` | dsl/dsl_registry.hpp | 方言描述符（含解析函数指针） |
| `network::tls::TlsRegistry` | network/tls/registry.hpp | TLS 后端 |

- `instance()` 一律 **function-local static**；
- 构造后**只读**（拷贝/赋值删除）；构造期冻结浅拷贝快照——`App` 构造时冻结，
  运行期不再接受注册；
- 注册的是**工厂而非原型**：避免 clone 语义，`create()` 每次给新实例；
- 查询：`supports(name)`、`create(name)`（chart）、`raster_name(kind)`（raster）；
- **枚举三件套**（2026-10-24 统一补齐，六表一致）：
  - `names()`：已登记名字列表。map 后端的表按字典序；vector 后端的表
    （plugin/dsl）按注册序——都在各自头文件的注释里写明，调用方按需排序；
  - `size()` / `empty()`：与 `names()` 严格同口径（`size() == names().size()`），
    不存在"查得到但数不清"的名字。

三件套的存在理由：AutoViz 路由、CLI `--help`、诊断列举都要求"已注册了什么"
可编程枚举。曾出现 ChartRegistry/RasterRegistry 缺枚举、上游只能硬编码名单
的割裂状态；现六表一致，且 `size()==names().size()` 有测试守着。

## 注册时机协议

**注册的唯一安全时机是 `App` 构造前**（CODEBUDDY.md 坑 5）。运行期注册插件或
scale 是未定义用法——快照已冻结，注册不生效或竞争。

为什么：注册表是全局可变状态的最小化尝试。冻结快照后，热路径查找无锁、
无失效回调、无线程问题；代价是失去运行期插拔能力，这是刻意交换。

## 初始化 vs 注册

`api::init_core()`（`std::call_once` 守卫，幂等）只做**全局副作用**的最后容身：
日志级别、默认能力快照。**注册表不在这里**——注册时机另有协议（上文）。
`core_initialized()` 可查询。

## 可扩展性契约

`tests/integration/replace_*.cpp` 三组实验验证：新增 backend / chart / raster
时核心零改动。新实现 = 新文件 + 构造期注册，不修改任何既有文件。

## 反例

- 在头文件的静态对象构造里注册（静态初始化顺序灾难）；
- 运行期（App 构造后）注册并期望生效；
- 用 clone 原型模式替代工厂。
