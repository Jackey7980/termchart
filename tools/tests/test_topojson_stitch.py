#!/usr/bin/env python3
"""TopoJSON 缝合（stitch_arc）离线回归测试。

锁住 tools/topojson_to_geojson.py::stitch_arc 的两处解码错误：

  1. 负索引必须取 ``~idx``（TopoJSON 的「反向引用」），直接 ``arcs[idx]`` 会读到
     完全不相干的弧（Python 的负数索引是从尾取，语义不同）；
  2. 反序必须放在**解码（delta 累加）之后**——差分编码的点若先 reverse 再累加，
     会把增量当成绝对坐标，几何全错。

两条都已在源码里修复。本测试用最小夹具 + 直接调用两种方式断言正确几何，
并显式验证「负索引解析到正确弧」这一不变量。

验收（回归红线）：把 ``stitch_arc`` 里的 ``~idx`` 改回 ``idx``，或把
``decoded.reverse()`` 移到 ``decode_arc`` 之前，下面任一断言都会变红。

运行：
    python tools/tests/test_topojson_stitch.py      # 独立运行，非零退出即失败
（CI guardrails 作业也会跑它，无需 pytest。）
"""
import json
import pathlib
import sys

# 让脚本能 import 同目录的 topojson_to_geojson（tools/ 不是包）。
_HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE.parent))
import topojson_to_geojson as tj  # noqa: E402


def _load_fixture() -> dict:
    return json.loads((_HERE / "topojson_stitch_sample.json").read_text(encoding="utf-8"))


def test_stitch_arc_direct():
    """直接调用 stitch_arc（单位变换），隔离缝合逻辑本身。"""
    arcs = [
        [[0, 0], [1, 0], [1, 1], [0, 1]],   # arc 0：正向
        [[5, 5], [2, 0], [0, 2]],           # arc 1：正向（被 -2 反向引用）
    ]
    # 0 正向 + (-2 → arc1 反向)
    got = tj.stitch_arc([0, -2], arcs, None)
    expected = [[0, 0], [1, 0], [2, 1], [2, 2], [7, 5], [5, 5]]
    assert got == expected, f"stitch_arc([0,-2]) = {got}, expected {expected}"


def test_stitch_arc_positive_only():
    """纯正向弧不应被反转。"""
    arcs = [
        [[0, 0], [1, 0], [1, 1], [0, 1]],
        [[5, 5], [2, 0], [0, 2]],
    ]
    got = tj.stitch_arc([1], arcs, None)
    expected = [[5, 5], [7, 5], [7, 7]]
    assert got == expected, f"stitch_arc([1]) = {got}, expected {expected}"


def test_negative_index_resolves_to_correct_arc():
    """负索引必须解析到 arc1（而非 Python 的 arcs[-2]==arcs[0]）。"""
    arcs = [
        [[0, 0], [1, 0], [1, 1], [0, 1]],   # arc 0
        [[5, 5], [2, 0], [0, 2]],           # arc 1
    ]
    # 单弧引用 -2：应等价于 arc1 反向，而不是 arc0 反向。
    got = tj.stitch_arc([-2], arcs, None)
    expected = [[7, 7], [7, 5], [5, 5]]     # arc1 解码后反序
    assert got == expected, f"stitch_arc([-2]) = {got}, expected {expected}"

    # 反例：若误用 arcs[idx]，会得到 arc0 反序 = [[2,2],[2,1],[1,0],[0,0]]，与上式不同。
    buggy = tj.stitch_arc([-2], arcs, None)  # 当前实现正确
    assert buggy != [[2, 2], [2, 1], [1, 0], [0, 0]], "负索引误解析到 arc0，stitch_arc 有回退 bug"


def test_convert_fixture_end_to_end():
    """端到端 convert()：负索引反向弧 + 正向弧，2 个 feature，几何正确。"""
    topo = _load_fixture()
    fc = tj.convert(topo, "countries")

    assert fc["type"] == "FeatureCollection"
    assert len(fc["features"]) == 2, f"feature 数应为 2，实际 {len(fc['features'])}"

    by_name = {f["properties"]["name"]: f for f in fc["features"]}
    assert "Alpha" in by_name and "Beta" in by_name

    # Alpha：arc0 正向 + arc1 反向（单位变换，几何即 delta 累加）。
    alpha = by_name["Alpha"]["geometry"]["coordinates"][0]
    expected_alpha = [[0, 0], [10, 0], [10, 10], [0, 10], [70, 50], [50, 50]]
    assert alpha == expected_alpha, f"Alpha ring = {alpha}, expected {expected_alpha}"

    # Beta：arc1 正向。
    beta = by_name["Beta"]["geometry"]["coordinates"][0]
    expected_beta = [[50, 50], [70, 50], [70, 70]]
    assert beta == expected_beta, f"Beta ring = {beta}, expected {expected_beta}"


def main() -> int:
    tests = [
        test_stitch_arc_direct,
        test_stitch_arc_positive_only,
        test_negative_index_resolves_to_correct_arc,
        test_convert_fixture_end_to_end,
    ]
    failures = 0
    for t in tests:
        try:
            t()
            print(f"  ok: {t.__name__}")
        except AssertionError as e:
            failures += 1
            print(f"  FAIL: {t.__name__}: {e}")
    if failures:
        print(f"topojson_stitch: {failures} 失败")
        return 1
    print("topojson_stitch: 全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
