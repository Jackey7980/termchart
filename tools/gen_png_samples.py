#!/usr/bin/env python3
"""gen_png_samples.py —— 生成 PNG 解码测试夹具（tests/unit/image/test_png_samples.inc）

为什么需要它：本库自带的 PNG **编码器**只会写"滤波 0 + stored deflate 块"，
如果只用它做往返测试，解码器里的 Huffman 与滤波 1..4 分支等于没被测到。
这里用 Python 的 **zlib（参考实现）** 与手工构造的扫描线造出一批覆盖面更广的
样本：四种滤波器、动态 Huffman、调色板（含 4 位打包与 tRNS）、16 位灰度、
灰度+alpha、以及坏 CRC 的畸形样本。

样本里的像素数据全部按**确定性公式**生成，C++ 测试用同一公式重算期望值，
因此夹具文件里只存 PNG 字节，不存期望像素。

手动运行（产物是测试夹具，不参与 check_generated）：
    python tools/gen_png_samples.py
"""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

# ---- 像素公式（必须与 C++ 测试里的 expected_pixel() 完全一致）----


def rgb_pixel(x: int, y: int) -> tuple[int, int, int]:
    return ((x * 37 + y * 11) % 256, (x * 17 + y * 53) % 256, (x * 97 + y * 7) % 256)


def gray_value(x: int, y: int) -> int:
    return (x * 40 + y * 20) % 256


def gray16_value(x: int, y: int) -> int:
    return (x * 3000 + y * 500) % 65536


# ---- PNG 构造 ----


def chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def build_png(width: int, height: int, bit_depth: int, color_type: int,
              rows: list[bytes], palette: bytes = b"", trns: bytes = b"",
              interlace: int = 0, break_crc: bool = False) -> bytes:
    ihdr = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, interlace)
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
    if palette:
        out += chunk(b"PLTE", palette)
    if trns:
        out += chunk(b"tRNS", trns)
    out += chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
    out += chunk(b"IEND", b"")
    if break_crc:  # 把最后一个字节的 CRC 打坏，用于校验"坏 CRC 必须拒绝"
        out = out[:-1] + bytes([out[-1] ^ 0xFF])
    return out


def filtered_rows(width: int, height: int, channels: int, bit_depth: int, filter_type: int,
                  sample_fn) -> list[bytes]:
    """按指定滤波类型生成扫描线（滤波是**编码前**对原始字节做的差分）。"""
    bpp = max(1, (channels * bit_depth + 7) // 8)
    row_bytes = (width * channels * bit_depth + 7) // 8
    rows: list[bytes] = []
    prev = bytes(row_bytes)
    for y in range(height):
        raw = bytearray()
        if bit_depth == 8:
            for x in range(width):
                values = sample_fn(x, y)
                raw.extend(values)
        elif bit_depth == 16:
            for x in range(width):
                for value in sample_fn(x, y):
                    raw.extend(struct.pack(">H", value))
        else:
            raise AssertionError("仅支持 8/16 位样本")
        assert len(raw) == row_bytes
        filtered = bytearray(row_bytes)
        for i in range(row_bytes):
            left = raw[i - bpp] if i >= bpp else 0
            up = prev[i]
            up_left = prev[i - bpp] if i >= bpp else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = up
            elif filter_type == 3:
                predictor = (left + up) // 2
            elif filter_type == 4:
                p = left + up - up_left
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - up_left)
                predictor = left if (pa <= pb and pa <= pc) else (up if pb <= pc else up_left)
            else:
                raise AssertionError("未知滤波类型")
            filtered[i] = (raw[i] - predictor) & 0xFF
        rows.append(bytes([filter_type]) + bytes(filtered))
        prev = bytes(raw)
    return rows


def palette_rows(width: int, height: int, bit_depth: int, index_fn) -> list[bytes]:
    """调色板图（单通道，可能是 1/2/4/8 位打包，MSB 优先）。"""
    row_bytes = (width * bit_depth + 7) // 8
    rows: list[bytes] = []
    for y in range(height):
        raw = bytearray(row_bytes)
        for x in range(width):
            index = index_fn(x, y)
            if bit_depth == 8:
                raw[x] = index
            else:
                per_byte = 8 // bit_depth
                shift = 8 - bit_depth * (x % per_byte) - bit_depth
                raw[x // per_byte] |= (index & ((1 << bit_depth) - 1)) << shift
        rows.append(bytes([0]) + bytes(raw))
    return rows


def to_cpp(name: str, data: bytes) -> str:
    lines = [f"static const unsigned char {name}[] = {{"]
    for i in range(0, len(data), 12):
        block = ", ".join(f"0x{byte:02X}" for byte in data[i:i + 12])
        lines.append(f"    {block},")
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    samples: list[tuple[str, str, bytes]] = []

    for filter_type, name in [(1, "kSubPng"), (2, "kUpPng"), (3, "kAveragePng"),
                              (4, "kPaethPng")]:
        rows = filtered_rows(5, 3, 3, 8, filter_type, lambda x, y: rgb_pixel(x, y))
        samples.append((name, f"5x3 RGB，全行滤波 {filter_type}", build_png(5, 3, 8, 2, rows)))

    # 24x8 的重复数据 → zlib 会用动态 Huffman（而不是 stored 块）
    rows = filtered_rows(24, 8, 3, 8, 0,
                         lambda x, y: ((x % 4) * 60, (y % 3) * 80, 128))
    samples.append(("kDynamicPng", "24x8 RGB，重复数据（动态 Huffman）",
                    build_png(24, 8, 8, 2, rows)))

    # PLTE 是**交错**的 RGB 三元组（R0G0B0, R1G1B1, ...），不是三段平面存储。
    # 写成平面会让每个索引读到的其实是"别人的红/绿/蓝"——第一版就踩了这个坑。
    palette = bytes(byte for i in range(6)
                    for byte in ((i * 8) & 0xFF, (255 - (i * 8)) & 0xFF, (i * 4) & 0xFF))
    samples.append(("kPalette8Png", "4x4 调色板，8 位索引 + tRNS",
                    build_png(4, 4, 8, 3, palette_rows(4, 4, 8, lambda x, y: (x + y) % 6),
                              palette=palette, trns=bytes([255, 128, 0, 64, 32, 16]))))

    samples.append(("kPalette4Png", "8x2 调色板，4 位索引（同一字节两个像素）",
                    build_png(8, 2, 4, 3, palette_rows(8, 2, 4, lambda x, y: (x + y) % 6),
                              palette=palette)))

    rows = filtered_rows(4, 2, 1, 16, 0, lambda x, y: (gray16_value(x, y),))
    samples.append(("kGray16Png", "4x2 灰度 16 位（取高字节）", build_png(4, 2, 16, 0, rows)))

    rows = filtered_rows(4, 2, 2, 8, 0,
                         lambda x, y: (gray_value(x, y), 255 - gray_value(x, y)))
    samples.append(("kGrayAlphaPng", "4x2 灰度+alpha", build_png(4, 2, 8, 4, rows)))

    rows = filtered_rows(4, 2, 4, 8, 0,
                         lambda x, y: (*rgb_pixel(x, y), (x * 60 + y * 30) % 256))
    samples.append(("kRgbaPng", "4x2 真彩+alpha", build_png(4, 2, 8, 6, rows)))

    rows = filtered_rows(4, 2, 3, 8, 0, lambda x, y: rgb_pixel(x, y))
    samples.append(("kBadCrcPng", "4x2 RGB，CRC 被故意打坏（必须拒绝）",
                    build_png(4, 2, 8, 2, rows, break_crc=True)))

    rows = filtered_rows(4, 2, 3, 8, 0, lambda x, y: rgb_pixel(x, y))
    samples.append(("kInterlacedPng", "4x2 RGB，声明为 Adam7 隔行（应报 Unsupported）",
                    build_png(4, 2, 8, 2, rows, interlace=1)))

    out = [
        "// test_png_samples.inc —— PNG 解码测试夹具（由 tools/gen_png_samples.py 生成，勿手改）",
        "//",
        "// 每个样本都是 Python 的 zlib（参考实现）压缩出来的合法 PNG，覆盖：",
        "//   滤波 1(Sub) / 2(Up) / 3(Average) / 4(Paeth)、动态 Huffman、",
        "//   调色板 8 位与 4 位（含 tRNS）、灰度 16 位、灰度+alpha、真彩+alpha，",
        "//   以及两个畸形样本（坏 CRC、Adam7 隔行）。",
        "// 像素值按 gen_png_samples.py 里的确定性公式生成，C++ 侧用同一公式重算期望值。",
        "",
    ]
    for name, description, data in samples:
        out.append(f"// {description}（{len(data)} 字节）")
        out.append(to_cpp(name, data))
        out.append("")
    path = Path(__file__).resolve().parent.parent / "tests" / "unit" / "image" / "test_png_samples.inc"
    path.write_text("\n".join(out), encoding="utf-8")
    print(f"已写出 {path}（{len(samples)} 个样本，共 "
          f"{sum(len(data) for _, _, data in samples)} 字节 PNG）")


if __name__ == "__main__":
    main()
