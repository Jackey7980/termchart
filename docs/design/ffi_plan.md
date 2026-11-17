# FFI / 语言桥方案（Rust 桥、Java 桥）

> 状态（2026-11-16 更新）：**C ABI（`termchart_capi`）+ Python ctypes + Java JNI + Rust 桥全部落地**：
> - C ABI：`libs/capi`（v1.1，见 [C ABI 与 Python 桥](../guide/c_api.md)）。
> - Java：`bindings/java`（`Termchart` 高层出图类 + JNI glue 包 capi），JNI 库自包含，
>   `java_smoke` 真 JVM 冒烟通过（plot/bar/kinds/renderLine/错误通道）；`TERMCHART_BUILD_JAVA=ON` 开启。
> - Rust：`bindings/rust`（`termchart-sys` 手写 FFI + `termchart` 安全封装），一行出图 +
>   Chart RAII + SVG/PNG 两段式导出；本机无 cargo，**源码已落地待 CI/Rust 工具链编译闭环**。
>
> 本文件降级为设计记录；实际 API 以各 `bindings/` 子目录与 `termchart_capi.h` 为准。

## 0. 现状缺口

`bindings/python`（pybind11）是唯一的语言桥。它直接吃 C++ 头，零 C ABI。

- **Rust**：全库没有任何 `extern "C"` 接口；C++ 库不是 C ABI 稳定层，不能直接
  `bindgen` 吃掉 `termchart::api::*`（模板/异常/STL 容器会炸）。
- **Java 通用桥**：现有一个 `plugins/gui_jni` 的 **GUI 注入后端**（把 `jintArray`
  当表面、剥壳原生事件），但它**只覆盖 GUI 平行栈**，不含 `api::quick` 的高层出图
  API。也就是说 Java 使用者现在无法 `plot()`/`save_png()`。

两套桥要成立，都得先补一层 **C ABI 适配层**（`termchart_capi`），再各自包壳。

## 1. C ABI 适配层（`termchart/capi`）

新建 `libs/capi`（静态库，C 链接，无异常透出）：

```c
// termchart_capi.h  (C ABI, 稳定 ABI, 全用 POD / 句柄 / 回调)
#ifdef __cplusplus
extern "C" {
#endif
typedef struct tc_chart* tc_chart_handle;
typedef struct tc_err*   tc_error_handle;

tc_chart_handle tc_chart_create(const char* kind, tc_error_handle* err);
void tc_chart_add_points(tc_chart_handle, const double* x, const double* y, size_t n);
tc_error_handle tc_chart_to_png(tc_chart_handle, const char* path,
                               uint32_t cell_w, uint32_t cell_h,
                               uint32_t px_w, uint32_t px_h);
void tc_error_message(tc_error_handle, char* buf, size_t cap);
void tc_error_free(tc_error_handle);
/* 像素缓冲、SVG、ANSI 文本同理，字符串用长度 + 调用方提供缓冲/回调 */
#ifdef __cplusplus
}
#endif
```

要点：
- 返回码一律 `tc_error_handle`，**绝不抛 C++ 异常**过 C 边界；Rust/Java 侧用 `catch
  (...) { tc_error_* }` 兜底。
- 字符串/像素走「调用方分配缓冲 + 长度回写」或回调，避免 `std::string` 跨 ABI。
- 这个层只包 `api::quick` 高层路径（出图/文件/PNG 字节），**不暴露 widget_tree**；
  真要 GUI 注入，Java 继续用现成 `plugins/gui_jni`，Rust 复用 `gui_*` 的 Take 2 思路
  另写（`tc_native_surface` 句柄 + `tc_backend_blit`）。

> 实现时暴露面已扩展（上面的代码片段是设计草图，非最终签名）：实际新增了 `tc_chart_*`
> 图表工厂、`tc_chart_add_series` 多系列、`tc_chart_set_prop` 模型属性通道（含
> `TC_PROP_X_LABEL` / `TC_PROP_Y_LABEL` 轴标签）、`tc_chart_to_svg` / `tc_chart_to_png`
> 两段式导出。完整签名与示例见 [C ABI 与 Python 桥](../guide/c_api.md)。

## 2. Java 桥（基于现有 JNI 后端 + 新 CAPI）

两条腿：
1. **高层出图**：用上面的 `termchart_capi`，写一份 JNI glue（`javac -h` 出的头）
   把 `api::quick` 暴露成 `Termchart.plot(double[], double[]) -> String`、
   `Termchart.savePng(...)`。比现在 `gui_jni` 的 `jintArray` 表面更通用。
2. **GUI 注入**（已有）：保留 `plugins/gui_jni`（`jni_backend.cpp` 已真 JVM 冒烟），
   供 Android/JavaFX 把图嵌进已有窗口。
- JDK：`find_package(JNI REQUIRED)`；构建 `libtermchart_jni` 共享库；运行期 `System.
  loadLibrary`。C API 的 `tc_chart_*` 链接进同一个 JNI 库即可。

## 3. Rust 桥

两种实现路线（二选一，倾向 A）：
- **A. cxx**：`cxx::bridge` 声明上面的 `tc_chart_*` 为 `extern "Rust"` ↔ `extern "C++"`，
  由 cxx 生成安全封装。优点：零 unsafe 面、能直接传 `&[f64]`。缺点：多一个构建依赖。
- **B. bindgen + 手写 unsafe**：bindgen 吃 `termchart_capi.h` 生成 `sys` crate，再手写
  安全封装。优点：少依赖；缺点：unsafe 面大、易错。
- 包名 `termchart-sys`（C ABI 绑定）+ `termchart`（安全封装），crates.io 发布。
- 数据入参用 `ndarray::ArrayView1<f64>` → 直接传裸指针，零拷贝（和 Python 桥的 numpy
  思路一致）。

## 4. 工作量与风险

| 项 | 工作量 | 风险 |
| --- | --- | --- |
| C API 适配层 | 中（约 1 个 .cpp + 头） | 低，纯 POD |
| Java 桥 | 中 | 中（JNI 签名/JVM 加载/ LGPL 动态链接） |
| Rust 桥（cxx） | 中 | 中（cxx 版本/跨编译） |

## 5. 落地节奏（2026-11-13 决策 → 2026-11-15 更新）

- **`termchart_capi` → 已落地**：C ABI 适配层方案（见 §1）已在 v1.1 实现，真实暴露面与示例见
  [C ABI 与 Python 桥](../guide/c_api.md)。开工时曾列的待敲定项已随实现收敛：
  ① 暴露面 = 高层出图（`render/print` 文本、`chart_*` 图表、`to_svg`/`to_png` 两段式导出、
  `set_prop` 模型属性通道），**不含 GUI 注入**（GUI 仍走 `plugins/gui_*` 原生后端）；
  ② 发布形式暂定仓库内 vendored（Rust/Java 开工时再定 crates.io / Maven Central）；
  ③ Rust 路线仍倾向 cxx。
- **Rust/Java 搭桥 → 推迟**：不在 v1.1，也不在本轮落地。两者都依赖 capi 先就位，故未来开工时
  `搭桥` 这一项在 todo 中排**较前**（先于其它扩展项），让 capi 先行、两桥随后并行。

开始实现后，本文件降级为设计记录，落到对应 `bindings/` 子目录与 CI job。
