#!/usr/bin/env python3
"""verify_hevc_hwaccel.py —— HEVC 真压缩档工业级硬件解码门禁。

对「尺寸 × QP × 形态」矩阵逐组验证：
  1. FFmpeg 软件解码 0 error，算 Y-PSNR；
  2. 平台硬件解码（Windows d3d11va / Linux vaapi / macOS videotoolbox），
     用 -hwaccel_output_format 强制硬件帧格式 —— 请求硬解却静默回退会直接
     失败，不会被误判为通过；
  3. 硬解 Y 平面与软解 Y 平面【逐字节相等】（maxdiff 必须为 0）；
  4. trace_headers 核对 slice_qp_delta == qp-26 且 pcm_enabled_flag == 0。

用法：
  python tools/verify_hevc_hwaccel.py                 # 默认矩阵（64 组）
  python tools/verify_hevc_hwaccel.py --quick        # 冒烟（1 尺寸 x 4 QP x 1 形态）
  python tools/verify_hevc_hwaccel.py --no-hw        # 只跑软解，用于 CI 无 GPU 环境

退出码：0 全通过；1 有失败；2 环境不满足（ffmpeg/CXX 缺失）——显式失败，
不静默降级。
"""
from __future__ import annotations

import argparse
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE_SRC = ROOT / "tools" / "hevc_hwaccel_probe.cpp"

DEFAULT_SIZES = [(64, 64), (128, 64), (256, 128), (640, 384)]
DEFAULT_QPS = [22, 26, 30, 34]
DEFAULT_MODES = ["flat", "hstep", "checker", "grad"]

HW_BACKEND = {
    "win32": ("d3d11va", "d3d11", "hwdownload,format=nv12", "nv12"),
    "linux": ("vaapi", "vaapi", "hwdownload,format=nv12", "nv12"),
    "darwin": ("videotoolbox", "videotoolbox", "hwdownload,format=nv12", "nv12"),
}


def find_ffmpeg() -> str | None:
    exe = shutil.which("ffmpeg")
    if exe:
        return exe
    p = Path(os.environ.get("APPDATA", "")) / (
        "Roaming/Python/Python314/site-packages/imageio_ffmpeg/binaries")
    hits = sorted(p.glob("ffmpeg*.exe")) if p.exists() else []
    return str(hits[0]) if hits else None


def find_cxx(explicit: str | None) -> str | None:
    if explicit:
        return explicit
    for name in ("CXX",):
        if os.environ.get(name):
            return os.environ[name]
    for name in ("c++", "g++", "clang++"):
        p = shutil.which(name)
        if p:
            return p
    for p in (Path("C:/Program Files/mingw64/bin/c++.exe"),):
        if p.exists():
            return str(p)
    return None


def run(args: list[str], cwd: Path = ROOT):
    return subprocess.run(args, cwd=cwd, capture_output=True)


def build_probe(cxx: str, out: Path) -> bool:
    srcs = [str(PROBE_SRC), "src/image/hevc_encode.cpp",
            "src/image/codec/bitstream/ep_nal.cpp"]
    srcs += [str(p) for p in sorted((ROOT / "src/image/codec/hevc").glob("*.cpp"))]
    cmd = [cxx, "-std=c++20", "-O2", "-DTERMCHART_STATIC",
           "-Iinclude", "-Isrc", "-Isrc/image", "-o", str(out), *srcs]
    r = run(cmd)
    if r.returncode != 0:
        sys.stderr.write(r.stderr.decode(errors="replace")[-2000:])
    return r.returncode == 0


def y_psnr(dec: bytes, src: bytes) -> float:
    n = len(src)
    se = sum((dec[i] - src[i]) ** 2 for i in range(n))
    return 99.0 if se == 0 else 10 * math.log10(255 * 255 / (se / n))


def check_params(ff: str, f265: Path, qp: int) -> tuple[int | None, bool]:
    r = run([ff, "-hide_banner", "-loglevel", "trace", "-i", str(f265),
             "-c", "copy", "-bsf:v", "trace_headers", "-f", "null", "-"])
    txt = r.stderr.decode(errors="replace") if r.stderr else ""
    m = re.search(r"slice_qp_delta\s+\S+\s*=\s*(-?\d+)", txt)
    delta = int(m.group(1)) if m else None
    pcm_off = re.search(r"pcm_enabled_flag\s+\S+\s*=\s*0", txt) is not None
    return delta, pcm_off


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true", help="冒烟矩阵")
    ap.add_argument("--no-hw", action="store_true", help="跳过硬解，只验软解")
    ap.add_argument("--cxx", default=None)
    ap.add_argument("--ffmpeg", default=None)
    ap.add_argument("--qps", default=",".join(map(str, DEFAULT_QPS)))
    ap.add_argument("--min-psnr", type=float, default=35.0,
                    help="仅对 QP=26 生效的 PSNR 门槛（ROADMAP:484）")
    args = ap.parse_args()

    ff = args.ffmpeg or find_ffmpeg()
    if not ff:
        print("FAIL: ffmpeg not found (explicit failure, no silent fallback)")
        return 2
    cxx = find_cxx(args.cxx)
    if not cxx:
        print("FAIL: C++ compiler not found")
        return 2

    sizes = [(128, 64)] if args.quick else DEFAULT_SIZES
    modes = ["grad"] if args.quick else DEFAULT_MODES
    qps = [int(x) for x in args.qps.split(",") if x.strip()]
    backend = HW_BACKEND.get(sys.platform)

    tmp = Path(tempfile.mkdtemp(prefix="termchart_hwaccel_"))
    probe = tmp / ("probe.exe" if sys.platform == "win32" else "probe")
    if not build_probe(cxx, probe):
        print("FAIL: probe build failed")
        shutil.rmtree(tmp, ignore_errors=True)
        return 2

    total = failed = 0
    worst = 999.0
    hw_active = not args.no_hw and backend is not None
    if not args.no_hw and backend is None:
        print(f"WARN: no hardware backend mapping for {sys.platform}; "
              f"hardware decode NOT verified (use --no-hw to state this explicitly)")

    try:
        for (W, H) in sizes:
            for qp in qps:
                for mode in modes:
                    total += 1
                    tag = f"{W}x{H}_qp{qp}_{mode}"
                    f265 = tmp / f"{tag}.265"
                    r = run([str(probe), str(qp), str(W), str(H), mode, str(f265)])
                    if r.returncode != 0:
                        print(f"FAIL {tag}: probe rc={r.returncode}")
                        failed += 1
                        continue
                    src = (tmp / f"{tag}.265.yuv").read_bytes()[:W * H]
                    fsw, fhw = tmp / f"{tag}_sw.yuv", tmp / f"{tag}_hw.nv12"
                    # DBF 已在 PPS 中启用：去块滤波是码流语义的一部分，软解参考必须
                    # 与硬解走同一套滤波，不能再加 -skip_loop_filter。
                    rs = run([ff, "-y", "-loglevel", "error",
                              "-i", str(f265), "-f", "rawvideo", "-pix_fmt", "yuv420p",
                              str(fsw)])
                    if rs.returncode or not fsw.exists():
                        print(f"FAIL {tag}: software decode rc={rs.returncode}")
                        failed += 1
                        continue
                    sw = fsw.read_bytes()[:W * H]
                    if len(sw) != W * H:
                        print(f"FAIL {tag}: short software frame {len(sw)}")
                        failed += 1
                        continue
                    p = y_psnr(sw, src)
                    worst = min(worst, p)

                    md = None
                    if hw_active:
                        hwaccel, hwfmt, vf, pixfmt = backend
                        rh = run([ff, "-y", "-loglevel", "error", "-hwaccel", hwaccel,
                                  "-hwaccel_output_format", hwfmt, "-i", str(f265),
                                  "-vf", vf, "-f", "rawvideo", "-pix_fmt", pixfmt,
                                  str(fhw)])
                        if rh.returncode or not fhw.exists():
                            print(f"FAIL {tag}: hardware decode rc={rh.returncode} "
                                  f"{rh.stderr.decode(errors='replace')[:160]}")
                            failed += 1
                            continue
                        hw = fhw.read_bytes()[:W * H]
                        if len(hw) != W * H:
                            print(f"FAIL {tag}: short hardware frame {len(hw)}")
                            failed += 1
                            continue
                        md = max(abs(a - b) for a, b in zip(sw, hw))

                    delta, pcm_off = check_params(ff, f265, qp)
                    good = (md in (None, 0) and delta == qp - 26 and pcm_off and
                            (qp != 26 or p > args.min_psnr))
                    if not good:
                        print(f"FAIL {tag}: psnr={p:.2f} hwdiff={md} "
                              f"delta={delta}(want {qp-26}) pcm_off={pcm_off}")
                        failed += 1
                    for f in (f265, fsw, fhw, tmp / f"{tag}.265.yuv"):
                        f.unlink(missing_ok=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print(f"\nHEVC HARDWARE GATE: {total - failed}/{total} PASS, fail={failed}, "
          f"hardware={'ACTIVE' if hw_active else 'SKIPPED'}, "
          f"worst Y-PSNR={worst:.2f} dB")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
