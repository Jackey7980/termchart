# termchart Rust 绑定

基于稳定的 C ABI `termchart_capi`（见 `docs/design/ffi_plan.md`）。两个 crate：

| crate | 作用 |
| --- | --- |
| `termchart-sys` | 手写 raw FFI 声明，逐条对齐 `libs/capi/include/termchart_capi.h`。不依赖 bindgen/libclang（C ABI 冻结、只增不改，checked-in 绑定即稳定）。 |
| `termchart` | 安全封装：`Error`、`plot`/`scatter`/`bar` 一行出图、`Chart` RAII、SVG/PNG 两段式导出、属性通道。点集用 `&[f64]` **零拷贝**直传（对齐 Python numpy 桥的取向）。 |

## 前置：先编 native C ABI 库

Rust 桥**不编译 C++**。先在仓库根构建 `termchart_capi`：

```powershell
cmake -S . -B build/ninja -DTERMCHART_BUILD_CAPI=ON
cmake --build build/ninja --target termchart_capi
```

得到 `build/ninja/libs/capi/libtermchart_capi.dll`（Windows）或 `.so`（Unix）。

## 构建/运行

指向 native 库所在目录，再 cargo：

```powershell
$env:TERMCHART_CAPI_DIR = "C:\termchart\build\ninja\libs\capi"
cargo build --manifest-path bindings/rust/Cargo.toml
cargo run   --manifest-path bindings/rust/Cargo.toml -p termchart --example quick
```

Unix 上把 `.so` 目录加进 `LD_LIBRARY_PATH`，Windows 上把 `.dll` 目录加进 `PATH`（运行期）。
也可用 `TERMCHART_CAPI_LIB` 指到完整库文件路径（取其父目录做链接搜索）。

## 设计对齐（用户定的 C 面原则）

- **固定骨架 + 属性通道**：`Chart::set_*_prop` 走单一 `tc_chart_set_prop`，加配置 = 加枚举 + 一个 C switch，Rust 绑定不动。
- **零拷贝批量数据**：`add_points(&[f64], &[f64])` 直传裸指针 + 长度，不经逐点 setter。
- **两段式缓冲**：`to_svg`/`to_png`/`kinds` 用 cap=0 探长度再取，Rust 侧封成 `Result<Vec<u8>>`。
- **fail-loud**：所有非 0 状态读 `tc_error_message` 抛 `Error{code,message}`，不静默降级。

## 验证状态（2026-11-16）

本机未安装 Rust 工具链（`cargo`/`rustc` 不在 PATH），故**本桥源码已落地但未在本机 `cargo build` 验证**。
`termchart-sys` 的每条 extern 声明均逐字对齐 `termchart_capi.h`；CI 后续在带 Rust 的 job 上跑
`cargo build` + `cargo test -p termchart` 即可闭环（与 Java 桥 `java_smoke`、Python ctypes 同档）。
