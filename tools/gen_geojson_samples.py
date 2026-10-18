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
        if "name" not in feat.get("properties", {}):
            errors.append(f"{path.name}: features[{i}] 缺 properties.name（无法匹配 value）")
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
