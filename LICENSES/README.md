# 许可证与资产↔许可映射

本仓库的第三方与派生资产及其许可如下：

| 资产 / 代码 | 许可 | 来源 / 备注 |
| --- | --- | --- |
| 项目本体 + doctest + utf8proc | MIT | `LICENSES/MIT.txt` |
| `src/color/colormap.cpp` 中的 **Turbo** 实现 | **Apache-2.0** | Google LLC, Anton Mikhailov — "Turbo, An Improved Rainbow Colormap for Visualization"（`LICENSES/Apache-2.0.txt`） |
| 计划中的 `assets/colormaps/viridis.dat`、`magma.dat`、`inferno.dat` | CC0-1.0 | `LICENSES/CC0-1.0.txt`（viridis/magma/inferno 为公共领域） |
| 计划中的 `assets/colormaps/turbo.dat`（若采用官方 LUT） | **Apache-2.0** | 同 Turbo 实现，绝非 CC0 |

## 重要更正

Turbo 原始配色（Anton Mikhailov / Google）采用 **Apache-2.0**，**不是 CC0**。
当前代码以锚点线性插值近似 Turbo（见 `src/color/colormap.cpp` 的 `kTurboStops` /
`turbo_sample`），仍须保留 Apache-2.0 归属与 `LICENSES/Apache-2.0.txt`。

REUSE 扫描（`check_license.sh` / `.github/workflows/license.yml`）要求每个带许可的
文件/资产在 `LICENSES/` 下有对应全文，且资产↔许可映射正确；Turbo 若误标 CC0 会直接报错。
若后续改用官方 256 级 LUT，请确保该数据同样按 Apache-2.0 归属。
