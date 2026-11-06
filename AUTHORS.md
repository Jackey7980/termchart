# Authors

## 维护者

| 作者 | 联系方式 | 角色 |
| --- | --- | --- |
| Jackey | <jackey_1@qq.com> | 项目发起人与维护者 |

完整提交历史见 `git log`；贡献者以仓库提交记录为准。

## 项目许可

termchart 本体采用 **MIT License**（版权人 Jackey），全文见 [`LICENSE`](LICENSE) 与
[`LICENSES/MIT.txt`](LICENSES/MIT.txt)。第三方资产见下表，各自的许可不覆盖本体。

## 第三方资产

| 资产 | 版权方 | 许可证 | 位置 |
| --- | --- | --- | --- |
| Turbo 色表 | Anton Mikhailov / Google LLC | Apache License 2.0 | `assets/colormaps/turbo.dat`（数据源；由 `tools/gen_turbo_dat.py` 抽取），全文见 `LICENSES/Apache-2.0.txt` |
| doctest | Viktor Kirilov 及贡献者 | MIT | `third_party/doctest/`（vendored，仅测试使用） |
| stb_image / stb_image_write | Sean Barrett（nothings.org） | public domain | `third_party/stb/`（vendored，可选图像编解码） |
| Qt 6 / 5（可选 GUI 后端） | The Qt Company Ltd. 及贡献者 | LGPLv3 / GPLv3 / 商业（本项目仅动态链接 LGPLv3） | **不随仓库分发**，由使用者 `find_package` 自备；见 `plugins/gui_qt/` |

许可证全文见 [`LICENSES/`](LICENSES/)。
