#!/usr/bin/env python3
"""HEVC 双向 B 片（GOP=3）真实解码门禁。

编码端按解码序写 I0 P2 B1 P4 B3 ...，POC=显示序号×2；ffmpeg 解码后按 POC
恢复显示序（SPS/VPS 的 max_num_reorder_pics=1）。默认 B 片走 merge（含 combined
bi-pred 双向候选），与 ffmpeg 逐像素一致。本脚本比对「编码端重建（按显示序 dump）」与 ffmpeg 解码输出，
要求逐字节相同；同时用 trace_headers 断言 slice_type 序列真含 B 片。
"""
import argparse
import os
import sys
import tempfile
from pathlib import Path

import verify_hevc_inter as base

W, H = base.W, base.H
NF = 7
SHAPES = ["still", "shift2", "mixed", "round", "cycle2", "noise", "code"]
QPS = [10, 26]
EXPECTED_SLICE_TYPES = [2, 1, 0, 1, 0, 1, 0]  # I0 P2 B1 P4 B3 P6 B5


def encode(probe, ff, tmp, shape, qp, frames=NF, prefix="bs"):
    stem = f"{prefix}_{shape}_q{qp}"
    f265 = tmp / (stem + ".265")
    reco = tmp / (stem + ".r.yuv")
    dec = tmp / (stem + ".d.yuv")
    env = dict(os.environ,
               TERMCHART_HEVC_INTER_BSLICE="1",
               TERMCHART_HEVC_RECO_YUV=str(reco))
    e = base.run([str(probe), str(qp), str(W), str(H), shape, str(f265), str(frames), "1"], env)
    if e.returncode or not f265.exists() or not reco.exists():
        return None, None, f"encode failed: {e.stderr.decode(errors='replace')[:160]}"
    d = base.run([ff, "-y", "-loglevel", "error", "-i", str(f265),
                  "-f", "rawvideo", "-pix_fmt", "yuv420p", str(dec)])
    if d.returncode or not dec.exists():
        return None, None, f"decode failed: {d.stderr.decode(errors='replace')[:160]}"
    r, c = reco.read_bytes(), dec.read_bytes()
    neq = sum(1 for a, b in zip(r, c) if a != b)
    if len(r) != len(c):
        neq += abs(len(r) - len(c))
    return (neq, f265.stat().st_size), None, e.stderr.decode(errors="replace")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--cxx", default=None)
    ap.add_argument("--ffmpeg", default=None)
    args = ap.parse_args()
    ff = args.ffmpeg or base.find_ffmpeg()
    cxx = base.find_cxx(args.cxx)
    if not ff or not cxx:
        print("FAIL: ffmpeg / C++ compiler not found (fail-loud)")
        return 2
    tmp = Path(tempfile.mkdtemp(prefix="termchart_bslice_"))
    probe = tmp / ("probe.exe" if sys.platform == "win32" else "probe")
    if not base.build_probe(cxx, probe):
        print("FAIL: probe build failed")
        return 2

    shapes = ["mixed", "cycle2"] if args.quick else SHAPES
    qps = [26] if args.quick else QPS
    bad = 0
    print(f"MODE: bslice oracle shapes={shapes} qps={qps} size={W}x{H} frames={NF}")
    for shape in shapes:
        for qp in qps:
            res, err, _ = encode(probe, ff, tmp, shape, qp)
            if err:
                print(f"  {shape:8s} QP{qp:<2d} FAIL {err}")
                bad += 1
                continue
            neq, size = res
            print(f"  {shape:8s} QP{qp:<2d} {'OK' if neq == 0 else 'BAD'} neq={neq:6d} bytes={size}")
            bad += neq != 0

    res, err, _ = encode(probe, ff, tmp, "mixed", 26, prefix="trace")
    if err:
        print(f"  trace FAIL {err}")
        bad += 1
    else:
        tr = base.run([ff, "-hide_banner", "-loglevel", "info", "-i",
                       str(tmp / "trace_mixed_q26.265"), "-c:v", "copy",
                       "-bsf:v", "trace_headers", "-f", "null", "-"])
        types = base.field_vals(tr.stderr.decode(errors="replace"), "slice_type")
        ok = types[:NF] == EXPECTED_SLICE_TYPES
        print(f"slice_type sequence: {types[:NF]} -> {'PASS' if ok else 'FAIL'}")
        bad += not ok
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
