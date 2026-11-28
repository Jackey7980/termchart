# third_party

随仓库分发（vendored）的第三方源码。**只收确实必须进仓库的代码**；能走系统包 / vcpkg 的一律不进。

| 目录 | 组件 | 版本 | 许可 | 用途 | 是否默认启用 |
| --- | --- | --- | --- | --- | --- |
| `doctest/` | [doctest](https://github.com/doctest/doctest) | 2.5.3 | MIT | 唯一测试框架（单头 `doctest/doctest.h`） | 是（构建测试时需要） |
| `stb/` | [stb](https://github.com/nothings/stb) `stb_image` / `stb_image_write` | v2.30 / v1.16 | public domain | 可选图像解码后端（JPEG/GIF/WebP），实现 TU 在 `src/media/image/decoder.cpp`，受 `TERMCHART_HAVE_STB_IMAGE` 守卫 | 否（主库自带 PNG 编解码，不依赖它） |

许可全文放在 `LICENSES/`：`MIT.txt`（本体与 doctest）、`Apache-2.0.txt`（Turbo 色表）。
public domain 组件无需许可全文，上表已声明归属。

升级 vendored 组件时：更新本表版本号 → 跑 `check_all` → 若影响可选解码路径，补跑对应的 `examples/` 与测试。
