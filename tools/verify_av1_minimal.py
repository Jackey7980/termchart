#!/usr/bin/env python3
"""AV1 一期门禁：自建 8x8 IVF 经 ffmpeg/libdav1d 解码，逐像素 recon==expected。

一期画面固定为 DC_PRED + 全零残差，因此期望输出是 Y/U/V 全 128。
"""
from __future__ import annotations

import glob
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TMP = ROOT / "build" / "av1_oracle"
PROBE_SRC = ROOT / "tools" / "av1_min_probe.cpp"


def run(cmd: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True)


def main() -> int:
    TMP.mkdir(parents=True, exist_ok=True)
    probe = TMP / "probe.exe"
    cxx = "g++"
    srcs = [str(PROBE_SRC), str(ROOT / "src/image/av1_encode.cpp")]
    srcs += sorted(glob.glob(str(ROOT / "src/image/codec/av1/*/*.cpp")))
    srcs.append(str(ROOT / "src/image/codec/bitstream/ep_nal.cpp"))
    args = [cxx, "-std=c++20", "-O0", "-DTERMCHART_STATIC",
            "-I" + str(ROOT / "include"), "-I" + str(ROOT / "src"),
            "-I" + str(ROOT / "src/image"), "-o", str(probe), *srcs,
            str(ROOT / "build/ninja/libtermchart.a")]
    r = run(args)
    if r.returncode:
        sys.stderr.write(r.stderr.decode(errors="replace")[-2000:])
        return 2

    ivf = TMP / "frame.ivf"
    yuv = TMP / "frame.yuv"
    r = run([str(probe), str(ivf)])
    if r.returncode:
        return 3
    r = run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(ivf),
             "-f", "rawvideo", "-pix_fmt", "yuv420p", "-y", str(yuv)])
    if r.returncode or not yuv.exists():
        sys.stderr.write(r.stderr.decode(errors="replace"))
        return 4
    data = yuv.read_bytes()
    ok = len(data) == 8 * 8 * 3 // 2 and set(data) == {128}
    print(f"AV1 MINIMAL ORACLE: {'PASS' if ok else 'FAIL'} "
          f"bytes={len(data)} unique={sorted(set(data))}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
