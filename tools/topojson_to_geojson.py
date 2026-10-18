#!/usr/bin/env python3
"""topojson_to_geojson.py —— TopoJSON (world-atlas) → 标准 GeoJSON 转换器

世界地图真数据源：https://github.com/topojson/world-atlas (CDN: jsdelivr)
- 110m (countries-110m.json) ≈ 100KB，177 国
- 50m (countries-50m.json)   ≈ 350KB
- 10m (countries-10m.json)   ≈ 几 MB，10m 精度需要

输出格式：标准 GeoJSON FeatureCollection，每国一个 Feature。
properties 含 'name'（用于 set_value 匹配），仅保留最小字段。

Y 轴语义：本脚本不翻转 Y——地理数据 lat 是上正下负（北为正），
画布/canvas Y 是下正上负（0 在顶），翻转由 GeoMap 渲染器做（致命提醒 1）。

用法：
    python tools/topojson_to_geojson.py \
        --in  assets/maps/world_110m.json \
        --out assets/maps/world.json
    python tools/topojson_to_geojson.py \
        --in  assets/maps/cn_50m.json \
        --out assets/maps/cn.json
"""
import argparse
import json
import pathlib
import sys
from typing import Any


def decode_arc(arc: list[list[float]], transform: dict | None) -> list[list[float]]:
    """TopoJSON 用整数 + transform 量化压缩；这里反量化得到真实 [lon, lat]。"""
    x, y = 0.0, 0.0
    decoded = []
    scale = transform.get("scale", [1.0, 1.0]) if transform else [1.0, 1.0]
    translate = transform.get("translate", [0.0, 0.0]) if transform else [0.0, 0.0]
    for pair in arc:
        x += pair[0]
        y += pair[1]
        decoded.append([x * scale[0] + translate[0], y * scale[1] + translate[1]])
    return decoded


def stitch_arc(arcs_list: list[list[int]], arcs: list[list[list[float]]],
               transform: dict | None) -> list[list[float]]:
    """缝合一个 ring：arcs_list 是 arc 索引列表（正数 = 正向，负数 = 反向）。"""
    if not arcs_list:
        return []
    pts: list[list[float]] = []
    for idx in arcs_list:
        # 负数索引在 TopoJSON 里表示"反向引用 ~idx"，不是 Python 的从尾取——
        # 直接用 arcs[idx] 会读到完全不相干的弧。反序也必须放在**解码之后**：
        # arcs 里的点是差分编码的，先 reverse 再累加会把增量当绝对坐标。
        decoded = decode_arc(arcs[idx if idx >= 0 else ~idx], transform)
        if idx < 0:
            decoded.reverse()
        if pts and decoded:
            # TopoJSON 共享端点：跳过首点
            pts.extend(decoded[1:])
        else:
            pts.extend(decoded)
    return pts


def geom_for(geom: dict, arcs: list[list[list[float]]], transform: dict | None) -> dict:
    """递归解 geometry → 嵌套 ring 数组。"""
    gtype = geom.get("type")
    if gtype == "Polygon":
        rings = [stitch_arc(arcs_list, arcs, transform) for arcs_list in geom["arcs"]]
        return {"type": "Polygon", "coordinates": rings}
    if gtype == "MultiPolygon":
        polys = []
        for poly_arcs in geom["arcs"]:
            rings = [stitch_arc(arcs_list, arcs, transform) for arcs_list in poly_arcs]
            polys.append(rings)
        return {"type": "MultiPolygon", "coordinates": polys}
    if gtype == "GeometryCollection":
        return {"type": "GeometryCollection",
                "geometries": [geom_for(g, arcs, transform) for g in geom["geometries"]]}
    raise ValueError(f"unsupported geometry type: {gtype}")


def convert(topo: dict, object_name: str) -> dict:
    obj = topo["objects"][object_name]
    arcs = topo["arcs"]
    transform = topo.get("transform")
    features: list[dict[str, Any]] = []
    for geom in obj["geometries"]:
        # name 在 properties.name（110m 是此结构）；同时兼容 50m 的 NAME 字段
        props = dict(geom.get("properties", {}))
        name = props.get("name") or props.get("NAME") or props.get("NAME_EN") or ""
        if not name:
            # 110m 用 numeric id；打印警告不报错
            print(f"  warn: feature {geom.get('id')} has no name", file=sys.stderr)
        out_props = {"name": name}
        # 透传几个常用字段（如有）
        for k in ("iso_a2", "iso_a3", "ISO_A2", "ISO_A3", "continent"):
            if k in props:
                out_props[k.lower()] = props[k]
        features.append({
            "type": "Feature",
            "properties": out_props,
            "geometry": geom_for(geom, arcs, transform),
        })
    bbox = topo.get("bbox", [])
    fc: dict[str, Any] = {"type": "FeatureCollection", "features": features}
    if bbox:
        fc["bbox"] = bbox
    return fc


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument("--in", dest="src", required=True, help="TopoJSON input path")
    p.add_argument("--out", dest="dst", required=True, help="GeoJSON output path")
    p.add_argument("--object", default="countries", help="object name (default: countries)")
    args = p.parse_args()

    src = pathlib.Path(args.src)
    dst = pathlib.Path(args.dst)
    if not src.is_file():
        print(f"missing: {src}", file=sys.stderr)
        return 1
    print(f"reading  {src}  ({src.stat().st_size} bytes)")
    topo = json.loads(src.read_text(encoding="utf-8"))
    print(f"converting  object='{args.object}'  geometries={len(topo['objects'][args.object].get('geometries', []))}")
    fc = convert(topo, args.object)
    print(f"writing  {dst}  features={len(fc['features'])}")
    # 不缩进，省字节（不刻意：std json 已缩进 0）
    dst.write_text(json.dumps(fc, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    print(f"done  size={dst.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
