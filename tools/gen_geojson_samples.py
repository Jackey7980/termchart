#!/usr/bin/env python3
# gen_geojson_samples.py —— 校验 assets/maps/*.json 的 GeoJSON 合法性
#
# 用法：
#   python tools/gen_geojson_samples.py --check   # 仅校验，diff 退出码
#   python tools/gen_geojson_samples.py          # 默认 --check
#
# 目的：把"地图数据"做成生成物校验对象，破坏样本立即在 ctest 之前失败。
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MAPS_DIR = ROOT / "assets" / "maps"

REQUIRED_KEYS = {"type", "features"}
ALLOWED_GEOMETRY = {"Polygon", "MultiPolygon"}

# WGS84 的合法值域。量化/差分编码写错时会越界（曾把差分增量当绝对坐标、
# 产出 lat=-119 的世界地图），只看 JSON 结构是看不出来的。
LON_RANGE = (-180.0, 180.0)
LAT_RANGE = (-90.0, 90.0)
EPS = 1e-6


def check_ring(name: str, ring: list) -> list[str]:
    """一个环：至少 3 个不同点 + 闭合，且每个点都在经纬度值域内。"""
    errors: list[str] = []
    if len(ring) < 4:
        errors.append(f"{name}: 环只有 {len(ring)} 个点（至少 4 个，含闭合点）")
        return errors
    if ring[0] != ring[-1]:
        errors.append(f"{name}: 环未闭合（首点 {ring[0]} ≠ 末点 {ring[-1]}）")
    for pt in ring:
        if not isinstance(pt, list) or len(pt) < 2:
            errors.append(f"{name}: 坐标 {pt} 不是 [lon, lat]")
            return errors
        lon, lat = pt[0], pt[1]
        if not (LON_RANGE[0] - EPS <= lon <= LON_RANGE[1] + EPS
                and LAT_RANGE[0] - EPS <= lat <= LAT_RANGE[1] + EPS):
            errors.append(f"{name}: 坐标 [{lon}, {lat}] 超出经纬度值域")
            return errors
    return errors


def check_one(path: pathlib.Path) -> list[str]:
    errors: list[str] = []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        return [f"{path.name}: JSON 解析失败: {e}"]

    missing = REQUIRED_KEYS - data.keys()
    if missing:
        errors.append(f"{path.name}: 缺字段 {missing}")
        return errors
    if data["type"] != "FeatureCollection":
        errors.append(f"{path.name}: type 应为 FeatureCollection，实际 {data['type']}")
        return errors

    for i, feat in enumerate(data["features"]):
        if feat.get("type") != "Feature":
            errors.append(f"{path.name}: features[{i}] type 不是 Feature")
            continue
        geom = feat.get("geometry", {})
        gtype = geom.get("type")
        if gtype not in ALLOWED_GEOMETRY:
            errors.append(f"{path.name}: features[{i}] geometry.type={gtype}（不支持）")
            continue
        props = feat.get("properties", {})
        if "name" not in props:
            errors.append(f"{path.name}: features[{i}] 缺 properties.name（无法匹配 value）")
            continue
        name = f"{path.name}[{props['name']}]"
        coords = geom.get("coordinates") or []
        polygons = [coords] if gtype == "Polygon" else coords
        for poly in polygons:
            for ring in poly:
                errors.extend(check_ring(name, ring))
    return errors


def main() -> int:
    files = sorted(MAPS_DIR.glob("*.json"))
    if not files:
        print(f"[gen_geojson_samples] WARN: {MAPS_DIR} 无 .json 文件", file=sys.stderr)
        return 0

    all_errors: list[str] = []
    for f in files:
        all_errors.extend(check_one(f))

    if all_errors:
        print("[gen_geojson_samples] FAIL:", file=sys.stderr)
        for e in all_errors:
            print(f"  - {e}", file=sys.stderr)
        return 1
    print(f"[gen_geojson_samples] OK: {len(files)} files valid ({[f.name for f in files]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
