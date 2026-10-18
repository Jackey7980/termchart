#!/usr/bin/env python3
"""fetch_map_data.py —— 从公开数据源抓取 assets/maps/*.json 的地图数据

三个数据源都是公开的原始地理数据，本脚本只做**无损裁剪**（丢字段、量化坐标、
点 → 小多边形的几何化），不改形状语义：

  world   world-atlas countries-50m.json（Natural Earth 1:50m，TopoJSON）
          → 复用 topojson_to_geojson.convert() 转标准 GeoJSON；
            坐标量化到 2 位小数（≈1.1 km），远细于任何终端子像素。
  cn      geo.datav.aliyun.com areas_v3/bound/100000_full.json（省级界，GeoJSON）
          → 只保留 name / name_zh 两个属性；adcode → 英文名，键与
            examples/68_cn_province.cpp 的 set_value 一致；坐标量化到 4 位小数。
  cities  natural-earth-vector ne_50m_populated_places_simple.geojson（城市点）
          → 取 pop_max 前 N 的城市，几何化成 16 边形"人口气泡"。
            原因：GeoMap（以及本库的 GeoJSON 解析器）只认 Polygon/MultiPolygon，
            Point 不支持；气泡半径 ∝ sqrt(pop_max)，经度半径按 1/cos(lat) 拉宽，
            让气泡在等距矩形投影下仍然是圆的。

为什么放在 tools/ 而不是库里：
  * 库必须零强制依赖（不引入 HTTP 客户端）；
  * 示例已登记进 ctest，联网会让 CI 超时；
  * 真正的产物已经提交在 assets/maps/，本脚本只是"怎么来的"的可复现记录。
  原始文件缓存到 build/mapdata/（不进版本库），重复运行不会重复下载。

用法：
    python tools/fetch_map_data.py --what all
    python tools/fetch_map_data.py --what world --force
    python tools/fetch_map_data.py --what cities --cities 30
"""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import sys
import urllib.request

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from topojson_to_geojson import convert as topo_convert  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "assets" / "maps"
CACHE_DIR = ROOT / "build" / "mapdata"

SOURCES = {
    "world": "https://unpkg.com/world-atlas@2/countries-50m.json",
    "cn": "https://geo.datav.aliyun.com/areas_v3/bound/100000_full.json",
    "cities": ("https://raw.githubusercontent.com/nvkelso/natural-earth-vector/"
               "master/geojson/ne_50m_populated_places_simple.geojson"),
}

# adcode → 英文名。键必须与 examples/68_cn_province.cpp 的 set_value 一致，
# 否则该省静默留在"无值灰底"（GeoMap 的既定行为：未命中不报错）。
CN_NAMES = {
    "110000": "Beijing",
    "120000": "Tianjin",
    "130000": "Hebei",
    "140000": "Shanxi",
    "150000": "Inner Mongolia",
    "210000": "Liaoning",
    "220000": "Jilin",
    "230000": "Heilongjiang",
    "310000": "Shanghai",
    "320000": "Jiangsu",
    "330000": "Zhejiang",
    "340000": "Anhui",
    "350000": "Fujian",
    "360000": "Jiangxi",
    "370000": "Shandong",
    "410000": "Henan",
    "420000": "Hubei",
    "430000": "Hunan",
    "440000": "Guangdong",
    "450000": "Guangxi",
    "460000": "Hainan",
    "500000": "Chongqing",
    "510000": "Sichuan",
    "520000": "Guizhou",
    "530000": "Yunnan",
    "540000": "Tibet",
    "610000": "Shaanxi",
    "620000": "Gansu",
    "630000": "Qinghai",
    "640000": "Ningxia",
    "650000": "Xinjiang",
    "710000": "Taiwan",
    "810000": "Hong Kong",
    "820000": "Macao",
    "100000_JD": "South China Sea Islands",
}


def fetch(url: str, cache: pathlib.Path, force: bool) -> bytes:
    if cache.is_file() and not force:
        print(f"  cache   {cache}  ({cache.stat().st_size} bytes)")
        return cache.read_bytes()
    print(f"  GET     {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "termchart-fetch/1.0"})
    with urllib.request.urlopen(req, timeout=180) as resp:  # noqa: S310 (固定 https 源)
        data = resp.read()
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_bytes(data)
    print(f"  saved   {cache}  ({len(data)} bytes)")
    return data


def quantize_rings(polygons: list, nd: int) -> list:
    """环坐标量化到 nd 位小数，并丢掉量化后退化（<4 个不同点）的环/多边形。

    量化只是压缩体积：2 位小数 ≈ 1.1 km，比终端最细的子像素还细两个数量级；
    环点数不变，所以海岸线/省界的"细节量"完全保留。
    """
    out = []
    for rings in polygons:
        kept = []
        for ring in rings:
            q = []
            last = None
            for pt in ring:
                p = [round(float(pt[0]), nd), round(float(pt[1]), nd)]
                if p != last:
                    q.append(p)
                    last = p
            if len(q) >= 4:
                kept.append(q)
        if kept:
            out.append(kept)
    return out


def as_polygons(geom: dict) -> list:
    """Polygon/MultiPolygon → 统一的多边形列表（每个元素是一组环）。"""
    gtype = geom.get("type")
    if gtype == "Polygon":
        return [geom["coordinates"]]
    if gtype == "MultiPolygon":
        return list(geom["coordinates"])
    raise ValueError(f"unsupported geometry type: {gtype}")


def feature(name: str, polygons: list, extra: dict | None = None) -> dict:
    props = {"name": name}
    if extra:
        props.update(extra)
    return {
        "type": "Feature",
        "properties": props,
        "geometry": {"type": "MultiPolygon", "coordinates": polygons},
    }


def write_json(obj: dict, path: pathlib.Path) -> None:
    text = json.dumps(obj, ensure_ascii=False, separators=(",", ":"))
    path.write_text(text, encoding="utf-8")
    print(f"  wrote   {path.relative_to(ROOT)}  ({len(text)} bytes, "
          f"{len(obj['features'])} features)")


# ── world ───────────────────────────────────────────────────────────────────

def build_world(force: bool) -> dict:
    print("[world] Natural Earth 1:50m countries")
    raw = fetch(SOURCES["world"], CACHE_DIR / "countries-50m.json", force)
    topo = json.loads(raw)
    fc = topo_convert(topo, "countries")

    features = []
    for f in fc["features"]:
        name = (f.get("properties") or {}).get("name") or ""
        polys = quantize_rings(as_polygons(f["geometry"]), 2)
        if polys:
            features.append(feature(name, polys))
    out = {"type": "FeatureCollection", "features": features}
    write_json(out, OUT_DIR / "world.json")
    return out


# ── cn ──────────────────────────────────────────────────────────────────────

def build_cn(force: bool) -> dict:
    print("[cn] 省级行政区边界")
    raw = fetch(SOURCES["cn"], CACHE_DIR / "cn-100000-full.json", force)
    src = json.loads(raw)

    features = []
    for f in src["features"]:
        props = f.get("properties") or {}
        adcode = str(props.get("adcode", ""))
        zh = (props.get("name") or "").strip()
        en = CN_NAMES.get(adcode)
        if en is None:
            print(f"  warn: adcode {adcode!r} ({zh}) 没有英文名映射，跳过",
                  file=sys.stderr)
            continue
        polys = quantize_rings(as_polygons(f["geometry"]), 4)
        if polys:
            features.append(feature(en, polys, {"name_zh": zh} if zh else None))
    out = {"type": "FeatureCollection", "features": features}
    write_json(out, OUT_DIR / "cn.json")
    return out


# ── cities ──────────────────────────────────────────────────────────────────

def circle(lon: float, lat: float, radius_deg: float, segments: int = 16) -> list:
    """经纬度上画一个"看起来是圆"的多边形。

    等距矩形投影把 1° 经度压成 cos(lat) × 1° 纬度，所以经度方向的半径要除以
    cos(lat) 才不会在高纬度变成扁椭圆。
    """
    k = 1.0 / max(0.05, math.cos(math.radians(lat)))
    ring = []
    for i in range(segments):
        a = 2.0 * math.pi * i / segments
        ring.append([lon + radius_deg * k * math.cos(a),
                     lat + radius_deg * math.sin(a)])
    ring.append(ring[0])  # GeoJSON 环闭合
    return ring


def bubble_radius(pop: float, pop_max: float) -> float:
    """半径 ∝ sqrt(pop)，范围 [3.0°, 10°]。

    量级是被终端逼出来的：90×30 字符格的盲文网格只有 180×120 子像素，
    世界宽 360° → 1 字符格 ≈ 3.6°、1 子像素 ≈ 2°。半径 <2° 的气泡在终端里
    连一个子像素都占不到，所以下限直接取 3°（≈ 1.7 格），保证肉眼可见。
    """
    t = math.sqrt(max(pop, 0.0) / max(pop_max, 1.0))
    return 3.0 + (10.0 - 3.0) * t


def build_cities(force: bool, limit: int) -> dict:
    print(f"[cities] Natural Earth populated places → 前 {limit} 个人口气泡")
    raw = fetch(SOURCES["cities"], CACHE_DIR / "populated-places-50m.json", force)
    src = json.loads(raw)

    places = []
    for f in src["features"]:
        geom = f.get("geometry") or {}
        if geom.get("type") != "Point":
            continue
        props = f.get("properties") or {}
        pop = float(props.get("pop_max") or 0.0)
        name = (props.get("nameascii") or props.get("name") or "").strip()
        if not name or pop <= 0.0:
            continue
        lon, lat = float(geom["coordinates"][0]), float(geom["coordinates"][1])
        if not (-180.0 <= lon <= 180.0 and -85.0 <= lat <= 85.0):
            continue
        places.append((pop, name, lon, lat, (props.get("adm0name") or "").strip()))

    places.sort(key=lambda p: (-p[0], p[1]))
    pop_max = places[0][0] if places else 1.0

    features = []
    seen: dict[str, int] = {}
    for pop, name, lon, lat, country in places:
        if len(features) >= limit:
            break
        # 重名城市（同一个名字不同国家）加国家后缀，保证 set_value 的键唯一。
        key = name
        if key in seen:
            seen[key] += 1
            key = f"{name} ({country})"
        else:
            seen[key] = 0
        r = bubble_radius(pop, pop_max)
        features.append(feature(key, [[circle(lon, lat, r)]], {"pop_max": int(pop)}))
    out = {"type": "FeatureCollection", "features": features}
    write_json(out, OUT_DIR / "cities.json")
    return out


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--what", default="all",
                    choices=["world", "cn", "cities", "all"],
                    help="要抓哪个数据集（默认 all）")
    ap.add_argument("--cities", type=int, default=24,
                    help="cities 数据集保留多少个城市（默认 24）")
    ap.add_argument("--force", action="store_true", help="忽略 build/mapdata 缓存重新下载")
    args = ap.parse_args()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    try:
        if args.what in ("world", "all"):
            build_world(args.force)
        if args.what in ("cn", "all"):
            build_cn(args.force)
        if args.what in ("cities", "all"):
            build_cities(args.force, args.cities)
    except Exception as exc:  # noqa: BLE001 —— CLI 工具，给一句话原因即可
        print(f"failed: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 1

    # 产物立刻过一遍生成物校验：断成"JSON 能解析但经纬度飞了"的数据在这里就拦住，
    # 而不是等到画出一团无法辨认的色块才发现（TopoJSON 差分/反序解码踩过）。
    from gen_geojson_samples import check_one  # noqa: PLC0415

    errors: list[str] = []
    for name in ("world", "cn", "cities"):
        p = OUT_DIR / f"{name}.json"
        if p.is_file():
            errors.extend(check_one(p))
    if errors:
        print("自检失败：", file=sys.stderr)
        for e in errors:
            print(f"  - {e}", file=sys.stderr)
        return 1

    print("done. 自检通过；也可单独跑 python tools/gen_geojson_samples.py --check")
    return 0


if __name__ == "__main__":
    sys.exit(main())
